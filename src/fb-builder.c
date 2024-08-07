#include "builder.h"

#include <expat.h>

#include <assert.h>
#include <unistd.h>

#ifdef XML_LARGE_SIZE
#  define XML_FMT_INT_MOD "ll"
#else
#  define XML_FMT_INT_MOD "l"
#endif

#ifdef XML_UNICODE_WCHAR_T
#  error "Your libexpat build encodes unicode characters in wchar, which is not supported by build-fb for now"
#  define XML_FMT_STR "ls"
#else
#  define XML_FMT_STR "s"
#endif

//TODO: figure out translations
//TODO: link to original
//NOTE: no coauthor info in search?
//NOTE: fanfic url can contain either numerical id or uuid



struct parser {
	XML_Parser p;
	enum {
		Reset = 0,//Outside of story entry
		Story,//Story with id
		Title,//Story title and linkid
		MainInfo,
		Likes,//Content will be likes
		Rewards,//Content will be rewards
		Tags,//Tags of story
		TagInfo,//id and name of tag
		Junk,//I'm not interested in this
	} state;//State machine

	uint8_t current_depth, junk_depth;
	uint32_t story_id, likes, rewards, pages;
	struct stringbuf linkid, title;
};

static const XML_Char* getAtt(const XML_Char **atts, const char *att) {
	for(size_t i = 0; atts[i]; i+=2) {
		if(strcmp(att, atts[i]) != 0)
			continue;
		else
			return atts[i + 1];
	}
	return NULL;
}

//return true to stop processing tokens
typedef bool (*forEachCallback)(struct parser *const restrict p, size_t len, const char start[restrict static len]);

static void forEachTok(struct parser *const restrict p, const XML_Char *in, const char delim, forEachCallback cb) {
	const size_t totallen = strlen(in);
	const char *const end = in + totallen;
	for(const char *cursor = in; *cursor;) {
		//skip empty substrings
		for(; *cursor == delim; cursor++);
		//check if we still didn't hit end
		if(!*cursor)
			break;
		const char *next = strchr(cursor + 1, delim);
		if(!next)
			next = end;
		if(cb(p, next - cursor, cursor))
			break;
		cursor = next;
	}
}

static bool divClassesMain (struct parser *const restrict p, size_t n, const char token[restrict static n]) {
	if(strncmp(token, "side-section", n) == 0) {
		p->junk_depth = 0;
	} else if(strncmp(token, "badge-like", n) == 0) {
		p->junk_depth = 0;
		p->state = Likes;
	} else if(strncmp(token, "badge-reward", n) == 0) {
		p->junk_depth = 0;
		p->state = Rewards;
	} else
		return false;
	return true;
}

static bool maininfoParse(struct parser *const restrict p, const XML_Char *name, const XML_Char **atts) {
	uint8_t depth = p->current_depth - 1;
	if(depth == 4) {
		if(strcmp(name, "div") == 0) {
			//get class info
			const XML_Char *class;
			class = getAtt(atts, "class");
			if(!class)
				return true;
			p->junk_depth = 1;
			//iterate over classes
			forEachTok(p, class, ' ', divClassesMain);
		} else if(strcmp(name, "dl") == 0) {
			//no need in class info
			//not marking as junk
		} else
			p->junk_depth++;
	} else if(depth == 5) {
		//we are in one of dls
		if(strcmp(name, "fanfic-more-dropdown") == 0) {
			const char *fid;
			char *endptr;
			fid = getAtt(atts, ":fanfic-id");
			if(!fid)
				return true;
			long long id;
			id = strtoll(fid, &endptr, 10);
			if(*endptr || id > UINT32_MAX)
				return true;
			p->story_id = id;
		}
		p->junk_depth++;
	}
	return false;
}

static void XMLCALL startElement(void *userData, const XML_Char *name, const XML_Char **atts) {
	struct parser *const parser = userData;

	//check max depth
	uint8_t depth = parser->current_depth++;
	if(depth > 32)
		//Bad input?
		goto stop;
	if(parser->junk_depth) {
		parser->junk_depth++;
		return;
	}
	switch(parser->state) {
		case Reset:
			if(depth > 1)
				goto stop;
			if(strcmp(name, "article") == 0) {
				//parser operated under assumption that article node is child of root(<articles>) node
				//stop parsing if this is not true
				if(depth != 1)
					goto stop;
				parser->state = Story;
			}
			break;
		case Story:
			if(depth == 2)
				;
			else if(depth == 3 && strcmp(name, "h3") == 0)
				parser->state = Title;
			else if(depth == 3 && strcmp(name, "div") == 0)
				parser->state = MainInfo;
			else
				goto stop;
			break;
		case MainInfo:
			if(maininfoParse(parser, name, atts))
				goto stop;
			break;
		case Title:
			if(strcmp(name, "a") == 0) {
				//get linkid
				const XML_Char *href;
				href = getAtt(atts, "href");
				if(!href)
					//what?
					goto stop;
				if(strncmp("/readfic/", href, 9) != 0)
					goto stop;
				const char *ref = href + 9, *eref = strrchr(ref, '?');
				if(eref)
					strmemtobuf(&parser->linkid, ref, eref - ref);
				else
					strtobuf(&parser->linkid, ref);
			} else
				//huh?
				goto stop;
			break;
		case Likes:
		case Rewards:
			//noop
			break;
	}
	return;

	stop:
	fprintf(stderr, "Stopped parser at depth %u\n", depth);
	XML_StopParser(parser->p, XML_FALSE);
}

static struct stringbuf tmpcharbuf = {0};

static void XMLCALL endElement(void *userData, const XML_Char *name) {
	struct parser *const parser = userData;
	(void)name;

	uint8_t depth = parser->current_depth--;
	// depth = previous depth
	if(parser->junk_depth)
		parser->junk_depth--;
	else {
		//articles d=1 Reset->Reset
		//|-article d=2 Story->Reset
		//  |-div d=3 Story->Story
		//    |-h3 d=4 Title->Story
		//    |-div d=4 MainInfo->Story
		//      |-div d=5 Likes->MainInfo
		//      |-div d=5 Rewards->MainInfo
		//do state machine thing
		uint8_t state = parser->state;
		assert(!(state == Reset && depth != 1));
		assert(!(state == Story && (depth > 3 || depth < 2)));
		switch(state) {
		case Story:
			if(depth == 2) {
				//should I check for incomplete info?
				//emit story info
				printf("%"PRIu32" %s %.*s\n", parser->story_id, parser->linkid.data, parser->title.length - 1, parser->title.data);
				//move to reset state
				parser->state = Reset;
				//reset parsed story?
				parser->likes = 0;
				parser->rewards = 0;
				parser->pages = 0;
				parser->title.length = 0;
			}
			break;
		case MainInfo:
		case Title:
			if(depth == 4)
				parser->state = Story;
			break;
		case Likes:
		case Rewards:
			if(depth == 5) {
				parser->state = MainInfo;
				uint32_t result = (uint32_t)atol(tmpcharbuf.data);
				if(state == Likes)
					parser->likes = result;
				else
					parser->rewards = result;
				tmpcharbuf.length = 0;
			}
			break;
		case TagInfo:
			//emit tag info
			//...
			parser->state = Tags;
			break;
		}
	}
}

static void charData(void *userData, const XML_Char *s, int len) {
	struct parser *const p = userData;
	if(len == 1 && s[0] == '\n')
		s = " ";
	switch(p->state) {
		case Title:
			if(len == 1 && s[0] == ' ' && p->title.length == 0)
				break;
			strmembufappend(&p->title, s, len);
			break;
		case TagInfo:
			break;
		case Likes:
		case Rewards:
			if(len == 1 && s[0] == ' ' && tmpcharbuf.length == 0)
				break;
			strmembufappend(&tmpcharbuf, s, len);
			break;
		default:
			//just noop
			break;
	}
}

static int parsebuf(XML_Parser parser, int len, int done) {
	if (XML_ParseBuffer(parser, (int)len, done) == XML_STATUS_ERROR) {
		fprintf(stderr,
			"Parse error at line %" XML_FMT_INT_MOD "u:\n%" XML_FMT_STR "\n",
			XML_GetCurrentLineNumber(parser),
			XML_ErrorString(XML_GetErrorCode(parser)));
		return 1;
	}
	return 0;
}

void builder_fb() {
	struct parser parsestate = {0};
	if (isatty(fileno(stdin))) {
		fprintf(stderr, "Stdin is not a pipe. You should pipe all fandom pages into build-fb.\n");
		return;
	}
	XML_Parser parser = XML_ParserCreate("UTF-8");
	parsestate.p = parser;
	int done;

	if (!parser) {
		fprintf(stderr, "Couldn't allocate memory for parser\n");
		return;
	}

	XML_SetUserData(parser, &parsestate);
	XML_SetElementHandler(parser, startElement, endElement);
	XML_SetCharacterDataHandler(parser, charData);

	XML_Parse(parser, "<articles>", strlen("<articles>"), 0);
	do {
		void *const buf = XML_GetBuffer(parser, BUFSIZ);
		if (!buf) {
			fprintf(stderr, "Couldn't allocate memory for buffer\n");
			XML_ParserFree(parser);
			return;
		}

		const size_t len = fread(buf, 1, BUFSIZ, stdin);

		if (ferror(stdin)) {
			fprintf(stderr, "Read error\n");
			XML_ParserFree(parser);
			return;
		}

		done = feof(stdin);

		done |= parsebuf(parser, (int)len, 0);
	} while (!done);
	XML_Parse(parser, "</articles>", strlen("</articles>"), 1);
	XML_ParserFree(parser);

	free(parsestate.linkid.data);
	free(parsestate.title.data);
	//emit output db
}
