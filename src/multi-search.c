//Multi-search format
//SSA
//Makefile-like
//
//Search for Faust case-sensetive and save to searchtags/Faust
//"searchtags/Faust": 'Faust'
//
//Search for Faust case-insensetive and do not save to file
//faust: "Faust"
//
//ingram "ingram": 123 & "Ingram"
//^       ^        ^      ^
//|       |        |      Search term
//|       |        Tag id
//|       File name
//Target name
//
//<target> <path>: <tag/target> [logop] term/tag/target
//
//Load target from file
//mare = "searchtags/mare"
//or
//mare "searchtags/mare" =
//
//Every file should not be mentioned more than once
//Every target should not be mentioned more than once
//
//Simplified MSF:
//<target> <path>: <target> [logop] term

#include "fimfar.h"

#include <minizip/unzip.h>

#include <stdio.h>

extern char *archive; //Path to archive
int getNextEpub(unzFile archive, unz_file_info *epub_info, char *fname);
uint32_t getHtmlCount(const void *data, size_t size);

struct target {
	struct stringbuf buf;
	struct stringview name, path;
	bool read_only;//Means loaded from disk
};

struct opcode {
	//struct stringview text; //Pattern to search
	const char *text; //Pattern to search
	const struct target *input; //List of ids for logic operation
	struct target *output; //Output list of ids
	enum LogOp mode;
	uint8_t depth;//Dependency depth
	//TODO: add max depth check
	bool sens; //Case-sensetiveness
};

#define REPORT_TARGETS 0
#if REPORT_TARGETS
static const char *logops[] = {"AND", "OR", "REMOVE"};
#endif

struct tchunk {
	struct target targets[64];
	struct tchunk *next;
};

struct ochunk {
	struct opcode opcodes[64];
	struct ochunk *next;
};

static size_t skipSpaces(const char *in, size_t left) {
	size_t skip = 0;
	for(; skip < left; skip++) {
		if(in[skip] == ' ' || in[skip] == '\t')
			continue;
		return skip;
	}
	return 0;
}

inline static bool cmpstrview(const struct stringview *restrict a, const struct stringview *restrict b) {
	if(a->length != b->length)
		return false;
	return memcmp(a->data, b->data, a->length) == 0;
}

inline static bool bufSearch(const struct stringbuf *buf, const uint32_t id, const bool fast) {
	uint32_t *list = (uint32_t*)buf->data;
	size_t count = buf->length / sizeof(uint32_t);
	if(fast) {
		for (size_t i = 0; i < count; i++) {
			if(list[i] < id)
				continue;
			else if(list[i] == id)
				return true;
			else
				break;
		}
		return false;
	} else {
		/*for (size_t i = 0; i < len; i++)
			if (list[i] == id)
				return true;*/
		//Here's optimization hack:
		//Check only last entry
		if(count)
			return list[count - 1] == id;
		return false;
	}
}

#define chkEOF() {if(unlikely(pos >= insize)) {fprintf(stderr, "Unexpected EOF\n"); exit(-1);}}
#define chkEOL(n) {if(unlikely(indata[pos + n] == '\n')) {fprintf(stderr, "Unexpected EOL\n"); exit(-1);}}
#define chkExists(name)
#define chkNotExists(name) !chkExists(name)
//FIXME: check for null
#define saveTarget(t) &current->targets[target_count%64]; {current->targets[target_count%64] = t; target_count++; if(!(target_count%64)) {current->next = malloc(sizeof(struct tchunk)); current = current->next; current->next = NULL;}}
#define searchTarget(output, tarname) {output = NULL; struct tchunk *cur = &root; if(tarname.length)for(size_t i = 0; i < target_count;) {\
		if(cmpstrview(&tarname, &cur->targets[i%64].name)){ output = &cur->targets[i%64]; break;}\
		i++; if(!(i%64)) cur = cur->next;\
	}}
//FIXME: check for null
#define saveOpcode(t) {o_current->opcodes[opcode_count%64] = t; opcode_count++; if(!(opcode_count%64)) {o_current->next = malloc(sizeof(struct ochunk)); o_current = o_current->next; o_current->next = NULL;}}
//FIXME: search appears to be broken
/*#define searchOpcode(output, targetptr) {struct ochunk *cur = &o_root; for(size_t i = 0; i < opcode_count;) {\
		if(cur->opcodes[i%64].output == targetptr){ output = &cur->targets[i%64]; break;}\
	}}*/
#define searchInput(inTarget, storyid) bufSearch(&inTarget->buf, storyid, inTarget->read_only)
#define addOutput(outTarget, storyid) {if(outTarget->read_only){dprintf(2, "Attempting to write read-only target\n"); abort();} bufappend(&outTarget->buf, &storyid, sizeof(storyid));}

static int dep_sort(const void *a, const void *b) {
	const struct opcode **A = (const struct opcode **)a, **B = (const struct opcode **)b;
	return ((int)(*A)->depth) - (int)(*B)->depth;
}

//static void save_results(struct opcode **BigOpcodeList, size_t opcode_count, const char *restrict statout) {}

void multisearch(const char *restrict arch, const char *restrict infile) {
	//Load infile
	char *indata;
	size_t insize = readfile(infile, (void**)&indata), pos = 0;
	if(insize == 0)
		return;
	size_t target_count = 0, opcode_count = 0;
	struct tchunk root;
	struct ochunk o_root;
	struct tchunk *current = &root;
	struct ochunk *o_current = &o_root;
	root.next = NULL;
	//Parse infile
	while(pos < insize) {
	/*
	 * 	skip spaces
	 * 	if not '\n' and not '"'
	 * 		read target
	 *
	 * 	skip spaces
	 *	if '"'
	 *		read path
	 *	else
	 *		skip line
	 *
	 * 	skip spaces
	 * 	if ':'
	 * 		do search
	 *	else if '='
	 *		do load
	 *	else
	 *		error
	 *
	 *	save target
	 */
		if(pos == insize)
			break;
		if(indata[pos] == '\n') {
			pos++;
			continue;
		}
		if(indata[pos] == '#') {
			//skip line
			for(; pos<insize; pos++)
				if(indata[pos] == '\n') {
					pos++;
					break;
				}
			continue;
		}
		struct target ot = {0};
		if (indata[pos] != '\n' && indata[pos] != '"') {
			//target name
			//TODO: It cannot start with '\'' or contain space or ':' or only digits
			for(size_t i = 0; i < insize-pos; i++) {
				//FIXME: unicode
				//chkEOF();//EOF is impossible here
				chkEOL(i);
				//Skip until space
				char c = indata[pos + i];
				if(c != ' ' && c != '\t' && c != ':' && c != '=')
					continue;
				//save shit
				ot.name.data = indata + pos;
				ot.name.length = i;
				pos += i;
				break;
			}
			//TODO: check if already exists
	 		pos += skipSpaces(indata + pos, insize - pos);
		}
		chkEOF();
		if(indata[pos] == '"') {
			//path
			pos++;
			//TODO: check for relative paths
			for(size_t i = 0; i < insize-pos; i++) {
				//FIXME: unicode
				//chkEOF();
				chkEOL(i);
				//Skip until quotes
				if(indata[pos + i] != '"')
					continue;
				//save shit
				ot.path.data = indata + pos;
				ot.path.length = i;
				pos += i + 1;
				break;
			}
	 		pos += skipSpaces(indata + pos, insize - pos);
		}
		chkEOF();
		if(indata[pos] == ':') {
			ot.read_only = false;
			//Prepare opcode
			pos++;
	 		pos += skipSpaces(indata + pos, insize - pos);
			struct opcode code;
			chkEOF();
			char c = indata[pos];
			code.input = NULL;
			code.depth = 0;
			if(c != '&' && c != '|' && c != '~' && c != '"' && c != '\'') {
				//input target
				struct stringview tar_in;
				tar_in.data = &indata[pos];
				tar_in.length = 0;
				for(size_t i = 0; i < insize-pos; i++) {
					//FIXME: unicode
					//chkEOF();//EOF is impossible here
					chkEOL(i);
					//Skip until space
					if(indata[pos + i] != ' ' && indata[pos + i] != '\t')
						continue;
					//save shit
					tar_in.length = i;
					pos += i;
					break;
				}
	 			pos += skipSpaces(indata + pos, insize - pos);
				chkEOF();
				c = indata[pos];
				//Search for target
				searchTarget(code.input, tar_in);
				if(!code.input) {
					//Target not found
					abort();
				}
			}
			if(c == '&' || c == '|' || c == '~') {
				//logop
				if(c == '&') {
					if(!code.input) {
						fprintf(stderr, "Logical and without another input at offset %zu\n", pos);
						exit(-1);
					}
					code.mode = AND;
				} else if(c == '|')
					code.mode = OR;
				else if(c == '~')
					code.mode = REMOVE;
				else {
					fprintf(stderr, "Bad logic operator \"%c\" at %zu\n", c, pos);
					exit(-1);
				}

				pos++;
	 			pos += skipSpaces(indata + pos, insize - pos);
				chkEOF();
				c = indata[pos];
			} else
				code.mode = code.input ? AND : OR;
			if(c == '"' || c == '\'') {
				//search term
				//TODO: sens is inverted
				code.sens = c != '\'';
				pos++;
				chkEOF();
				for(size_t i = 0; i < insize-pos; i++) {
					//FIXME: unicode
					//chkEOF();
					chkEOL(i);
					//Skip until quotes
					if(indata[pos + i] != c)
						continue;
					//save shit
					code.text = strndup(indata + pos, i);
					pos += i + 1;
					break;
				}
	 			pos += skipSpaces(indata + pos, insize - pos);
			} else
				fprintf(stderr, "Search term not found at offset %zu\n", pos);
			//Save target
			code.output = saveTarget(ot);
			//Save opcode
			saveOpcode(code);
		} else if (indata[pos] == '=') {
			//Load file
			ot.read_only = true;
			pos++;
			if(!ot.path.data) {
				//Load path
				//TODO
				abort();
			}
			//Read file
			//NOTE: careful with allocas
			char *path = alloca(ot.path.length + 1);
			memcpy(path, ot.path.data, ot.path.length);
			path[ot.path.length] = 0;
			ot.buf.length = ot.buf.size = readfile(path, (void**)&ot.buf.data);
			if(!ot.buf.data)
				exit(-1);
			//Save target
			(void)saveTarget(ot);
		} else {
			fprintf(stderr, "Unexpected symbol at offset %zu\n", pos);
			exit(-1);
		}
	 	pos += skipSpaces(indata + pos, insize - pos);
	}
	#if REPORT_TARGETS
	current = &root;
	for(size_t i = 0; i < target_count;) {
		struct target *t = &current->targets[i%64];
		printf("%zu:\t", i);
		if(t->name.data)
			printf("%.*s ", t->name.length, t->name.data);
		if(t->path.data)
			printf("\"%.*s\"", t->path.length, t->path.data);
		if(!t->read_only) {
			printf(":");
		} else
			printf("=");
		printf("\n");
		i++;
		if(!(i%64))
			current = current->next;
	}
	#endif
	//Create waves for batching
	struct opcode **BigOpcodeList;
	struct waveinfo {
		size_t amount;
		struct opcode **op;//Array of pointers
		struct checkRq *rqs;//Array of structs
	} *waves;
	BigOpcodeList = malloc(opcode_count * sizeof(struct opcode*));
	if(BigOpcodeList == NULL) {
		exit(-1);
		dprintf(2, "Failed to allocate memory\n");
		goto freeops;
	}
	o_current = &o_root;
	size_t wave_count = 0;
	for(size_t i = 0; i < opcode_count;) {
		struct opcode *o = &o_current->opcodes[i%64];

		struct ochunk *cur_dep = &o_root;
		for(size_t j = 0; j < i;) {
			if(cur_dep->opcodes[j%64].output == o->input){
				//set dependency depth
				o->depth = cur_dep->opcodes[j%64].depth + 1;
				wave_count = o->depth > wave_count ? o->depth : wave_count;
				break;
			}
			j++;
			if(!(j%64))
				cur_dep = cur_dep->next;
		}
		#if REPORT_TARGETS
		printf("%zu:\t", i);
		if(o->input)
			printf("%.*s %s ", o->input->name.length, o->input->name.data, logops[o->mode]);
		if(!o->sens)
			printf("'%s' ", o->text);
		else
			printf("\"%s\" ", o->text);
		printf("%u\n", o->depth);
		#endif

		BigOpcodeList[i] = o;
		i++;
		if(!(i%64))
			o_current = o_current->next;
	}
	wave_count++;
	waves = malloc(wave_count * sizeof(*waves));
	if(waves == NULL) {
		free(BigOpcodeList);
		exit(-1);
		dprintf(2, "Failed to allocate memory\n");
		goto freeops;
	}
	//Sort by depth
	qsort(BigOpcodeList, opcode_count, sizeof(*BigOpcodeList), dep_sort);
	//Create waves
	for(size_t i = 0; i < opcode_count;) {
		size_t added = 0;
		struct opcode **tis = BigOpcodeList + i;
		uint8_t depth = (*tis)->depth;
		for(size_t j = i; j < opcode_count; j++) {
			if(depth == (*(BigOpcodeList + j))->depth)
				added++;
		}
		waves[depth].amount = added;
		waves[depth].op = tis;
		waves[depth].rqs = malloc(added * sizeof(struct checkRq));
		if(waves[depth].rqs == NULL) {
			free(BigOpcodeList);
			for(size_t j = 0; j < depth; j++)
				free(waves[j].rqs);
			free(waves);
			dprintf(2, "Failed to allocate memory\n");
			goto freeops;
		}
		for(size_t j = 0; j < added; j++) {
			waves[depth].rqs[j].text = (*(tis + j))->text;
			waves[depth].rqs[j].sens = (*(tis + j))->sens;
		}
		i += added;
	}
	for(size_t i = 0; i < wave_count; i++)
		printf("Wave %zu with %zu\n", i, waves[i].amount);
	
	//Open archive
	unzFile *archive;
	//NOTE: no file mapping becase I have 32-bit systems
	archive = unzOpen64(arch);
	if (!archive) {
		dprintf(2, "Failed to open archive file %s\n", arch);
		goto freetree;
	}
	unz_global_info info;
	if (unzGetGlobalInfo(archive, &info) != UNZ_OK) {
		dprintf(2, "Failed to get archive global info\n");
		goto freetree;
	}
	//Iterate over epub
	char *fname = alloca(4096);
	int ret = UNZ_OK;
	while (ret == UNZ_OK) {
		unz_file_info file_info;
		ret = getNextEpub(archive, &file_info, fname);
		if(ret != 1)
			break;

		//Check in list
		uint32_t id, htmls;
		id = strtoul(strrchr(fname, '-') + 1, NULL, 10);

		void *buf = NULL;
		//Iterate over waves
		for(size_t j = 0; j < wave_count; j++) {
			struct waveinfo *wi = &waves[j];

			//Do logic here
			for(size_t k = 0; k < wi->amount; k++) {
				bool found = false;
				struct opcode *o = *(wi->op + k);
				if(o->input)
					found = searchInput(o->input, id);
				if(found && o->mode == OR) {
					//add
					addOutput(o->output, id);
					wi->rqs[k].ret = 2;//2 means skip
				} else if(!found && o->mode != OR) {
					//skip
					wi->rqs[k].ret = 2;
				} else
					wi->rqs[k].ret = 0;
			}
			bool skip = true;
			for(size_t k = 0; k < wi->amount; k++)
				if(wi->rqs[k].ret == 0) {
					skip = false;
					break;
				}
			if(skip)
				continue;
			//search text
			//Read file if needed
			if(!buf) {
				buf = malloc(file_info.uncompressed_size);
				if (!buf) {
					dprintf(2, "Failed to allocate memory\n");
					exit(-1);
				}
				ret = unzOpenCurrentFile(archive);
				if (ret != UNZ_OK) {
					dprintf(2, "Failed to open file from archive, terminating\n");
					abort();
				} else {
					//Read and search
					ret = unzReadCurrentFile(archive, buf, file_info.uncompressed_size);
					if (ret < 0) {
						//Error
						dprintf(2, "Failed to read file from archive, terminating\n");
						abort();
					}
				}
				//htmls = getHtmlCount(buf, file_info.uncompressed_size);
			}
			//Ok, now search
			checkFileMulti(buf, file_info.uncompressed_size, wi->rqs, wi->amount);
			for(size_t k = 0; k < wi->amount; k++)
				if(wi->rqs[k].ret == 1 && !(wi->op[k]->mode == REMOVE)) {
					addOutput(wi->op[k]->output, id);
				}
		}
		if(buf)
			unzCloseCurrentFile(archive);
		free(buf);

		//Don't forget to select next file
		ret = unzGoToNextFile(archive);
	};
	
	//Save results
	for(size_t i = 0; i < opcode_count; i++) {
		struct opcode *op = BigOpcodeList[i];
		if(!op->output->path.data)
			continue;
		//Sort ids
		qsort(op->output->buf.data, op->output->buf.length/sizeof(uint32_t), sizeof(uint32_t), id_sort);
		//Prepare for file opening
		size_t len = op->output->path.length;
		char path[len + 1];
		memcpy(path, op->output->path.data, len);
		path[len] = 0;
		//Open file
		FILE *out = fopen(path, "wb");
		if(out == NULL) {
			dprintf(2, "Failed to open file %s\n", path);
			continue;
		}
		//Write and close
		fwrite(op->output->buf.data, sizeof(uint32_t), op->output->buf.length/sizeof(uint32_t), out);
		fclose(out);
		//Print stats
	}

	//Free memory
	//freearchive:
	unzClose(archive);
	freetree:
	free(BigOpcodeList);
	free(waves);
	freeops:
	//TODO
	free(indata);
	indata = NULL;
	insize = 0;
}
