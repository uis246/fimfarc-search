// Note: data is not used if this is the "document:" field: it goes
// directly to m_metaData[cstr_dj_keycontent] to avoid an extra copy
// 
// Messages are made of data elements. Each element is like:
// name: len\ndata
// An empty line signals the end of the message, so the whole thing
// would look like:
// Name1: Len1\nData1Name2: Len2\nData2\n

#include "rclcommon.h"


// -- part that maybe will be shared --
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

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
