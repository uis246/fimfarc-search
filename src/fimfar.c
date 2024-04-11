#include "fimfar.h"

#include <argp.h>

#define STATE_REPORT 0

static struct argp_option namer_options[] = {
	{ "tag", 't', 0, 0, "Return tag by id", 0 },
	{ "name", 'n', 0, 0, "Return story name by id", 0 },
	#if STORE_PATH
	{ "path", 'p', 0, 0, "Return path of story id", 0 },
	#endif
	{ "unname", 'u', 0, 0, "Uno-reverso. Return id by nane", 0 },
	{ "exact", 'e', 0, 0, "Exact match", 1 },
	{ "case", 'C', 0, 0, "Case-insensetive", 1 },
	{ "in", 'i', 0, 0, "Input file", 1 },
	{ 0 }
}, searcher_options[] = {
	{ "list", 'l', "file", 0, "Ids of stories", 0 },
	{ "archive", 'a', "file", 0, "Archive file", 0 },
	//{ "text", 't', "pattern", 0, "Pattern to search", 0 },
	{ "out", 'o', "file", 0, "Output file", 0 },
	{ "op", 'p', "operation", 0, "Logical operation", 1 },
	{ "case", 'C', 0, 0, "Case-insensetive", 1 },
	{ 0 }
};

static char doc[] = "Fimfiction archive searching tools";
static char args_doc[] = "ID/PATTERN",
searcher_doc[] = "PATTERN";

static struct {
	enum {
		None = 0,
		Builder,//Builds db for tags and names
		Namer,//Get name from id and vice versa
		Selector,//By tags
		Searcher,//In text
		Merger,//And, or
		Loader,//Just loads from text to bin list
		Multisearch,//Do multiple searches in text in parallel
		Arcstat,
	} mode;
	union{
		struct {
			char *text;
			enum {
				Error = 0,
				Path,
				Tag,
				Name
			} type;
			bool unname, file, exact, sens;
		} namer;
		struct {
			enum LogOp mode;
		} merger;
	};
} args={None, {{0}}};

static error_t parse_opt(int key, char *arg, struct argp_state *) {
switch (key) {
	#if STORE_PATH
	case 'p': 
		args.namer.type = Path;
		break;
	#endif
	case 't':
		args.namer.type = Tag;
		break;
	case 'n':
		args.namer.type = Name;
		break;
	case 'u':
		args.namer.unname = true;
		break;
	case 'i':
		args.namer.file = true;
		break;
	case 'e':
		args.namer.exact = true;
		break;
	case 'C':
		args.namer.sens = true;
		break;
	case ARGP_KEY_ARG:
		args.namer.text = arg;
		break;
        default: return ARGP_ERR_UNKNOWN;
}
return 0;
}
error_t search_parse_opt(int key, char *arg, struct argp_state*);

static struct argp namer_argp = { namer_options, parse_opt, args_doc, doc, 0, 0, 0 },
search_argp = { searcher_options, search_parse_opt, searcher_doc, doc, 0, 0, 0 };


void bufappend(struct stringbuf *buf, const void *data, const size_t size) {
	//Check buffer size
	if(buf->size < buf->length + size) {
		//Reallocate if needed
		buf->size += 4096;
		buf->data = realloc(buf->data, buf->size);
	}

	//Do copy
	memcpy(buf->data + buf->length, data, size);
	//Mark space as used
	buf->length += size;
}

static void selecter(FILE *out, const void *assocs, size_t assoc_size, unsigned int tag) {
	assoc_size /= STF_SIZE;
	//TODO: optional input
	for(size_t i = 0; i < assoc_size; i++) {
		const struct story_tag_file *stf = assocs;
		if(stf->tag_id == tag) {
			//Write id
			fwrite(&stf->story_id, sizeof(stf->story_id), 1, out);
		}
		assocs += STF_SIZE;
	}
}

static void name(const void *in, const size_t size, uint32_t *ids, size_t count) {
	size_t found = 0;
	count /= sizeof(uint32_t);
	if (!ids) {
		ids = alloca(sizeof(uint32_t));
		ids[0] = strtoul(args.namer.text, NULL, 10);
		count = 1;
	}
	for(size_t off = 0; off < size && found < count;) {
		const struct id_text_file *itf = in + off;
		//Ids are sorted, so skip if not found
		//But I don't think this will happen
		//Made just in case
		for (; itf->id > ids[found]; ++found)
			if (found == count)
				return;
		if (itf->id == ids[found]) {
			switch(args.namer.type) {
				case Name:
					printf("%"PRIu32":\t", itf->id);
					// fall-through
				case Tag:
					printf("%.*s\n", itf->length, itf->data);
					break;
				#if STORE_PATH
				case Path: {
					//Path printing is kinda messy
					size_t len = strlen(itf->data) + 1;
					printf("%.*s\n", itf->length - len, itf->data + len);
					break;
				}
				#endif
			}
			found++;
		}
		off += ITF_SIZE + itf->length;
	}
}
static void unname(const void *in, size_t size) {
	size_t text_len = strlen(args.namer.text);
	#if STORE_PATH
	if (args.namer.type == Path) {
		dprintf(2, "Don't unname path, idiot!\n");
		return;
	}
	#endif
	for(size_t off = 0; off < size;) {
		const struct id_text_file *itf = in + off;
		int ret;
		//TODO: implement not-exact case-insensetive
		if (args.namer.exact && !args.namer.sens)
			ret = strncmp(itf->data, args.namer.text, itf->length);
		else if (args.namer.exact /*&& args.namer.sens*/)
			ret = strncasecmp(itf->data, args.namer.text, itf->length);
		else
			ret = memmem(itf->data, itf->length, args.namer.text, text_len) == NULL;
		if (ret == 0) {
			printf("%"PRIu32": %.*s\n", f32toh(itf->id), itf->length, itf->data);
			if (args.namer.exact)
				return;
		}
		off += ITF_SIZE + itf->length;
	}
}

size_t readfile(const char *restrict path, void *restrict*ptr) {
	FILE *f;
	void *buf;
	size_t size;
	//Open
	f = fopen(path, "rb");
	if (f == NULL) {
		dprintf(2, "Cannot open file %s\n", path);
		*ptr = NULL;
		return 0;
	}
	//Get length
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	//Read
	buf = malloc(size);
	if (buf == NULL) {
		dprintf(2, "Cannot allocate memory for %s in %zu bytes\n", path, size);
		size = 0;
	} else {
		fread(buf, 1, size, f);
	}
	//Close
	fclose(f);
	*ptr = buf;
	return size;
}

int id_sort(const void *a, const void *b) {
	const uint32_t *A = a, *B = b;
	return *A - *B;
}

//FIXME: Use fixed endian
int main(int argc, char* argv[])
{
	if (argc < 2) {
		dprintf(2, "Select one tool:\n"
		"\tbuild\n\tname\n\tselect\n\tsearch\n\tmerge\n\tload\n\tmake\n\tarcstat\n");
		return -1;
	}

	if (strcmp(argv[1], "build") == 0)
		args.mode = Builder;
	else if (strcmp(argv[1], "name") == 0)
		args.mode = Namer;
	else if (strcmp(argv[1], "select") == 0)
		args.mode = Selector;
	else if (strcmp(argv[1], "search") == 0)
		args.mode = Searcher;
	else if (strcmp(argv[1], "merge") == 0)
		args.mode = Merger;
	else if (strcmp(argv[1], "load") == 0)
		args.mode = Loader;
	else if (strcmp(argv[1], "make") == 0)
		args.mode = Multisearch;
	else if (strcmp(argv[1], "arcstat") == 0)
		args.mode = Arcstat;
	else {
		dprintf(2, "Tool %s not found\n", argv[1]);
		return -1;
	}

	if (args.mode == Builder) {
		builder();
	} else if (args.mode == Searcher) {
		argp_parse(&search_argp, argc - 1, argv + 1, 0, 0, 0);
		search();
	} else if (args.mode == Namer) {
		void *in, *list = NULL;
		size_t in_size, list_size = 0;
		argp_parse(&namer_argp, argc - 1, argv + 1, 0, 0, 0/*&args*/);
		if (args.namer.type == Error) {
			dprintf(2, "Select mode\n");
			return -1;
		} else if (args.namer.type == Tag)
			in_size = readfile(TAG_PATH, &in);
		else
			in_size = readfile(STORY_PATH, &in);
		if (args.namer.file) {
			list_size = readfile(args.namer.text, &list);
			if (list == NULL) {
				return -2;
			}
		}
		if (args.namer.unname)
			unname(in, in_size);
		else {
			name(in, in_size, list, list_size);
		}
		free(list);
		free(in);
	} else if (args.mode == Selector) {
		if (argc != 4) {
			dprintf(2, "Usage: %s select OUTFILE TAG\n", argv[0]);
			return -1;
		}
		//Open files
		void *assoc;
		FILE *out;
		size_t assocs;
		unsigned int tag;
		assocs = readfile(ASSOC_PATH, &assoc);
		//Open writer
		out = fopen(argv[2], "wb");
		tag = strtoul(argv[3], NULL, 10);
		selecter(out, assoc, assocs, tag);
		free(assoc);
		fclose(out);
	} else if (args.mode == Merger) {
		struct stringbuf stories = {0}, a_buf;
		if (argc < 6) {
			dprintf(2, "Usage: %s merge OPERATION OUTFILE INFILE1 INFILE2...\n"
			"Operatins:\nand\nor\nremove\n", argv[0]);
			return -1;
		}
		if (strcmp(argv[2], "or") == 0) {
			args.merger.mode = OR;
		} else if (strcmp(argv[2], "and") == 0) {
			args.merger.mode = AND;
		} else if (strcmp(argv[2], "remove") == 0) {
			args.merger.mode = REMOVE;
		} else {
			dprintf(2, "Operation %s not found\n", argv[2]);
			return -1;
		}

		uint32_t *in[2];
		size_t size[2];
		a_buf.length = readfile(argv[4], (void**)&a_buf.data);
		a_buf.size = a_buf.length;

		//I think result will be sorted anyway
		for (int number = 5; number < argc; number++) {
		in[0] = (uint32_t*)a_buf.data;
		size[0] = a_buf.length / sizeof(**in);
		size[1] = readfile(argv[number], (void**)&in[1]) / sizeof(**in);
		for (size_t a = 0, b = 0; a < size[0]; a++) {
			for (; b < size[1] && in[0][a] > in[1][b]; b++)
				if (args.merger.mode == OR)
					bufappend(&stories, &in[1][b], sizeof(**in));

			if (b == size[1]) {
				if (args.merger.mode == OR || args.merger.mode == REMOVE)
					bufappend(&stories, &in[0][a], (size[0] - a) * sizeof(**in));
				break;
			}
			if (in[0][a] == in[1][b]) {
				//DO IT.
				if (args.merger.mode == AND)
					//Add
					bufappend(&stories, &in[0][a], sizeof(**in));
				else if (args.merger.mode == OR) {
					//Add
					bufappend(&stories, &in[0][a], sizeof(**in));
					b++;
				}
				//Do not add if want removal
			} else {
				//Not found in b
				if (args.merger.mode == REMOVE || args.merger.mode == OR)
					//Add
					bufappend(&stories, &in[0][a], sizeof(**in));
			}
		}
		free(a_buf.data);
		a_buf = stories;
		stories = (struct stringbuf){0};
		free(in[1]);
		in[1] = NULL;//For sure
		size[0] = 0;
		size[1] = 0;
		}

		FILE *out = fopen(argv[3], "wb");
		fwrite(a_buf.data, a_buf.length/sizeof(**in), sizeof(**in), out);
		free(a_buf.data);//Same as free(in[1]);
		fclose(out);
	} else if (args.mode == Loader) {
		if (argc != 4) {
			dprintf(2, "Usage: %s load OUTFILE LIST\n", argv[0]);
			return -1;
		}
		struct stringbuf stories = {0};
		FILE *out;
		void *in;
		size_t in_s;
		in_s = readfile(argv[3], &in);
		//FIXME: current implementation is unsafe
		for (char *in_c = in, *num = in/*GCC said it cound be noninited. How?*/; in_c && (void*)num < in + in_s; num = strsep(&in_c, "\n ")) {
			char *endptr;
			uint32_t id;
			//Check is it number
			id = strtoul(num, &endptr, 10);
			if (errno == ERANGE || *endptr != 0 || num == endptr)
				continue;//Shit found
			//
			bufappend(&stories, &id, sizeof(id));
		}
		free(in);
		//Sort
		qsort(stories.data, stories.length/sizeof(uint32_t), sizeof(uint32_t), id_sort);
		printf("Found %zu ids\n", stories.length/sizeof(uint32_t));
		//Write
		out = fopen(argv[2], "wb");
		fwrite(stories.data, sizeof(uint32_t), stories.length/sizeof(uint32_t), out);
		fclose(out);
		free(stories.data);
	} else if(args.mode == Multisearch) {
		if(argc != 4) {
			printf("Usage: %s make ARCHIVE MSFILE\n", argv[0]);
			return -1;
		}
		multisearch(argv[2], argv[3]);
	} else if(args.mode == Arcstat) {
		if(argc != 3) {
			printf("Usage: %s arcstat ARCHIVE\n", argv[0]);
			return -1;
		}
		arcstat(argv[2]);
	} else {
		dprintf(2, "Tool %s is not implemented\n", argv[1]);
		return -2;
	}
	return 0;
}
