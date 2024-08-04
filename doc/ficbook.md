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
#### Authors
```
|-section
  |-[div class="hat-creator-container"]
    |-div class="creator-info"
      |-a href="/authors/$autorid" class="creator-username" itemprop="author"
      | \author name
      |-i class="small-text text-muted"
        \author type
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
```
div id="content" class="js-part-text part_text clearfix js-public-beta-text js-bookmark-area" data-is-adult="" itemprop="articleBody"
|-(text with markup)
```
