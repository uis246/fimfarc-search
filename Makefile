OPT_EXT = -Ofast -fsched-pressure -fsched-spec-load -s -g0 -fno-unwind-tables
OPT_INT = ${OPT_EXT}#-Ofast -ggdb3 -fsched-pressure -fsched-spec-load
CFLAGS = -fipa-pta -fno-stack-protector -fno-asynchronous-unwind-tables -fno-math-errno -fmerge-all-constants -fno-ident -pipe -fno-plt -D_GNU_SOURCE -fvisibility=hidden -flto -fno-fat-lto-objects -DNDEBUG -mcpu=native -ffunction-sections -fdata-sections -mtune=native -march=native
CFLAGS_EXT = ${OPT_EXT} ${CFLAGS}
CFLAGS_INT = ${OPT_INT} ${CFLAGS} -I pdjson -I minizip-mem -Wall -Wextra -Wno-switch
LDFLAGS_NDEBUG = -Wl,--gc-sections,--as-needed
LDFLAGS = -lexpat -lminizip -Wl,-z,noseparate-code,-z,norelro ${LDFLAGS_NDEBUG}
all: fimfar
build:
#@mkdir build
db:
#@mkdir db
build/ioapi.o: minizip-mem/ioapi_mem.c
	nice gcc ${CFLAGS_EXT} -c -o build/ioapi.o minizip-mem/ioapi_mem.c
build/pdjson.o: pdjson/pdjson.c
	nice gcc ${CFLAGS_EXT} -c -o build/pdjson.o pdjson/pdjson.c
build/build.o: src/builder.c src/fimfar.h src/builder.h
	nice gcc ${CFLAGS_INT} -c -o $@ $<
build/ff-build.o: src/ff-builder.c src/fimfar.h src/builder.h
	nice gcc ${CFLAGS_INT} -c -o $@ $<
build/fb-build.o: src/fb-builder.c src/fimfar.h src/builder.h
	nice gcc ${CFLAGS_INT} -c -o $@ $<
build/search.o: src/search.c src/fimfar.h
	nice gcc ${CFLAGS_INT} -c -o $@ $<
build/fimfar.o: src/fimfar.c src/fimfar.h
	nice gcc ${CFLAGS_INT} -c -o $@ $<
build/multi.o: src/multi-search.c
	nice gcc ${CFLAGS_INT} -c -o $@ $<
build/utils.o: src/utils.c
	nice gcc ${CFLAGS_INT} -c -o $@ $<
fimfar: build/ioapi.o build/pdjson.o build/build.o build/search.o build/fimfar.o build/multi.o build/ff-build.o build/fb-build.o
	nice gcc $(OPT_INT) $(CFLAGS) $(LDFLAGS) -o fimfar -Wall -Wextra -Wno-switch build/pdjson.o build/ioapi.o build/build.o build/search.o build/fimfar.o build/multi.o build/ff-build.o build/fb-build.o
clean:
	rm build/*
rebuild: db
	unzip -p /media/fimfarchive-20231201.zip index.json | nice ./fimfar build
pack:
	rm -f fimfar.zip
	zip -9r fimfar.zip Makefile src group.sh minizip-mem
	tar c Makefile src group.sh minizip-mem | xz -z9e > fimfar.tar.xz
