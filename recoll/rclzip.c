// Note: data is not used if this is the "document:" field: it goes
// directly to m_metaData[cstr_dj_keycontent] to avoid an extra copy
// 
// Messages are made of data elements. Each element is like:
// name: len\ndata
// An empty line signals the end of the message, so the whole thing
// would look like:
// Name1: Len1\nData1Name2: Len2\nData2\n

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

struct hdr{
	char *type;
	uint8_t *data;
	uint32_t length;
};

static int readhdr(struct hdr *hdr) {
	char *line = NULL;
	size_t n;
	ssize_t ret = getline(&line, &n, stdin);
	if(ret == -1) {
		//EOF, just fine
		free(line);
		return 0;
	}

	if(ret < 2) {
		//end of header
		free(line);
		return 0;
	}
	
	char *delim = strchr(line, ' ');
	if(delim == NULL || delim == line || *(delim-1) != ':') {
		//error
		free(line);
		return -1;
	}
	
	//TODO: add maximum length
	hdr->length = atol(delim + 1);
	//NOTE: can be replaced by editing line
	char *type = strndup(line, delim - line - 1);
	free(line);
	line = NULL;
	if(type == NULL)
		return -1;
	hdr->type = type;
	if(hdr->length == 0) {
		hdr->data = NULL;
		return 1;
	}
	// Allocate buffer for data and null-terminator
	void *buf = malloc(hdr->length + 1);
	if(buf == NULL) {
		//error
		//free(hdr->type);
		free(type);
		hdr->type = NULL;
		return -1;
	}
	hdr->data = buf;
	uint32_t left = hdr->length;
	n = fread(hdr->data + hdr->length - left, 1, left, stdin);
	hdr->data[n] = 0;
	if(n != left) {
		//does it matter difference between EOF and error?
		free(hdr->data);
		hdr->data = NULL;
		free(hdr->type);
		hdr->type = NULL;
		return -1;
	}
	return 1;
}

// handler returns true if no errors happened
bool handle(const struct hdr *paramv, size_t paramc);

static uint64_t maxsizekb;

int main(/*int argc, char* argv[], char* envp[]*/) {
	{
	const char *mskb = getenv("RECOLL_FILTER_MAXMEMBERKB");
	maxsizekb = mskb != NULL ? atoll(mskb) : UINT32_MAX;
	}
	//I'll do like python implementation
	struct hdr params[10];
	while(1) {
		size_t i;
		int ret;
		// read request headers
		for(i = 0; i < 10; i++) { 
			ret = readhdr(&params[i]);
			// not a valid header
			if(ret != 1)
				break;
		};
		if(i == 10 && ret == 1) {
			// 10 headers and still not all
			// this means too many headers
			ret = -1;
		}
		// error while parsing last header
		if(ret == -1) {
			// error
			exit(-1);
		}
		// otherwise last header was normal
		if(i == 0) {
			// or was it?
			return -1;
		}

		//call handler
		ret = handle(params, i);

		// free headers
		while(i--) {
			free(params[i].data);
			params[i].data = NULL;
			free(params[i].type);
			params[i].type = NULL;
		};

		// stop once error occured
		if(ret == false)
			return -1;
	};
}

// -- end of shared part --
// -- part that maybe will be shared --
#include <inttypes.h>

const struct hdr* findParam(const struct hdr *restrict first, const size_t size, const char *restrict name) {
	for(size_t i = 0; i < size; i++)
		if(strcasecmp(first[i].type, name) == 0)
			return first + i;
	return NULL;
}

#define needParam(first, size, name, out) {out = findParam(first, size, name); if(out == NULL) return false;}
#define addParam(params, count, name, value, len) {params[count].type = name; params[count].length = len; params[count++].data = (uint8_t*)value;}

void respond(const struct hdr *paramv, size_t paramc) {
	for(size_t i = 0; i < paramc; i++) {
		fprintf(stdout, "%s: %"PRIu32"\n", paramv[i].type, paramv[i].length);
		if(paramv[i].length)
			fwrite(paramv[i].data, 1, paramv[i].length, stdout);
	}
	fwrite("\n", 1, 1, stdout);
	fflush(stdout);
}

// -- format-specific part --
#include <minizip/unzip.h>

#include <time.h>
#include <assert.h>
bool handle(const struct hdr *paramv, size_t paramc) {
	static unzFile *archive = NULL;
	static unz_global_info info;

	struct hdr reply[10];
	size_t repc = 0;
	const struct hdr *filename, *ipath;
	int ret;
	bool readnext;
	needParam(paramv, paramc, "filename", filename);
	ipath = findParam(paramv, paramc, "ipath");

	if(filename->length) {
		// close old file, open new one
		if(archive != NULL) {
			// close
			unzClose(archive);
		}
		// open file
		archive = unzOpen64(filename->data);
		if(archive == NULL) {
			// report error
			//maybe later
			abort();
			//just give Fileerror
		}
		if (unzGetGlobalInfo(archive, &info) != UNZ_OK) {
			// report error
			//maybe later
			abort();
		}
		if(ipath == NULL || ipath->length == 0) {
			// return self doc
			addParam(reply, repc, "Document", NULL, 0);
			addParam(reply, repc, "Mimetype", "text/plain", 10);
			respond(reply, repc);
			return true;
		}
	}
	// check if archive is open
	if(archive == NULL) {
		addParam(reply, repc, "Eofnext", NULL, 0);
		addParam(reply, repc, "Fileerror", NULL, 0);
		respond(reply, repc);
		return true;
	}
	readnext = ipath == NULL || ipath->length == 0;
	if(!readnext) {
		// search by name
		ret = unzLocateFile(archive, (const char*)ipath->data, 1);
		if(ret != UNZ_OK) {
			// not found
			addParam(reply, repc, "Document", NULL, 0);
			addParam(reply, repc, "Eofnow", NULL, 0);
			addParam(reply, repc, "filename", NULL, 0);
			addParam(reply, repc, "Ipath", NULL, 0);
			respond(reply, repc);
			return true;
		}
	}
	unz_file_info finfo;
	char *fname = alloca(2048);
	if(readnext) {
		// next
		// TODO: check if last already was reached
		do {
			ret = unzGetCurrentFileInfo(archive, &finfo, fname, 2048, NULL, 0, NULL, 0);
			if(ret != UNZ_OK)
				// damaged archive?
				// document error
				goto docerror;
			// check against filename filter
			size_t len = strlen(fname);
			assert(len < 2048);
			if(fname[len - 1] == '/')
				// skip dirs
				goto next;
			// dirty check
			if(strncmp("epub/", fname, 5) != 0)
				goto next;
			// ...
			// check file size
			if(finfo.uncompressed_size > maxsizekb * 1024)
				goto next;

			// check passed, sending file
			const char *filename = strrchr(fname, '/');
			if(filename == NULL)
				filename = fname;
			else
				filename++;
			void *buf = malloc(finfo.uncompressed_size);
			if(buf == NULL) {
				// out of memory
				unzClose(archive);
				archive = NULL;
				return false;
			}
			ret = unzOpenCurrentFile(archive);
			if(ret < 0) {
				// archive damaged
				free(buf);
				addParam(reply, repc, "Ipath", fname, len);
				addParam(reply, repc, "filename", filename, len - (filename - fname));
				goto docerror;
			}
			ret = unzReadCurrentFile(archive, buf, finfo.uncompressed_size);
			if(ret < 0) {
				// archive damaged
				free(buf);
				addParam(reply, repc, "Ipath", fname, len);
				addParam(reply, repc, "filename", filename, len - (filename - fname));
				goto docerror;
			}
			//TODO: check if CRC is correct
			unzCloseCurrentFile(archive);
			addParam(reply, repc, "Document", buf, finfo.uncompressed_size);
			addParam(reply, repc, "Ipath", fname, len);
			addParam(reply, repc, "filename", filename, len - (filename - fname));
			// -- extract metadata --
			char mtime[21];
			uint64_t time;
			{
				struct tm t = {0};
				t.tm_year = finfo.tmu_date.tm_year - 1900;
				t.tm_mon = finfo.tmu_date.tm_mon;
				t.tm_mday = finfo.tmu_date.tm_mday;
				t.tm_hour = finfo.tmu_date.tm_hour;
				t.tm_min = finfo.tmu_date.tm_min;
				t.tm_sec = finfo.tmu_date.tm_sec;
				time = mktime(&t);
			}
			int slen = snprintf(mtime, 21, "%"PRIu64, time);
			if(slen >= 0)
				addParam(reply, repc, "modificationdate", mtime, slen);
			// -- end extract metadata --

			ret = unzGoToNextFile(archive);
			if(ret != UNZ_OK)
				addParam(reply, repc, "Eofnext", NULL, 0);
			respond(reply, repc);
			free(buf);
			if(ret != UNZ_OK) {
				unzClose(archive);
				archive = NULL;
			}
			return true;
			docerror:
			ret = unzGoToNextFile(archive);
			if(ret == UNZ_OK)
				addParam(reply, repc, "subdocerror", NULL, 0)
			else {
				addParam(reply, repc, "fileerror", NULL, 0);
				addParam(reply, repc, "Eofnext", NULL, 0);
			}
			respond(reply, repc);
			return true;
			next:
			ret = unzGoToNextFile(archive);
		} while(ret == UNZ_OK && readnext);
		addParam(reply, repc, "Eofnow", NULL, 0);
		respond(reply, repc);
		// close archive
		unzClose(archive);
		archive = NULL;
		return true;
	} else {
		ret = unzGetCurrentFileInfo(archive, &finfo, fname, 2048, NULL, 0, NULL, 0);
		if(ret != UNZ_OK)
			// damaged archive?
			// NOTE: maybe just report file error?
			return false;
		void *buf = malloc(finfo.uncompressed_size);
		if(buf == NULL) {
			// out of memory
			unzClose(archive);
			archive = NULL;
			return false;
		}
		ret = unzOpenCurrentFile(archive);
		if(ret != UNZ_OK) {
			// archive damaged
			free(buf);
			unzClose(archive);
			archive = NULL;
			return false;
		}
		ret = unzReadCurrentFile(archive, buf, finfo.uncompressed_size);
		if(ret < 0) {
			// archive damaged
			free(buf);
			unzCloseCurrentFile(archive);
			unzClose(archive);
			archive = NULL;
			return false;
		}
		unzCloseCurrentFile(archive);
		addParam(reply, repc, "Document", buf, finfo.uncompressed_size);
		addParam(reply, repc, "Ipath", ipath->data, ipath->length);
		const char *filename = strrchr((const char*)ipath->data, '/');
		if(filename == NULL)
			filename = fname;
		else
			filename++;
		addParam(reply, repc, "filename", filename, strlen(filename));
		respond(reply, repc);
	}

	return true;
}
