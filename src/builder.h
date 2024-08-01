#pragma once

#include "fimfar.h"
#include <stdio.h>

void bufalloc(struct stringbuf *buf);
#define strtobuf(buf, string) strmemtobuf(buf, string, strlen(string))
void strmemtobuf(struct stringbuf *buf, const void *data, size_t size);
void strmembufappend(struct stringbuf *restrict buf, const void *data, size_t size);
void tagtobuf(struct stringbuf *tagbuf, struct tag *tag);
void writeAndFreeTags(FILE *out, struct stringbuf *tagbuf);
