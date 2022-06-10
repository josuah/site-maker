LDFLAGS = -static
CFLAGS = -g -pedantic -std=c99 -Wall -Wextra -Wno-unused-function

V = v1.10.3

all: index.cgi tmp db/category db/item db/image

tmp db/category db/item db/image:
	mkdir -p -m 700 $@
	chown www:www $@

index.cgi: index.c libhttpd.h
	${CC} ${LDFLAGS} ${CFLAGS} -o $@ index.c
