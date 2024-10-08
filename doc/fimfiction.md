# Description of ficbook structure

Ficbook is a big russian-speaking fanfic website

## URLs
1. `/group/$groupid/$groupname/folders`
List of folders in group. Group name in url doesn't seem to matter.
2. `/group/$groupid/folder/$folderid/$foldername`
Folder or subfolder. I guess tree structure should be extracted from crawling.
3. `/groups?page=$page`
List of all groups. Note, that some might be hidden beghind mature.

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

### Folders list
```
div class="folder_lsit"
|-table
  |-thdead
  | |-...
  |-tbody
    |-[tr]
      |-td
      | |-a href="$folderurl"
      | | \$foldername
      | |-br (if there are subfolders)
      | |-[a class="folder" href="$subfolderurl"]
      |   |-l ...
      |   \$subfoldername
      |-td class="count"
        \$count
```

### Folder
```
div data_group_id="$groupid" data-folder-id="$folderid" data-controller-id="$noidea"
|-div class="context_box folder-description" (optional)
| \(not interested)
|-div class="folder_list" (optional?)
| |-(see folder list)
|-div class=""
  |-div class="paginator-container" ...
  | |-(junk)
  |-div class="list_boxes"
  | |-ul ...
  |   |-[li]
  |     |-div id="story-class-container-$storyid" class="story-class-container" data-story-id="$storyid" ...
  |       |-(not interested)
  |-div class="folder-paginator" ...
    |-(junk)
```

### Group list
```
div data-tab="groups" ...
|-ul class="group-card-list"
| |-[li]
|   |-a href="/group/$groupid/$grname"
|   | |-img ...
|   | |-div class="group-name"
|   | | \$groupname
|   | |-(not interested)
|   |-(not interested)
|-div class="page_list"
  |-ul
    |-[li ...]
      |-(too lazy, but it has number of pages in group list)
```
