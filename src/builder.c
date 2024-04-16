#include "fimfar.h"

#include <assert.h>
#include <pdjson.h>

#include <sys/mman.h>
#include <unistd.h>

#define STATE_REPORT 0
#define PROGRESS_REPORT 0

#if STATE_REPORT
static const char *StateName[] =
{"Reset", "Story", "Tags", "TagInfo", "Archive", "Junk"};
#endif


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

static int tag_sort(const void *a, const void *b) {
	const struct tag *A = a, *B = b;
	return A->id - B->id;
};

void builder() {
	char prev[32];
	struct json_stream s[1];
	struct tag tag = {{0}, 0};
	//Writer data
	struct stringbuf tagbuf = {0}, story = {0};//result

	FILE *f, *story_bin, *tag_bin, *assoc_bin;
	const char *value;
	uint32_t milestone=0;
	unsigned int top = 0;
	enum json_type type;
	bool got_key = false;
	struct {
		enum {
			Reset = 0,//Outside of story entry
			Story,//Story with id
			Tags,//Tags of story
			TagInfo,//id and name of tag
			Archive,//Archival metadata
			Junk,//I'm not interested in this
		} state;//State machine

		unsigned int story_id;
		struct stringbuf path, title;
	} parser = {0};
	//f = fdopen(0, "rb");
	f = stdin;
	if (isatty(fileno(f))) {
		fprintf(stderr, "Stdin is not a pipe. You should pipe index.json into builder.\n");
		return;
	}
	story_bin = fopen(STORY_PATH, "wb");
	tag_bin = fopen(TAG_PATH, "wb");
	assoc_bin = fopen(ASSOC_PATH, "wb");
	//FIXME: check fopen for errors
	json_open_stream(s, f);
	json_set_streaming(s, 1);
	
	do {
		type = json_next(s);
		switch(type) {
			case JSON_OBJECT:
				if(parser.state == Tags)
					parser.state = TagInfo;
				else if(parser.state == Story) {
					assert(top>1);
					if(strcmp(prev, "archive") == 0)
						parser.state = Archive;
					else
						parser.state = Junk;
				} else if(parser.state == Reset && top == 1) {
					parser.state = Story;
					top++;
					got_key=false;
					continue;
				}
				// fall through
			case JSON_ARRAY:
				if(parser.state == Story) {
					if(strcmp(prev, "tags") == 0)
						parser.state = Tags;
					else
						parser.state = Junk;
				}
				top++;
				got_key=false;
				if(parser.state != Story)
					continue;
				else
					break;
			case JSON_OBJECT_END:
				if(parser.state == Junk && top == 3) {
					parser.state = Story;
				} else if(parser.state == Story) {
					//Push story
					struct id_text_file *itf;
					story.length = 4 + 1 + parser.title.length + (STORE_PATH ? parser.path.length : 0) - 1;
					bufalloc(&story);
					itf = (struct id_text_file*)story.data;
					itf->id = parser.story_id;
					itf->length = parser.title.length + (STORE_PATH ? parser.path.length : 0) - 1;
					memcpy(itf->data, parser.title.data, parser.title.length - 1 + STORE_PATH);
					#if SOTRE_PATH
					memcpy(itf->data + parser.title.length, parser.path.data, parser.path.length - 1);
					#endif
					fwrite(itf, ITF_SIZE + itf->length, 1, story_bin);
					#if PROGRESS_REPORT
					//Show progress
					uint32_t tmp = parser.story_id/2000*2000;
					if(tmp > milestone) {
						printf("Current story: %"PRIu32"\n", parser.story_id);
						milestone = tmp;
					}
					//printf("Got story %"PRIu32" with title \"%s\"\nAccesable by path: %s\n", parser.story_id, parser.title.data, parser.path.data);
					#endif
					parser.state = Reset;
				} else if(parser.state == Archive)
					parser.state = Story;
				else if(parser.state == TagInfo) {
					//Push tag info
					tagtobuf(&tagbuf, &tag);
					//Add tag to story
					struct story_tag_file st;
					st.story_id = parser.story_id;
					st.tag_id = tag.id;
					fwrite(&st, STF_SIZE, 1, assoc_bin);
					parser.state = Tags;
				}
				//
				top--;
				if(parser.state != Story)
					continue;
				else
					break;
			case JSON_ARRAY_END:
				assert(parser.state != Story);
				if(parser.state == Junk && top == 3)
					parser.state = Story;
				else if(parser.state == Tags)
					parser.state = Story;
				top--;
				continue;
			case JSON_STRING:
				if(parser.state == Junk)
					continue;//Skip junk

				value = json_get_string(s, NULL);
				if(parser.state == Story && got_key) {
					if(strcmp(prev, "title") == 0)
						strtobuf(&parser.title, value);
					prev[0]=0;
					got_key=false;
					continue;
				} else if(parser.state == Story && !got_key)
					got_key=true;

				strncpy(prev, value, 32);

				if(parser.state == Archive) {
					//Already have key
					//Read value
					type = json_next(s);
					if(type == JSON_STRING && strcmp(prev, "path") == 0)
						strtobuf(&parser.path, value);
				} else if(parser.state == TagInfo) {
					type = json_next(s);
					assert(type == JSON_STRING || type == JSON_NUMBER);
					if(strcmp(prev, "id") == 0)
						tag.id = strtoul(value, NULL, 10);
					else if(strcmp(prev, "name") == 0)
						strtobuf(&tag.name, value);
				}
				if(top == 1 && parser.state == Reset) {
					//We can fill story id later, but let's do it now
					uint32_t id = strtoul(value, NULL, 10);
					assert(id > parser.story_id);
					parser.story_id = id;
					prev[0]=0;
				}
				break;
			case JSON_NUMBER:
				if(parser.state == Junk)
					continue;
				value = json_get_string(s, NULL);
				if(parser.state == Story && got_key) {
					if(strcmp(prev, "id") == 0) { 
						uint32_t id = strtoul(value, NULL, 10);
						//TODO: replace with assert?
						if(id != parser.story_id)
							dprintf(2, "++ID MISMATCH! %u!=%u++\n", parser.story_id, id);
					}
				}
				got_key = false;
				break;
			default:
				got_key=false;
		}
		#ifndef NDEBUG
		assert(!(parser.state == Story && top != 2));
		if(top == 1 && type == JSON_STRING) {
			assert(parser.state != Junk);
			//Entering the story
			if(parser.state != Reset) {
				dprintf(2, "FSM malfunction, l: %u\n", top);
				return;
			}
		}
		#endif
		#if STATE_REPORT
		printf("L %u, t %u, s %s\n", top, type, StateName[parser.state]);
		#endif
	} while(type != JSON_DONE && type != JSON_ERROR);
	//termination:
	free(parser.path.data);
	free(parser.title.data);
	free(tag.name.data);
	json_reset(s);
	json_close(s);
	//Sort tags
	milestone = tagbuf.length/sizeof(tag);
	qsort(tagbuf.data, milestone, sizeof(tag), tag_sort);
	//Write tags
	for(uint32_t i = 0; i < milestone; i++) {
		struct id_text_file *itf;
		struct tag *tag = (struct tag*)tagbuf.data;
		itf = alloca(ITF_SIZE + tag[i].name.length - 1);
		itf->id = tag[i].id;
		itf->length = tag[i].name.length - 1;
		memcpy(&itf->data, tag[i].name.data, itf->length);
		fwrite(itf, 1, ITF_SIZE + itf->length, tag_bin);
	}
	//Free tags
	for(uint32_t i = 0; i < milestone; i++) {
		free(((struct tag*)tagbuf.data)[i].name.data);
	}
	tagfreebuf(&tagbuf);
}

