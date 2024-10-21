# Fimfarchive search tools
This repository contains tools for searching fanfics from fimfiction.net
## Preparing data
1. Build search tools. `git submodule update --init && make` should be enough.
2. Download latest [fimfarchive](https://www.fimfiction.net/user/116950/Fimfarchive/blog).
3. Build metadata database from index.json of that archive. `unzip -p $ARCHIVE_PATH index.json | ./fimfar build`
## Old search
Old search system is combination of various tools from `./fimfar` such as:

`name`, `merge`, `select`, `search`, `multisearch`.

They don't need much additional space on disk or in RAM, but every full-text search with `multisearch` and especially `search` requires long time to complete.
## New search
New search uses power of existing search engine [Xapian](https://xapian.org) wrapped in [Recoll](https://www.recoll.org).

Using new search requires more additional steps and preparatory time, but results faster full-text search.
1. Install recoll of at least version 1.40.2 or latest git if it hasn't been released yet
2. Compile `rclfimf` in `recoll/` by executing `gcc -lminizip -O2 -o rclfimf rclfimf.c -I ../src/ rclcommon.c ../src/utils.c`
3. Use example configs in `recoll/config` to configure your recoll, move fimfarchive to empty directory
4. Copy `db/extra.bin` into that directory as archive and name it same, but with .zip replaced to .bin
5. Do step 4 for `db/alttag.bin` and extension .tag
6. Index with `recollindex`
