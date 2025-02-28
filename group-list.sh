#!/bin/sh

#Print call info or usage
if [[ $# -ne 0 ]]; then
	echo "Usage: $0"
	exit -1
else
	echo Loading stories from group...
	#echo Pages: $1
fi

function curlget() {
	/home/uis/git/curl-impersonate/curl-8.1.1/src/curl-impersonate-chrome --ciphers 'TLS_AES_128_GCM_SHA256,TLS_AES_256_GCM_SHA384,TLS_CHACHA20_POLY1305_SHA256,ECDHE-ECDSA-AES128-GCM-SHA256,ECDHE-RSA-AES128-GCM-SHA256,ECDHE-ECDSA-AES256-GCM-SHA384,ECDHE-RSA-AES256-GCM-SHA384,ECDHE-ECDSA-CHACHA20-POLY1305,ECDHE-RSA-CHACHA20-POLY1305,ECDHE-RSA-AES128-SHA,ECDHE-RSA-AES256-SHA,AES128-GCM-SHA256,AES256-GCM-SHA384,AES128-SHA,AES256-SHA' -H 'sec-ch-ua: "Chromium";v="116", "Not)A;Brand";v="24", "Google Chrome";v="116"' -H 'sec-ch-ua-mobile: ?0' -H 'sec-ch-ua-platform: "Windows"' -H 'Upgrade-Insecure-Requests: 1' -H 'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36' -H 'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7' -H 'Sec-Fetch-Site: none' -H 'Sec-Fetch-Mode: navigate' -H 'Sec-Fetch-User: ?1' -H 'Sec-Fetch-Dest: document' -H 'Accept-Encoding: gzip, deflate, br' -H 'Accept-Language: en-US,en;q=0.9' --http2 --http2-no-server-push --compressed --tlsv1.2 --alps --tls-permute-extensions --cert-compression brotli -b cookiejar -c cookiejar -L $1
	return $?
}

#Check first page
mkdir -p group/list
if ! [ -f "group/list/1" ]; then
	curlget "https://www.fimfiction.net/groups -o group/list/1 -f --remove-on-error"
fi

#Get page count
count=$(hxnormalize -x -i 0 < group/list/1 | hxselect div.page_list 'a::attr(href)' -c -s '\n' | awk '{max = $2 > max ? $2 : max} END {print max}' 'FS==')
if [ "2" -lt "$count" ]; then
	echo "Pages: $count"
else
	echo "No pages? 2 < $count?"
	exit -1
fi

tmpfile="$(mktemp -d)"

#Scrape group list
if ! [ -f "group/list/.scraped" ]; then
	#Prepare download list
	for i in $(seq 2 $count); do
		printf "url=https://www.fimfiction.net/groups?page=$i\noutput=group/list/$i\n" >> "$tmpfile/1"
	done
	curlget "-f --fail-early --remove-on-error -Z --rate 40/m --parallel-max 6 -K $tmpfile/1"
	if [ $? -ne 0 ]; then
		exit -1
	fi
	touch group/list/.scraped
else
	echo Group list skipped
fi

#Extract group ids
mkdir -p group/root
if ! [ -f "group/group-names" ]; then
	#rm -f "group/group-names"
	for i in $(seq 1 $count); do
		while read -r line; do
			id=$(echo "$line" | hxselect 'a::attr(href)' -c | awk '{print $3}' 'FS=/')
			echo "$id \"$(echo "$line" | hxselect 'a::attr(title)' -c)\"" >> "group/group-names"
		done < <(hxnormalize -x -i 0 -l 1024 < "group/list/$i" | hxselect ul.group-card-list div.group-name a -s '\n')
	done
fi
if ! [ -f "group/root/.scraped" ]; then
	#rm "$tmpfile"
	#touch "$tmpfile/2"
	for i in $(seq 1 $count); do
		for j in $(hxnormalize -x -i 0 < "group/list/$i" | hxselect ul.group-card-list div.group-name 'a::attr(href)' -c -s '\n' | awk '{print $3}' 'FS=/'); do
			if ! [ -f "group/root/$j" ] && [ "$j" -ne "205729" ] && [ "$j" -ne "206283" ]; then
				printf "url=https://www.fimfiction.net/group/$j/noname/folders\noutput=group/root/$j\n" >> "$tmpfile/2"
			fi
		done
		#hxnormalize -x -i 0 < group/list/$i | hxselect ul.group-card-list div.group-name 'a::attr(href)' -c -s '\n' | awk '{print "url=https://fimfiction.net"$0"/folders\noutput=group/root/"$3}' 'FS=/' >> "$tmpfile"
	done
	curlget "-f --fail-early --remove-on-error --rate 40/m -Z --parallel-max 10 -K $tmpfile/2"
	#curlget "-f --fail-early --remove-on-error --rate 40/m -v -K $tmpfile/2"
	if [ $? -ne 0 ]; then
		exit -1
	fi
	touch group/root/.scraped
else
	echo Root folders skipped
fi

#Extract(seed) folders
mkdir -p group/folder
if ! [ -f "group/folder/.scraped" ]; then
	#rm "$tmpfile"
	touch "$tmpfile/3"
	if ! [ -f "group/folder/.folders" ]; then
	rm -f group/folder/.names
	for f in group/root/*; do
		#gid="$(echo "$f" | awk '{print $3}' 'FS=/')"
		prepared="$(hxnormalize -x -i 0 -l 2048 < $f | hxselect div.folder_list tbody td a -s '\n' | hxremove a.folder | tr -s '\n')"
		#ids="$()"
		if [ "$prepared" != "" ]; then
			while read -r line; do
				id=$(echo "$line" | hxselect 'a::attr(href)' -c | awk '{print $5}' 'FS=/')
				echo "$id \"$(echo "$line" | hxselect a -c)\"" >> group/folder/.names
				printf "${f:11} $id 0\n" >> "group/folder/.folders"
			done <<< "$prepared"
		fi
	done
	fi
	while read -r gid id parent; do
		if ! [ -f "group/folder/$gid.$id" ]; then
			printf "url=https://www.fimfiction.net/group/$gid/folder/$id/\noutput=group/folder/$gid.$id\n" >> "$tmpfile/3"
		fi
	done < "group/folder/.folders"
	curlget "-f --fail-early --remove-on-error --rate 40/m -Z --parallel-max 10 -K $tmpfile/3"
	#curlget "-f --fail-early --remove-on-error --rate 40/m -v -K $tmpfile/3"
	if [ $? -ne 0 ]; then
		exit -1
	fi
	touch "group/folder/.scraped"
else
	echo Skipped seeding
fi
#so, why do I have separate seeding code?

#Extract subfolders func
function subflist() {
	#$1 - in
	#$2 - out
	mkdir -p $2
	if ! [ -f "$2/.scraped" ]; then
		rm -f "$tmpfile/4"
		touch "$tmpfile/4"
		if ! [ -f "$2/.folders" ]; then
			rm -f "$1/.pages" "$2/.names"
			for f in $1/*; do
				norm="$(hxnormalize -x -i 0 -l 2048 < $f)"
				tmp="$(echo "$f" | awk '{print $3}' 'FS=/')"
				gid="$(echo "$tmp" | awk '{print $1}' 'FS=.')"
				parent="$(echo "$tmp" | awk '{print $2}' 'FS=.')"
				prepared="$(hxselect div.folder_list tbody td a -s '\n' <<< "$norm" | hxremove a.folder | tr -s '\n')"
				if [ "$prepared" != "" ]; then
				while read -r line; do
					id=$(echo "$line" | hxselect 'a::attr(href)' -c | awk '{print $5}' 'FS=/')
					if [ "$id" != "" ]; then
						printf "$gid $id $parent\n" >> "$2/.folders"
						echo "$id \"$(echo "$line" | hxselect a -c)\"" >> "$2/.names"
					fi
				done <<< "$prepared"
				fi
				num="$(echo "$norm" | hxselect div.page_list 'a::attr(href)' -c -s '\n' | awk '{max = $2 > max ? $2 : max} END {print max}' 'FS==')"
				if [ "$num" != "" ]; then
					echo "$gid $parent $num" >> "$1/.pages"
				fi
			done
		fi
		while read -r gid id parent; do
			if ! [ -f "$2/$gid.$id" ]; then
				printf "url=https://www.fimfiction.net/group/$gid/folder/$id/\noutput=$2/$gid.$id\n" >> "$tmpfile/4"
			fi
		done < "$2/.folders"
		curlget "-f --fail-early --remove-on-error --rate 45/m -Z --parallel-max 10 -K $tmpfile/4"
		ret=$?
		if [ $ret -eq 0 ]; then
			touch "$2/.scraped"
		else
			echo Stopping
			exit
		fi
	else
		echo "Skipped $2"
	fi
}
#subflist group/root group/folder
subflist group/folder group/folder-1
subflist group/folder-1 group/folder-2
subflist group/folder-2 group/folder-3
subflist group/folder-3 group/folder-4
subflist group/folder-4 group/folder-5
subflist group/folder-5 group/folder-6
#uncomment for -6 names and pages
#subflist group/folder-6 group/folder-7

#Download pages
function pagesdl() {
	#$1 - in
	#$2 - out
	rm -f "$tmpfile/5"
	touch "$tmpfile/5"
	mkdir -p $2
	if ! [ -f "$2/.scraped" ]; then
		while read -r gid id num; do
			for i in $(seq 2 $num); do
				if ! [ -f "$2/$gid.$id.$i" ]; then
					printf "url=https://www.fimfiction.net/group/$gid/folder/$id/?page=$i\noutput=$2/$gid.$id.$i\n" >> "$tmpfile/5"
				fi
			done
		done < "$1/.pages"
		curlget "-f --fail-early --remove-on-error --rate 50/m -Z --parallel-immediate --parallel-max 2 -K $tmpfile/5"
		#curlget "-f --fail-early --remove-on-error --rate 50/m -v -K $tmpfile/5"
		if [ $? -eq 0 ]; then
			touch "$2/.scraped"
		fi
	else
		echo "Skipped $2"
	fi
}
pagesdl group/folder group/folder-page
pagesdl group/folder-1 group/folder-page-1
pagesdl group/folder-2 group/folder-page-2
pagesdl group/folder-3 group/folder-page-3
pagesdl group/folder-4 group/folder-page-4
pagesdl group/folder-5 group/folder-page-5
#-6 are singlepaged for now
rm -r "$tmpfile"
#exit 0

#Extract ids
function pagesparse() {
	#$1 - in
	#$2 - out
	#$3 - in2(pages)
	mkdir -p $2
	if ! [ -f "$2/.scraped" ]; then
		rm -r $2
		mkdir -p $2
		cp $1/.folders $1/.names $2/
		#First page
		while read -r gid id parent; do
			touch $2/$id
			hxnormalize -x -i 0 < $1/$gid.$id | hxselect ul.story-card-list 'a.story_link::attr(href)' -s '\n' -c | awk '{print $3}' 'FS=/' > $2/$id
		done < "$1/.folders"
		while read -r gid id num; do
			#Next pages
			for i in $(seq 2 $num); do
				hxnormalize -x -i 0 < $3/$gid.$id.$i | hxselect 'a.story_link::attr(href)' -s '\n' -c | awk '{print $3}' 'FS=/' >> $2/$id
			done
		done < "$1/.pages"
		touch "$2/.scraped"
	else
		echo "Skipped $2"
	fi
}
pagesparse group/folder group/out group/folder-page &
pagesparse group/folder-1 group/out-1 group/folder-page-1 &
pagesparse group/folder-2 group/out-2 group/folder-page-2 &
wait -n
pagesparse group/folder-3 group/out-3 group/folder-page-3 &
wait -n
pagesparse group/folder-4 group/out-4 group/folder-page-4 &
wait -n
pagesparse group/folder-5 group/out-5 group/folder-page-5 &
wait -n
pagesparse group/folder-6 group/out-6 group/folder-page-6 &
wait
