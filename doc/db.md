# Database
## Formats
### Common
All integers are in host endian.
### iddb
Id db consists of stream of uint32 numbers.
### tagdb
AoS with
1. uint32 id
2. uint32 length
3. length bytes of data
### assocdb
Association db. AoS with
1. uint32 id1
2. uint32 id2
## FS structure
1. `db/tag.bin` tagdb of tag names
2. `db/story.bin` tagdb of story names
3. `db/assoc.db` assocdb of stories and tags
4. `db/fb-tag.bin` tagdb of tag names for ficbook
5. `db/fb-author.bin` tagdb for author name on ficbook
6. `db/fb-story.bin`
### Groups
```
group
|-[$groupid]
  \[$parentfolderid.]$folderid.list
```
\*.list is text file with story id on each line
