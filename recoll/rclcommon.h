#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct hdr{
	char *type;
	uint8_t *data;
	uint32_t length;
};

// handler returns true if no errors happened
bool handle(const struct hdr *paramv, size_t paramc);
void initHandler(int argc, char *argv[]);

// maximal size of output file
extern uint64_t maxsizekb;
