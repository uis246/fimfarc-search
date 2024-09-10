#include "fimfar.h"
#include "builder.h"

#include <assert.h>
#include <pdjson.h>

#include <unistd.h>
#include <time.h>

#define STATE_REPORT 0
#define PROGRESS_REPORT 0
#define BUILD_EXTRA 1

#if STATE_REPORT
static const char *StateName[] =
{"Reset", "Story", "Tags", "TagInfo", "Archive", "Junk"};
#endif

// extra db from story state:
// publishing time - s(t) +
// update time - s(t) +
// completion status - s(i) +
// content rating - s(i) +
// long description - s +
// short description - s +
// likes - i +
// dislikes - i +
// comments - i +
// views - i +
// tags - i[] +

#if BUILD_EXTRA
struct exl {
	struct extra_leaf d;
	struct stringbuf ld, sd, tags;
};
//=3*8+2*8+3*2=5*8+6=46
static uint64_t timeStringParse(const char *in) {
	struct tm t;
	strptime(in, "%Y-%m-%dT%H:%M:%S.%f%z", &t);
	return mktime(&t);
}
#endif



static int tag_sort(const void *a, const void *b) {
	const struct tag *A = a, *B = b;
	return A->id - B->id;
};

void builder() {
	char prev[32];
	struct json_stream s[1];
	struct tag tag = {{0}, 0}, shtag = {{0}, 0};
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

		uint32_t story_id;
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
	#if BUILD_EXTRA
	FILE *extra_bin, *alttag_bin;
	extra_bin = fopen(EXTRA_PATH, "wb");
	alttag_bin = fopen(ALTTAG_PATH, "wb");
	struct stringbuf alttagbuf = {0};
	struct exl el = {0};
	#endif
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
					uint32_t l;
					//Exclude bullshit stories
					//TODO: do not hardcode excluded stories
					if(parser.story_id != 221463 && /*parser.story_id != 234296 &&*/ parser.story_id != 363931) {
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
					#if BUILD_EXTRA
					//ldlen, sdlen, tagsz, skipbytes
					if(el.ld.length > UINT16_MAX) {
						printf("Story %"PRIu32" description is too long\n", parser.story_id);
						el.ld.length = 0;
					}
					if(el.sd.length > UINT16_MAX) {
						printf("Story %"PRIu32" short description is too long\n", parser.story_id);
						el.ld.length = 0;
					}
					if(el.tags.length > UINT16_MAX) {
						printf("Story %"PRIu32" has too many tags\n", parser.story_id);
						el.ld.length = 0;
					}
					el.d.ldlen = el.ld.length;
					el.d.sdlen = el.sd.length;
					el.d.tagsz = el.tags.length;
					l = EXL_SIZE + el.ld.length + el.sd.length + el.tags.length;
					el.d.skipbytes = EXL_SIZE + el.d.ldlen + el.d.sdlen + el.d.tagsz;
					if(el.d.skipbytes == 0 || l >= UINT16_MAX) {
						fprintf(stderr, "Can't write story %u, too long\n", itf->id);
						exit(-1);
					}
					//write
					fwrite(&el, EXL_SIZE, 1, extra_bin);
					fwrite(el.ld.data, el.d.ldlen, 1, extra_bin);
					fwrite(el.sd.data, el.d.sdlen, 1, extra_bin);
					//TODO: presort tags
					fwrite(el.tags.data, el.d.tagsz, 1, extra_bin);
					//reset
					el.ld.length = 0;
					el.sd.length = 0;
					el.tags.length = 0;
					#endif
					#if PROGRESS_REPORT
					//Show progress
					uint32_t tmp = parser.story_id/2000*2000;
					if(tmp > milestone) {
						printf("Current story: %"PRIu32"\n", parser.story_id);
						milestone = tmp;
					}
					//printf("Got story %"PRIu32" with title \"%s\"\nAccesable by path: %s\n", parser.story_id, parser.title.data, parser.path.data);
					#endif
					}
					parser.state = Reset;
				} else if(parser.state == Archive)
					parser.state = Story;
				else if(parser.state == TagInfo) {
					//Push tag info
					tagtobuf(&tagbuf, &tag);
					tagtobuf(&alttagbuf, &shtag);
					//Add tag to story
					struct story_tag_file st;
					st.story_id = parser.story_id;
					st.tag_id = tag.id;
					fwrite(&st, STF_SIZE, 1, assoc_bin);
					#if BUILD_EXTRA
					bufappend(&el.tags, &tag.id, sizeof(tag.id));
					#endif
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
					else if(strcmp(prev, "content_rating") == 0) {
						if(strcmp(value, "everyone") == 0)
							el.d.cr = EVERYPONE;
						else if(strcmp(value, "teen") == 0)
							el.d.cr = TEEN;
						else if(strcmp(value, "mature") == 0)
							el.d.cr = MATURE;
						else
							printf("cr: %s\n", value);
					} else if(strcmp(prev, "completion_status") == 0) {
						if(strcmp(value, "incomplete") == 0)
							el.d.complete = INCOMPLETE;
						else if(strcmp(value, "complete") == 0)
							el.d.complete = COMPLETE;
						else if(strcmp(value, "hiatus") == 0)
							el.d.complete = HIATUS;
						else if(strcmp(value, "cancelled") == 0)
							el.d.complete = CANCELLED;
						else
							printf("cs: %s\n", value);
					} else if(strcmp(prev, "date_published") == 0)
						el.d.ctime = timeStringParse(value);
					else if(strcmp(prev, "date_updated") == 0)
						el.d.mtime = timeStringParse(value);
					else if(strcmp(prev, "description_html") == 0)
						strtobuf(&el.ld, value);
					else if(strcmp(prev, "short_description") == 0)
						strtobuf(&el.sd, value);
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
					if(strcmp(prev, "id") == 0) {
						tag.id = strtoul(value, NULL, 10);
						shtag.id = tag.id;
					} else if(strcmp(prev, "name") == 0)
						strtobuf(&tag.name, value);
					else if(strcmp(prev, "url") == 0)
						strtobuf(&shtag.name, strrchr(value, '/') + 1);
				}
				if(top == 1 && parser.state == Reset) {
					//We can fill story id later, but let's do it now
					uint32_t id = strtoul(value, NULL, 10);
					assert(id > parser.story_id);
					parser.story_id = id;
					el.d.id = id;
					prev[0]=0;
				}
				break;
			case JSON_NUMBER:
				if(parser.state == Junk)
					continue;
				value = json_get_string(s, NULL);
				if(parser.state == Story && got_key) {
					uint32_t id = strtoul(value, NULL, 10);
					if(strcmp(prev, "id") == 0) { 
						//TODO: replace with assert?
						if(id != parser.story_id)
							dprintf(2, "++ID MISMATCH! %u!=%u++\n", parser.story_id, id);
					}
					#if BUILD_EXTRA
					else if(strcmp(prev, "num_likes") == 0)
						el.d.likes = id;
					else if(strcmp(prev, "num_dislikes") == 0)
						el.d.dislikes = id;
					else if(strcmp(prev, "num_views") == 0)
						el.d.views = id;
					else if(strcmp(prev, "num_comments") == 0)
						el.d.comments = id;
					#endif
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
	free(story.data);
	free(parser.path.data);
	free(parser.title.data);
	free(tag.name.data);
	free(el.ld.data);
	free(el.sd.data);
	free(el.tags.data);
	json_reset(s);
	json_close(s);
	//Sort tags
	milestone = tagbuf.length/sizeof(tag);
	qsort(tagbuf.data, milestone, sizeof(tag), tag_sort);
	qsort(alttagbuf.data, milestone, sizeof(tag), tag_sort);
	writeAndFreeTags(tag_bin, &tagbuf);
	writeAndFreeTags(alttag_bin, &alttagbuf);
}
