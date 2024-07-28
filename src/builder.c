#include "fimfar.h"
#include "builder.h"

#include <sys/mman.h>

#define STATE_REPORT 0
#define PROGRESS_REPORT 0
#define BUILD_EXTRA 1

#if STATE_REPORT
static const char *StateName[] =
{"Reset", "Story", "Tags", "TagInfo", "Archive", "Junk"};
#endif

// extra db from story state:
// publishing time - s(t) +
// update time - s(t) +
// completion status - s(i) +
// content rating - s(i) +
// long description - s +
// short description - s +
// likes - i +
// dislikes - i +
// comments - i +
// views - i +
// tags - i[] +

void bufalloc(struct stringbuf *buf) {
	if(buf->size < buf->length) {
		free(buf->data);
		buf->data = malloc(buf->length);
		buf->size = buf->length;
	}
}
void strtobuf(struct stringbuf *buf, const char *string) {
	//Copy with NULL-terminator
	buf->length = strlen(string)+1;
	//Check buffer size
	bufalloc(buf);
	//Do copy
	memcpy(buf->data, string, buf->length);
}

#define TAG_PAGE_SIZE 4096*8
void tagtobuf(struct stringbuf *tagbuf, struct tag *tag) {
	struct tag *tags = (struct tag*)tagbuf->data;
	size_t count = tagbuf->length/sizeof(*tag);
	//First allocation
	if(tagbuf->size == 0) {
		tagbuf->size = TAG_PAGE_SIZE;
		tagbuf->data = mmap(NULL, tagbuf->size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
	}
	//Search for same
	for(size_t i = 0; i < count; i++)
		if(tags[i].id == tag->id)
			return;//Already added
	//Check buffer
	if(tagbuf->size < tagbuf->length+sizeof(*tag)) {
		tagbuf->data = mremap(tagbuf->data, tagbuf->size, tagbuf->size+TAG_PAGE_SIZE, MREMAP_MAYMOVE);
		tagbuf->size += TAG_PAGE_SIZE;
	}
	//Copy
	memcpy(tagbuf->data+tagbuf->length, tag, sizeof(*tag));
	//tags[count] = *tag;
	tagbuf->length += sizeof(*tag);
	//Remove buffer from tag
	tag->name.data = NULL;
	tag->name.size = 0;
	tag->name.length = 0;
}
void tagfreebuf(struct stringbuf *buf) {
	munmap(buf->data, buf->size);
	buf->data = NULL;
	buf->size = 0;
}

void writeAndFreeTags(FILE *out, struct stringbuf *tagbuf) {
	uint32_t milestone = tagbuf->length/sizeof(struct tag);
	for(uint32_t i = 0; i < milestone; i++) {
		struct id_text_file *itf;
		struct tag *tag = (struct tag*)tagbuf->data;
		itf = alloca(ITF_SIZE + tag[i].name.length - 1);
		itf->id = tag[i].id;
		itf->length = tag[i].name.length - 1;
		memcpy(&itf->data, tag[i].name.data, itf->length);
		fwrite(itf, 1, ITF_SIZE + itf->length, out);
	}
	//Free tags
	for(uint32_t i = 0; i < milestone; i++) {
		free(((struct tag*)tagbuf->data)[i].name.data);
	}
	tagfreebuf(tagbuf);
}
