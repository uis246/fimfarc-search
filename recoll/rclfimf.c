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
#include "fimfar.h"
#include <minizip/unzip.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <time.h>
#include <assert.h>

static const char *statuses[] = {
	"incomplete",
	"complete",
	"hiatus",
	"cancelled"
}, *ratings[] = {
	"everypony",
	"teen",
	"mature"
};

void initHandler(int argc, char *argv[]) {(void)argc;(void)argv;}

#define closeArchive() {unzClose(archive); archive = NULL; prev = 0; off = 0; if(meta) {munmap(meta, metasize); meta = NULL;} if(tag) {munmap(tag, tagsize); tag = NULL;}}
#define FNAME_MAX 2048

bool handle(const struct hdr *paramv, size_t paramc) {
	static unzFile *archive = NULL;
	static unz_global_info info;
	static uint8_t *meta = NULL, *tag = NULL, *grp = NULL;
	static off_t metasize, tagsize, grpsize/*grpsize in u32*/;
	static uint32_t prev = 0;
	static off_t off = 0, grpoff = 0;

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
			closeArchive();
		}
		if(meta != NULL) {
			munmap(meta, metasize);
			meta = NULL;
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
			// self doc is only returned during indexing. I think.
			// open metadata file
			//NOTE: assuming char is 1 byte long
			char *metaname = alloca(filename->length + 5);
			memcpy(metaname, filename->data, filename->length);
			if(metaname) {
				memcpy(metaname + filename->length, ".bin", 5);
				int fd;
				// open extra metadata db
				fd = open(metaname, O_RDONLY);
				if(fd != -1) {
					// get size
					struct stat sb;
					fstat(fd, &sb);
					metasize = sb.st_size;
					// map
					if(metasize <= UINT32_MAX)
						meta = mmap(NULL, metasize, PROT_READ, MAP_SHARED, fd, 0);
					close(fd);
				}
				memcpy(metaname + filename->length, ".tag", 4);
				// open alttag db
				fd = open(metaname, O_RDONLY);
				if(fd != -1) {
					// get size
					struct stat sb;
					fstat(fd, &sb);
					tagsize = sb.st_size;
					// map
					if(tagsize <= UINT32_MAX)
						tag = mmap(NULL, tagsize, PROT_READ, MAP_SHARED, fd, 0);
					close(fd);
				}
				memcpy(metaname + filename->length, ".grp", 4);
				// open groupassoc db
				fd = open(metaname, O_RDONLY);
				if(fd != -1) {
					// get size
					struct stat sb;
					fstat(fd, &sb);
					grpsize = sb.st_size;
					// map
					if(grpsize <= UINT32_MAX)
						grp = mmap(NULL, grpsize, PROT_READ, MAP_SHARED, fd, 0);
					grpsize /= sizeof(uint32_t);
					close(fd);
				}
				//free(metaname);
			}
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
	char *fname = alloca(FNAME_MAX);
	if(readnext) {
		// next
		// TODO: check if last already was reached
		do {
			ret = unzGetCurrentFileInfo(archive, &finfo, fname, FNAME_MAX, NULL, 0, NULL, 0);
			if(ret != UNZ_OK)
				// damaged archive?
				// document error
				goto docerror;
			// check against filename filter
			size_t len = strlen(fname);
			assert(len < FNAME_MAX);
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
			const char *fileid = strrchr(filename, '-');
			if(fileid == NULL)
				fileid = filename;
			else
				fileid++;
			uint32_t id = strtoul(fileid, NULL, 10);
			//TODO: unhardcode excluded stories
			if(id == 221463 || /*id == 234296 ||*/ id == 363931)
				goto next;
			void *buf = malloc(finfo.uncompressed_size);
			if(buf == NULL) {
				// out of memory
				closeArchive();
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
			// if metadata file is open
			if(meta != NULL) {
				// check if id in zip monotonically grows
				// db is sorted by id
				if(prev > id) {
					// not stonks, search from start
					off = 0;
					grpoff = 0;
				}
				struct extra_leaf *thismeta = (void*)(meta + off);
				// find by id
				while(off < metasize && thismeta->id < id) {
					off += (off_t)thismeta->skipbytes;
					thismeta = (void*)(meta + off);
				}
				if(off < metasize && thismeta->id == id) {
					// found, add metadata
					char mtime[21], ctime[21];
					int slen = snprintf(mtime, 21, "%"PRIu64, thismeta->mtime);
					if(slen >= 0)
						addParam(reply, repc, "modificationdate", mtime, slen);
					slen = snprintf(ctime, 21, "%"PRIu64, thismeta->ctime);
					if(slen >= 0)
						addParam(reply, repc, "dctime", ctime, slen);
					// validate
					uint8_t idx = thismeta->complete;
					if(idx >= sizeof(statuses)/sizeof(*statuses))
						//fuck it, crash
						abort();
					addParam(reply, repc, "status", statuses[idx], strlen(statuses[idx]));
					idx = thismeta->cr;
					if(idx >= sizeof(ratings)/sizeof(*ratings))
						//fuck it, crash
						abort();
					addParam(reply, repc, "rating", ratings[idx], strlen(ratings[idx]));
					// add tags
					char *tags_list = alloca(1024);
					uint32_t *sorted_tags = alloca(thismeta->tagsz);
					memcpy(sorted_tags, meta + off + EXL_SIZE + thismeta->ldlen + thismeta->sdlen, thismeta->tagsz);
					uint_fast16_t count = thismeta->tagsz/sizeof(uint32_t), bufused = 0;
					qsort(sorted_tags, count, sizeof(uint32_t), id_sort);
					//Add tags
					for(size_t off = 0, i = 0; off < (size_t)tagsize && i < count;) {
						const struct id_text_file *itf = (struct id_text_file*)(tag + off);
						if(itf->id == sorted_tags[i]) {
							// now add tag for real
							if(bufused + 1 > 1024)
								abort();
							memcpy(tags_list + bufused, itf->data, itf->length);
							tags_list[bufused + itf->length] = ' ';
							bufused += itf->length + 1;
							i++;
						} else if(itf->id < sorted_tags[i]) {
							// not there yet
						} else if(itf->id > sorted_tags[i]) {
							// too far, tag not found in db
							i++;
							continue;
						}
						off += ITF_SIZE + itf->length;
					}
					if(bufused != 0)
						addParam(reply, repc, "keywords", tags_list, bufused - 1);
					//Add groups
					if(grp != NULL) {
						uint32_t *grp_kv = (uint32_t*)grp;
						// find by id
						while(grpoff < grpsize && *(grp_kv + grpoff) < id)
							grpoff += 2;

						#define groups_list_size 2048
						char *groups_list = alloca(groups_list_size);
						bufused = 0;
						while(grpoff < grpsize - 1 && *(grp_kv + grpoff) == id) {
							bufused += snprintf(groups_list + bufused, groups_list_size - bufused, "%d ", *(grp_kv + grpoff + 1));
							if(bufused >= groups_list_size) {
								//story is in too many folders, probably trolling campaign
								// pretend there are no folders
								bufused = 0;
								break;
							}
							grpoff += 2;
						}
						if(bufused != 0)
							addParam(reply, repc, "folder", groups_list, bufused - 1);
					}
					//TODO: add long describrion as annotation and short as abstract?
				} else
					// not found
					//can't happen, worth investigation
					abort();
			}
			// -- end extract metadata --

			//TODO: iterate until next epub, recoll doesn't like eofnow
			ret = unzGoToNextFile(archive);
			if(ret != UNZ_OK)
				addParam(reply, repc, "Eofnext", NULL, 0);
			prev = id;
			respond(reply, repc);
			free(buf);
			if(ret != UNZ_OK) {
				closeArchive();
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
		closeArchive();
		return true;
	} else {
		ret = unzGetCurrentFileInfo(archive, &finfo, fname, FNAME_MAX, NULL, 0, NULL, 0);
		if(ret != UNZ_OK)
			// damaged archive?
			// NOTE: maybe just report file error?
			return false;
		void *buf = malloc(finfo.uncompressed_size);
		if(buf == NULL) {
			// out of memory
			closeArchive();
			return false;
		}
		ret = unzOpenCurrentFile(archive);
		if(ret != UNZ_OK) {
			// archive damaged
			free(buf);
			closeArchive();
			return false;
		}
		ret = unzReadCurrentFile(archive, buf, finfo.uncompressed_size);
		if(ret < 0) {
			// archive damaged
			free(buf);
			unzCloseCurrentFile(archive);
			closeArchive();
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
