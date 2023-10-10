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
	struct stringview text; //Pattern to search
	const struct target *input; //List of ids for logic operation
	union {
		struct target *output; //Output list of ids
		const char *outfile;
	};
	enum LogOp mode;
	bool sens; //Case-sensetiveness
};

struct tchunk {
	struct target targets[64];
	struct tchunk *next;
};

struct ochunk {
	struct opcode opcodes[64];
	struct ochunk *next;
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

inline static bool cmpstrview(const struct stringview *restrict a, const struct stringview *restrict b) {
	if(a->length != b->length)
		return false;
	return memcmp(a->data, b->data, a->length) == 0;
}

#define chkEOF() {if(pos >= insize) {fprintf(stderr, "Unexpected EOF\n"); exit(-1);}}
#define chkEOL(n) {if(indata[pos + n] == '\n') {fprintf(stderr, "Unexpected EOL\n"); exit(-1);}}
#define chkExists(name)
#define chkNotExists(name) !chkExists(name)
#define saveTarget(t) &current->targets[target_count%64]; {current->targets[target_count%64] = t; target_count++; if(!target_count%64) {current->next = malloc(sizeof(struct tchunk)); current = current->next; current->next = NULL;}}
#define searchTarget(output, tarname) {struct tchunk *cur = &root; if(tarname.length)for(size_t i = 0; i < target_count;) {\
		if(cmpstrview(&tarname, &cur->targets[i%64].name)){ output = &cur->targets[i%64]; break;}\
	}}
#define saveOpcode(t) {o_current->opcodes[opcode_count%64] = t; opcode_count++; if(!opcode_count%64) {o_current->next = malloc(sizeof(struct ochunk)); o_current = o_current->next; o_current->next = NULL;}}
#define searchOpcode(output, targetptr) {struct ochunk *cur = &o_root; for(size_t i = 0; i < opcode_count;) {\
		if(cur->opcodes[i%64].output == targetptr){ output = &cur->targets[i%64]; break;}\
	}}


void multisearch(const char *infile) {
	//Load infile
	char *indata;
	size_t insize = readfile(infile, (void**)&indata), pos = 0;
	size_t target_count = 0, opcode_count = 0;
	struct tchunk root;
	struct ochunk o_root;
	struct tchunk *current = &root;
	struct ochunk *o_current = &o_root;
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
		chkEOF();
		struct target ot = {0};
		if (indata[pos] != '\n' && indata[pos] != '"') {
			//target name
			//TODO: It cannot start with '\'' or contain space or ':' or only digits
			for(size_t i = 0; i < insize-pos; i++) {
				//FIXME: unicode
				//chkEOF();//EOF is impossible here
				chkEOL(i);
				//Skip until space
				char c = indata[pos + i];
				if(c != ' ' && c != '\t' && c != ':' && c != '=')
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
		chkEOF();
		if(indata[pos] == '"') {
			//path
			pos++;
			//TODO: check for relative paths
			for(size_t i = 0; i < insize-pos; i++) {
				//FIXME: unicode
				//chkEOF();
				chkEOL(i);
				//Skip until quotes
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
		if(indata[pos] == ':') {
			ot.read_only = false;
			//Prepare opcode
			pos++;
	 		pos += skipSpaces(indata + pos, insize - pos);
			struct opcode code;
			chkEOF();
			char c = indata[pos];
			code.input = NULL;
			if(c != '&' && c != '|' && c != '~' && c != '"' && c != '\'') {
				//input target
				struct stringview tar_in;
				tar_in.data = &indata[pos];
				tar_in.length = 0;
				for(size_t i = 0; i < insize-pos; i++) {
					//FIXME: unicode
					//chkEOF();//EOF is impossible here
					chkEOL(i);
					//Skip until space
					if(indata[pos + i] != ' ' && indata[pos + i] != '\t')
						continue;
					//save shit
					tar_in.length = i;
					pos += i;
					break;
				}
	 			pos += skipSpaces(indata + pos, insize - pos);
				chkEOF();
				c = indata[pos];
				//Search for target
				searchTarget(code.input, tar_in);
			}
			if(c == '&' || c == '|' || c == '~') {
				//logop
				if(c == '&') {
					if(!code.input) {
						fprintf(stderr, "Logical and without another input at offset %zu\n", pos);
						exit(-1);
					}
					code.mode = AND;
				} else if(c == '|')
					code.mode = OR;
				else if(c == '~')
					code.mode = REMOVE;
				else {
					fprintf(stderr, "Bad logic operator \"%c\" at %zu\n", c, pos);
					exit(-1);
				}

				pos++;
	 			pos += skipSpaces(indata + pos, insize - pos);
				chkEOF();
				c = indata[pos];
			} else
				code.mode = OR;
			if(c == '"' || c == '\'') {
				//search term
				code.sens = c == '\'';
				pos++;
				chkEOF();
				for(size_t i = 0; i < insize-pos; i++) {
					//FIXME: unicode
					//chkEOF();
					chkEOL(i);
					//Skip until quotes
					if(indata[pos + i] != c)
						continue;
					//save shit
					code.text.data = indata + pos;
					code.text.length = i;
					pos += i + 1;
					break;
				}
	 			pos += skipSpaces(indata + pos, insize - pos);
			} else
				fprintf(stderr, "Search term not found at offset %zu\n", pos);
			//Save target
			code.output = saveTarget(ot);
			//Save opcode
			saveOpcode(code);
		} else if (indata[pos] == '=') {
			//Load file
			ot.read_only = true;
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
			//Save target
			(void)saveTarget(ot);
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
			printf(":");
		} else
			printf("=");
		printf("\n");
		i++;
		if(!i%64)
			current = current->next;
	}
	//Create tree for multithreadig
	
	//Open archive
	//Iterate over epub
	//	Iterate over tree
	
	//Save results
	
	//Free memory
	free(indata);
	indata = NULL;
	insize = 0;
}
