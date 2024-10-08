#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#define STORE_PATH 0

#define ASSOC_PATH "db/assoc.bin"
#define TAG_PATH "db/tag.bin"
#define STORY_PATH "db/story.bin"
#define EXTRA_PATH "db/extra.bin"
#define ALTTAG_PATH "db/alttag.bin"

enum LogOp {
        AND = 0,
        OR,
        REMOVE,
};

enum ComplStatus {
	INCOMPLETE = 0,
	COMPLETE,
	HIATUS,
	CANCELLED
};
enum ContentRating {
	EVERYPONE = 0,
	TEEN,
	MATURE
};

struct stringbuf {
	size_t size, length;
	char *data;
};
struct stringview {
	char *data;
	size_t length;
};
struct tag {
	struct stringbuf name;
	uint32_t id;
};

__attribute((packed)) struct id_text_file {
	uint32_t id;
	uint_least8_t length;// = name + path
	char data[];
};
#define ITF_SIZE 5

__attribute((packed)) struct story_tag_file {
	uint32_t story_id;
	uint_least16_t tag_id;
};
#define STF_SIZE 6

__attribute((packed))  struct extra_leaf {
	uint32_t id;
	uint16_t skipbytes;
	//skipbytes is size of entire structure on disk
	uint8_t complete, cr;//CompleteStatus and ContentRating
	uint64_t ctime, mtime;//create time and update time
	uint32_t likes, dislikes, comments, views;
	uint16_t ldlen, sdlen, tagsz;
};
#define EXL_SIZE (1*4 + 1*2 + 2*1 + 2*8 + 4*4 + 3*2)

//Format endianess conversion
#if 0
//Format to host
#define f32toh(x) le32toh(x)
//Host to format
#define htof32(x) htobe32(x)
#else
#define f32toh(x) x
#define htof32(x) x
#endif

#ifdef __GNUC__
#define unlikely(x) __builtin_expect(x, 0)
#else
#define unlikely(x) x
#endif

struct checkRq {
	const char *text;
	bool sens;

	uint8_t ret;
};

void builder();
void search();
void multisearch(const char * restrict archive, const char * restrict infile);
void arcstat(const char *restrict archive);

size_t readfile(const char *restrict path, void *restrict *ptr);
void bufappend(struct stringbuf *restrict buf, const void *restrict data, const size_t size);
bool checkFile(const void *data, size_t size, const char *text, bool sens);
void checkFileMulti(const void *data, size_t size, struct checkRq *rqs, size_t amount, char **cache);
int id_sort(const void *a, const void *b);
