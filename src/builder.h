#pragma once

#include "fimfar.h"
#include <stdio.h>

void bufalloc(struct stringbuf *buf);
void strtobuf(struct stringbuf *buf, const char *string);
void tagtobuf(struct stringbuf *tagbuf, struct tag *tag);
void writeAndFreeTags(FILE *out, struct stringbuf *tagbuf);
