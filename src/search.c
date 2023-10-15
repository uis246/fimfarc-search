#include "fimfar.h"

#include <argp.h>
#include <minizip/unzip.h>
#include "ioapi_mem.h"

#include <assert.h>

static struct {
	char *text;//Pattern to search
	char *archive;//Path to archive
	char *list;//Path to list of ids
	char *out;//Path to output list of ids
	enum LogOp mode;
	bool sens;
} search_args;

error_t search_parse_opt(int key, char *arg, struct argp_state*) {
switch (key) {
	case 'l':
		search_args.list = arg;
		break;
	case 'o':
		search_args.out = arg;
		break;
	case 'a':
		search_args.archive = arg;
		break;
	case 'p':
		if (strcmp(arg, "or") == 0)
			search_args.mode = OR;
		else if (strcmp(arg, "remove") == 0)
			search_args.mode = REMOVE;
		else
			search_args.mode = AND;
		break;
	case 'C':
		search_args.sens = true;
		break;
	case ARGP_KEY_ARG:
		search_args.text = arg;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
}
return 0;
}



//Return true if found pattern
bool checkFile(const void *data, size_t size, const char *text, bool sens) {
	struct checkRq rq = {text, sens, false};
	checkFileMulti(data, size, &rq, 1);
	return rq.ret;
}

//Return true if found pattern
void checkFileMulti(const void *data, size_t size, struct checkRq *rqs, size_t amount) {
	unzFile *story;
	char *fname;
	void *buf;// size_t bsize = 0;
	int ret;
	zlib_filefunc_def filefunc = { 0 };
	ourmemory_t unzmem = {0};

	unzmem.size = size;
	unzmem.base = (void*)data;

	fill_memory_filefunc(&filefunc, &unzmem);

	story = unzOpen2("", &filefunc);
	if (unlikely(!story)) {
		dprintf(2, "Failed to open story file\n");
		return;
	}

	unz_global_info info;
	ret = unzGetGlobalInfo(story, &info);
	if (unlikely(ret != UNZ_OK)) {
		dprintf(2, "Failed to geet story global info\n");
		unzClose(story);
		return;
	}

	//NOTE: alloca looks fine here, malloc is excessive
	fname = alloca(1024);

	for(uLong i = 0; i < info.number_entry; i++) {
		unz_file_info finfo;
		size_t len;
		if (unlikely(ret != UNZ_OK)) {
			dprintf(2, "Failed to load next story file\n");
			break;
		}

		ret = unzGetCurrentFileInfo(story, &finfo, fname, 1024, NULL, 0, NULL, 0);
		if (unlikely(ret != UNZ_OK)) {
			dprintf(2, "Couldn't read file info, skipping\n");
			continue;
		}
		len = strnlen(fname, 1024);
		if (fname[len - 1] == '/') {
			//It's a dir, skipping
			ret = unzGoToNextFile(story);
			continue;
		}

		if (strcmp(fname + len - 5, ".html") == 0) {
			//That's a chapter
			//Read it
			//TODO: reduce amount of mallocs
			buf = malloc(finfo.uncompressed_size);
			if (unlikely(!buf)) {
				dprintf(2, "Failed to allocate memory\n");
				free(buf);
				break;
			}
			ret = unzOpenCurrentFile(story);
			if (unlikely(ret != UNZ_OK)) {
				dprintf(2, "Failed to open chapter, skipping\n");
			} else {
				//Do read
				//Heaviest call in function
				//Over 70% of total time
				ret = unzReadCurrentFile(story, buf, finfo.uncompressed_size);
				if (ret < 0) {
					//Error
					dprintf(2, "Failed to read chapter, skipping\n");
				} else {
					//Ok, now search
					bool stop = true;
					for(size_t j = 0; j < amount; j++) {
						if(rqs[j].ret)
							continue;
						char *sret;
						if (rqs[j].sens)
							sret = strcasestr(buf, rqs[j].text);
						else
							sret = strstr(buf, rqs[j].text);
						if (sret != NULL) {
							//FOUND!
							rqs[j].ret = true;
							goto found;
						}
						stop = stop & rqs[j].ret;
					}
					if(stop)
						goto found;
				}
				unzCloseCurrentFile(story);
			}
			free(buf);
		}
		ret = unzGoToNextFile(story);
	}

	//free(fname);//Not a malloc
	unzClose(story);
	return;

	found:
	unzCloseCurrentFile(story);
	free(buf);
	//free(fname);//Not a malloc
	unzClose(story);
	return;
}

static int id_sort(const void *a, const void *b) {
	const uint32_t *A = a, *B = b;
	return *A - *B;
}

void search() {
	struct stringbuf stories = {0};
	FILE *out;
	size_t list_s = 0;
	uint32_t *list = NULL;
	char *fname;
	unzFile *archive;
	void *buf;
	if (!search_args.archive || !search_args.out || !search_args.text) {
		dprintf(2, "You must specify patthern and paths to archive and output\n");
		return;
	}
	if (search_args.list) {
		//Listed mode
		if (search_args.mode == OR) {
			dprintf(2, "Mode \"OR\" is not implemented yet\n");
			return;
		}
		//Open list
		list_s = readfile(search_args.list, (void**)&list);
		list_s /= sizeof(uint32_t);
		if (!list)
			return;
	} else
		//Listless mode
		search_args.mode = OR;

	//Open archive
	//NOTE: no file mapping becase I have 32-bit systems
	archive = unzOpen64(search_args.archive);
	if (!archive) {
		dprintf(2, "Failed to open archive file %s\n", search_args.archive);
		perror("Error");
		free(list);
		return;
	}
	unz_global_info info;
	if (unzGetGlobalInfo(archive, &info) != UNZ_OK) {
		dprintf(2, "Failed to get archive global info\n");
		goto cancel;
	}
	//Open output file
	out = fopen(search_args.out, "wb");
	//Search for listed files in archive
	fname = malloc(4096);
	int ret = UNZ_OK;//That's kinda from unzGoToNextFile
	for(uLong i = 0; i < info.number_entry; i++) {
		unz_file_info file_info;
		size_t len;
		if (ret != UNZ_OK) {//unzGoToNextFile failed
			dprintf(2, "Failed to read next file\n");
			break;
		}
		ret = unzGetCurrentFileInfo(archive, &file_info, fname, 4096, NULL, 0, NULL, 0);
		if (ret != UNZ_OK) {
			dprintf(2, "Couldn't read file info, skipping\n");
			continue;
		}
		len = strlen(fname);
		if (fname[len - 1] == '/') {
			//It's a dir, skipping
			ret = unzGoToNextFile(archive);
			continue;
		}
		if (strcmp(fname + len - 5, ".epub") == 0) {
			//That's a story
			//Check in list
			uint32_t id;
			bool found = false;
			fname[len - 5] = 0;
			id = strtoul(strrchr(fname, '-') + 1, NULL, 10);

			if (list) {
			for (size_t i = 0; i < list_s; i++) {
				if (list[i] == id) {
					found = true;
					break;
				}
			}
			if (!found ^ (search_args.mode == OR)) {
				//Not found in list, skip
				//Or found, but in OR mode
				ret = unzGoToNextFile(archive);
				continue;
			}
			}

			//Read it
			buf = malloc(file_info.uncompressed_size);
			if (!buf) {
				dprintf(2, "Failed to allocate memory\n");
				free(buf);
				goto end;
			}
			ret = unzOpenCurrentFile(archive);
			if (ret != UNZ_OK) {
				dprintf(2, "Failed to open file from archive, skipping\n");
			} else {
				//Read and search
				ret = unzReadCurrentFile(archive, buf, file_info.uncompressed_size);
				if (ret < 0) {
					//Error
					dprintf(2, "Failed to read file from archive, skipping\n");
				} else {
					//Ok, now search
					found = checkFile(buf, file_info.uncompressed_size, search_args.text, search_args.sens);
					if (found ^ (search_args.mode == REMOVE))
						//Add to stories list
						bufappend(&stories, &id, sizeof(id));
				}
				unzCloseCurrentFile(archive);
			}
			free(buf);
		}
		ret = unzGoToNextFile(archive);
	}
	end:
	free(fname);
	qsort(stories.data, stories.length/sizeof(uint32_t), sizeof(uint32_t), id_sort);
	fwrite(stories.data, sizeof(uint32_t), stories.length/sizeof(uint32_t), out);
	fclose(out);
	cancel:
	unzClose(archive);
	free(list);
}
