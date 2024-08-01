#include "fimfar.h"
#include "builder.h"

#include <sys/mman.h>
#include <assert.h>

void bufalloc(struct stringbuf *buf) {
	if(buf->size < buf->length) {
		free(buf->data);
		buf->data = malloc(buf->length);
		buf->size = buf->length;
		if(!buf->data)
			abort();
	}
}

void strmemtobuf(struct stringbuf *restrict buf, const void *data, size_t size) {
	//Copy and add NULL-terminator
	buf->length = size + 1;
	//Check buffer size
	bufalloc(buf);
	//Do copy
	memcpy(buf->data, data, size);
	buf->data[size] = 0;
}

void strmembufappend(struct stringbuf *restrict buf, const void *data, size_t size) {
	//Fast path for empty buffer
	if(size == 0)
		return;
	if(buf->length == 0) {
		strmemtobuf(buf, data, size);
		return;
	}
	//buf should be null-terminated
	assert(buf->data[buf->length - 1] == 0);
	//Check buffer size
	if(buf->size < buf->length + size) {
		buf->size = buf->length + size;
		buf->data = realloc(buf->data, buf->size);
		if(!buf->data)
			abort();
	}
	//Do copy
	memcpy(buf->data + buf->length - 1, data, size);
	buf->length += size;
	buf->data[buf->size - 1] = 0;
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
