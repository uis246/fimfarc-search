# Description of ficbook structure

Ficbook is a big russian-speaking fanfic website

## URLs
1. `/readfic/$linkid`
Contents page, can also contain text for single-chapter fanfics
2. `/readfic/$linkid/$chapterid`
Chapter page
3. `/tags/$tagid`
4. `/pairings/$pairing`
Character or pairing
5. `/authors/$userid`
User profile

## Direction
Direction means type of relationship fanfic focuses on.
- "Джен"/Gen - no relations
- "Гет"/Get - regular relations
- "Слэш"/Slash - gay
- "Фемслэш"/Femslash - lesbian
- "Смешанная"/Mixed - combination of above
- "Другое"/Other - can't be classified as one of above
- "Статья"/Article

## Rating
- G
- R
- PG-13
- NC-17
- NC-21

## Status
Status of progress and equivalent fimfiction status
- finished = complete
- in-progress = incomplete
- frozen = hiatus

## Characters and pairings
Characters consist of modifier and name.
Format with single modifier is simple: "modifier!Name".
For example character Luna with modifier human would be "human!Luna".

Relationship notation is "character1/character2/character3/..."

## Author types
### Common
1. "бета"/beta = proofreader
2. "гамма"/gamma = editor
### In authored
1. "автор" = author
2. "соавтор" = coauthor
### In translate
1. "переводчик" = translator
2. "сопереводчик" = cotranslator

## XML/DOM trees
Example tree:
```
root
|-node 1
| |-subnode 1
|-node 2
| \character leaf
\character leaf
|-node 3
\character leaf
```
would be xml
```
<root>
<node 1>
    <subnode 1></subnode>
</node>
<node 2>character leaf</node>
character leaf
<node 3></node>
character leaf
</root>
```
### Header
```
header class="head mt-15"
|-div class="fanfic-main-info"
| |-hi
| | \title
| |-div class="mb-10"
| | |-[a href="/fanfiction/$fandom"]
| |   \fandom
| |-section
|   |-(badges similar to fanfic-main-info in fandom list)
|-section class="fanfic-hat"
  |-section
    |-div class="sticky-block"
      |-div class="fanfic-hat-body rounded-block clearfix"
        |-section
        | |-[div class="hat-creator-container"]
        |   |-div class="creator-info"
        |     |-a href="/authors/$autorid" class="creator-username" itemprop="author"
        |     | \author name
        |     |-i class="small-text text-muted"
        |       \author type
        |-div
          |-div class="description word-break"
            |-div class="mb-5"
            | |-strong
            | | \"Пэйринг и персонажи:"
            | |-div
            |   |-[a class="pairing-link pairing-highlight" href="/pairings/$pairing"]
            |     \pairing
            |-div class="mb-5"
            | |-(size junk)
            |-div class="mb-5"
            | |-strong
            | | \"Метки:"
            | |-div class="tags"
            |   |-a class="tag" href="/tags/$tagid"
            |     \tag
            |-div class="mb-5"
            | |-strong
            | | \"Описание:"
            | |-div class="urlized-links js-public-beta-description text-preline" itemprop="description"
            |   \decsription
            |-div class="mb-5"
            | |-strong
            | | \"Посвящение:"
            | |-div class="urlized-links js-public-beta-dedication text-preline"
            | | \dedication
            |-div class="mb-5"
              |-strong
              | \"Примечания:"
              |-div class="urlized-links js-public-beta-author-comment text-preline"
                \author's comment
```
### Contents
```
ul class="list-unstyled list-of-fanfic-parts clearfix"
|-[li class="part"]
| |-a href="/readfic/11115297/28590729#part\_content" class="part-link "
| | |-div class="part-title word-break"
| | | |-h3
| | |   \title
| | |-(junk)
| |-div class="part-info"
|   |-span title="$datetime"
|   | \datetime
|   |-a
|     \reviews
```
### Chapter
TODO: pre-chapter and post-chapter author notes
TODO: references
```
div id="content" class="js-part-text part_text clearfix js-public-beta-text js-bookmark-area" data-is-adult="" itemprop="articleBody"
|-(text with markup)
```
Chapter id of single-chapter fanfic can be extracted from `input name="part_id" value="$chapterid"`
### Fandom fanfic list
```
articles
|-[article]
  |-div (why?)
    |-h3 class="fanfic-inline-title
    | |-a href="/readfic/$linkid"
    |   \title
    |-div class="fanfic-main-info"
      |-div class="badge-with-icon small-direction-$gen ..."
      | |-(junk related to direction)
      |-div class="badge-with-icon badge-rating-$rating ..."
      | |-(junk related to rating)
      |-(div class="badge-with-icon badge-translate")
      | |-(junk related to translation)
      |-div class="badge-with-icon badge-status-$status ..."
      | |-(junk related to status)
      |-div class="badge-with-icon badge-like ..."
      | |-(visual junk)
      | |-span class="badge-text"
      |   \amount of likes
      |-div class="badge-with-icon badge-reward ..."
      | |-(visual junk)
      | |-span class="badge-text"
      |   \amount of rewards
      |-div class="side-section"
      | |-fanfic-more-dropdown :fanfic-id="$id"
      | | |-a
      | | | |-picture
      | | |   |-source srcset="$cover" (small?)
      | | |   |-source srcset="$cover" (big?)
      | | |-(junk)
      | |-(hot junk)
      |-dl class="fanfic-inline-info mt-5"
      | |-dt
      | |-dd
      |   |-span class="author word-break"
      |   | |-a href="/authors/$authorid"
      |   | | |-junk
      |   |   \author name
      |   |-(span class="author word-break" title="Автор оригинального текста")
      |     |-a href="/translations/by_author?author=$original_author_name">
      |       |-(junk)
      |       \original author name
      |-dl class="fanfic-inline-info"
      | |-dt
      | |-dd
      |   |-[a href="/fanfiction/$fandomtype/$fandomnae"]
      |     \fandom name
      |-dl class="fanfic-inline-info"
      | |-dd
      |   |-[a class=" pairing-link" href="/pairings/$pairing"]
      |     \pairing or character name
      |-dl class="fanfic-inline-info"
      | |-(size junk, always ignore)
      |-dl class="fanfic-inline-info"
      | |-(last update)
      |-div class="tags"
      | |-span
      | |-[a class="tag" href="/tags/$tagid"]
      | | \tag name
      | |-span class="show-hidden-tags-btn js-show-hidden-tags"
      |-div class="fanfic-description"
        |-div class="wrap word-break urlize-links fanfic-description-text"
          \description
```
