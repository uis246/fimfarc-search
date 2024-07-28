#include "fimfar.h"
#include <stdio.h>

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
