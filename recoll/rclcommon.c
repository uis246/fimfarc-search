#include "rclcommon.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

uint64_t maxsizekb;

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
