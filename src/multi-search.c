//Multi-search format
//SSA
//Makefile-like
//
//Search for Faust case-sensetive and save to searchtags/Faust
//"searchtags/Faust": 'Faust'
//
//Search for Faust case-insensetive and do not save to file
//faust: "Faust"
//
//ingram "ingram": 123 & "Ingram"
//^       ^        ^      ^
//|       |        |      Search term
//|       |        Tag id
//|       File name
//Target name
//
//<target> <path>: <tag/target> [logop] term/tag/target
//
//Load target from file
//mare = "searchtags/mare"
//or
//mare "searchtags/mare" =
//
//Every file should not be mentioned more than once
//Every target should not be mentioned more than once
//
//Simplified MSF:
//<target> <path>: <target> [logop] term
//
//Oversimplified MSF:
//target/path: <target> [logop] term

#include "fimfar.h"

#include <stdio.h>

extern char *archive; //Path to archive

struct target {
	struct stringbuf buf;
	struct stringview name, path;
	bool read_only;//Means loaded from disk
};

struct opcode {
	char *text; //Pattern to search
	struct target input; //List of ids for logic operation
	union {
		struct target output; //Output list of ids
		const char *outfile;
	};
	enum LogOp mode;
	bool sens; //Case-sensetiveness
};

struct tchunk {
	struct target targets[64];
	struct tchunk *next;
};

static size_t skipSpaces(const char *in, size_t left) {
	size_t skip = 0;
	for(; skip < left; skip++) {
		if(in[skip] == ' ' || in[skip] == '\t')
			continue;
		return skip;
	}
	return 0;
}

#define chkEOF() {if(pos >= insize) {fprintf(stderr, "Unexpected EOF\n"); exit(-1);}}
#define chkEOL() {if(indata[pos] == '\n') {fprintf(stderr, "Unexpected EOL\n"); exit(-1);}}
#define chkExists(name)
#define chkNotExists(name) !chkExists(name)
#define saveTarget(t) {size_t id = target_count%64; current->targets[id] = t; target_count++; if(!target_count%64) {current->next = malloc(sizeof(struct tchunk)); current = current->next; current->next = NULL;}}

void multisearch(const char *infile) {
	//Load infile
	char *indata;
	size_t insize = readfile(infile, (void**)&indata), pos = 0;
	size_t target_count = 0;
	struct tchunk root;
	struct tchunk *current = &root;
	root.next = NULL;
	//Parse infile
	while(pos < insize) {
	/*
	 * 	skip spaces
	 * 	if not '\n' and not '"'
	 * 		read target
	 *
	 * 	skip spaces
	 *	if '"'
	 *		read path
	 *	else
	 *		skip line
	 *
	 * 	skip spaces
	 * 	if ':'
	 * 		do search
	 *	else if '='
	 *		do load
	 *	else
	 *		error
	 *
	 *	save target
	 */
		if(pos == insize)
			break;
		if(indata[pos] == '\n') {
			pos++;
			continue;
		}
		struct target ot = {0};
		if (indata[pos] != '\n' && indata[pos] != '"') {
			//target name
			//TODO: It cannot start with '\'' or contain space or ':' or only digits
			for(size_t i = 0; i < insize-pos; i++) {
				//FIXME: unicode
				chkEOF();
				chkEOL();
				//Skip until space
				if(indata[pos + i] != ' ' && indata[pos + i] != '\t')
					continue;
				//save shit
				ot.name.data = indata + pos;
				ot.name.length = i;
				pos += i;
				break;
			}
			//TODO: check if already exists
	 		pos += skipSpaces(indata + pos, insize - pos);
		}
		if(indata[pos] == '"') {
			//path
			pos++;
			//TODO: check for relative paths
			for(size_t i = 0; i < insize-pos; i++) {
				//FIXME: unicode
				chkEOF();
				chkEOL();
				//Skip until quotes
				if(indata[pos + i] == '\n') {
					//error
				}
				if(indata[pos + i] != '"')
					continue;
				//save shit
				ot.path.data = indata + pos;
				ot.path.length = i;
				pos += i + 1;
				break;
			}
	 		pos += skipSpaces(indata + pos, insize - pos);
		}
		chkEOF();
		chkEOL();
		if(indata[pos] == ':') {
			//Prepare opcode
			pos++;
	 		pos += skipSpaces(indata + pos, insize - pos);
			char c = indata[pos];
			if(c == '"' || c == '\'') {
			}
			abort();
			//Save target
			saveTarget(ot);
			//Save opcode
		} else if (indata[pos] == '=') {
			//Load file
			pos++;
			if(!ot.path.data) {
				//Load path
				//TODO
				abort();
			}
			//Read file
			char *path = alloca(ot.path.length + 1);
			memcpy(path, ot.path.data, ot.path.length);
			path[ot.path.length] = 0;
			ot.buf.length = ot.buf.size = readfile(path, (void**)&ot.buf.data);
			if(!ot.buf.data)
				exit(-1);
			ot.read_only = true;
			//Save target
			saveTarget(ot);
		} else {
			fprintf(stderr, "Unexpected symbol at offset %zu\n", pos);
			exit(-1);
		}
	 	pos += skipSpaces(indata + pos, insize - pos);
	}
	current = &root;
	for(size_t i = 0; i < target_count;) {
		struct target *t = &current->targets[i%64];
		printf("%zu:\t", i);
		if(t->name.data)
			printf("%.*s ", t->name.length, t->name.data);
		if(t->path.data)
			printf("\"%.*s\"", t->path.length, t->path.data);
		if(!t->read_only) {
			abort();
		} else
			printf("=");
		printf("\n");
		i++;
		if(!i%64)
			current = current->next;
	}
	//Create tree
	//Check for cyclic dependencies
	
	//Open archive
	//Iterate over epub
	//	Iterate over tree
	
	//Save results
	
	//Free memory
	free(indata);
	indata = NULL;
	insize = 0;
}
