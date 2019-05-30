CC	=	gcc
CFLAGS	=	-g -Og -Wconversion -Wall
CLAXFLAGS	=	-g -Og
PKGFLAGS	=	`pkg-config fuse --cflags --libs`

command.o: command.c command.h
	$(CC) -c $< -o $@ $(CFLAGS)

microfs.o: microfs.c command.h
	$(CC) -c $< -o $@ $(CFLAGS) $(PKGFLAGS)


microfs: command.o microfs.o
	  $(CC) -o $@ $^ $(CFLAGS) $(PKGFLAGS)

.PHONY: run

run: microfs
	fusermount -u ./mnt || true
	mkdir -p ./mnt
	./microfs ./mnt -- ./passthrough.sh ./testdir
