#include "builder.h"

#include <expat.h>
#include <string.h>

#ifdef XML_LARGE_SIZE
#  define XML_FMT_INT_MOD "ll"
#else
#  define XML_FMT_INT_MOD "l"
#endif

#ifdef XML_UNICODE_WCHAR_T
#  define XML_FMT_STR "ls"
#else
#  define XML_FMT_STR "s"
#endif

//TODO: figure out translations
//NOTE: no coauthor info in search?
//NOTE: fanfic url can contain either numerical id or uuid

//articles
//|-[article]
//  |-div (why?)
//    |-h3 class="fanfic-inline-title
//    | |-a href="$link"
//    |   \title
//    |-div class="fanfic-main-info"
//      |-div class="badge-with-icon small-direction-$gen ..."
//      | |-(junk related to direction)
//      |-div class="badge-with-icon badge-rating-$rating ..."
//      | |-(junk related to rating)
//      |-(div class="badge-with-icon badge-translate")
//      | |-(junk related to translation)
//      |-div class="badge-with-icon badge-status-$status ..."
//      | |-(junk related to status)
//      |-div class="badge-with-icon badge-like ..."
//      | |-(junk related to likes)
//      |-div class="badge-with-icon badge-reward ..."
//      | |-(junk related to rewards)
//      |-div class="side-section"
//      | |-fanfic-more-dropdown :fanfic-id="$id"
//      | | |-(junk)
//      | |-(hot junk)
//      |-dl class="fanfic-inline-info mt-5"
//      | |-dt
//      | |-dd
//      |   |-span class="author word-break"
//      |   | |-a href="/authors/$authorid"
//      |   | | |-junk
//      |   |   \author name
//      |   |-(span class="author word-break" title="Автор оригинального текста")
//      |     |-a href="/translations/by_author?author=$original_author_name">
//      |       |-(junk)
//      |       \original author name
//      |-dl class="fanfic-inline-info"
//      | |-dt
//      | |-dd
//      |   |-[a href="/fanfiction/$fandomtype/$fandomnae"]
//      |     \fandom name
//      |-dl class="fanfic-inline-info"
//      | |-dd
//      |   |-[a class=" pairing-link" href="/pairings/$pairing"]
//      |     \pairing or character name
//      |-dl class="fanfic-inline-info"
//      | |-(size junk, always ignore)
//      |-dl class="fanfic-inline-info"
//      | |-(last update)
//      |-div class="tags"
//      | |-span
//      | |-[a class="tag" href="/tags/$tagid"]
//      | | \tag name
//      | |-span class="show-hidden-tags-btn js-show-hidden-tags"
//      |-div class="fanfic-description"
//        |-div class="wrap word-break urlize-links fanfic-description-text"
//          \description

//On direction:
//"Джен"/Gen - no relations
//"Гет"/Get - regular relations
//"Слэш"/Slash - gay
//"Фемслэш"/Femslash - lesbian
//"Смешанная"/Mixed - combination of above
//"Другое"/Other - can't be classified as one of above

//On rating:
//R

//On status:
//finished = complete
//in-progress = incomplete
//frozen = hiatus
//? = canceled?(does it exist or frozen can mean canceled too?)

//On pairings:
//"Luna/Celestia" means relationship
//"Luna" means character Luna
//"human!Luna" means humanized Luna
//Format: recpairing = "[$modifier!]$character[/$recpairing]"

void build_fb() {
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
}
