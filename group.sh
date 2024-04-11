#!/bin/sh

#Print call info or usage
if [[ $# -ne 3 ]]; then
	echo "Usage: $0 GROUP_ID FOLDER_ID PAGES"
	exit -1
else
	echo Loading stories from group...
	echo Group: $1
	echo Folder: $2
	echo Pages: $3
fi

#Scrape
if ! command -v aria2c &> /dev/null; then
	echo Aria2 not found, using curl
	for i in $(seq 1 $3); do
		curl "https://www.fimfiction.net/group/$1/folder/$2/?page=$i" | grep -Eo '/story/[^/"]+' | grep -v '-' | sed 's/\/story\///' > /tmp/group.$$.$i
done
else
	echo Aria2 found
	for i in $(seq 1 $3); do
		printf "https://www.fimfiction.net/group/$1/folder/$2/?page=$i\n\tout=load.$$.$i\n" >> /tmp/aria2.$$
	done

	aria2c -i /tmp/aria2.$$ -d /tmp --enable-http-pipelining true
	rm /tmp/aria2.$$

	for i in $(seq 1 $3); do
		grep -Eo '/story/[^/"]+' /tmp/load.$$.$i | grep -v '-' | sed 's/\/story\///' >> /tmp/group.$$
		rm /tmp/load.$$.$i
	done
fi

#cat /tmp/group.$$.* > /tmp/group.$$

#Save to binaries
mkdir -p group
./fimfar load group/$1_$2.bin /tmp/group.$$

rm /tmp/group.$$
