LDFLAGS = -static
CFLAGS = -g -pedantic -std=c99 -Wall -Wextra -Wno-unused-function

all: index.cgi tmp db/category db/item

tmp db/category db/item:
	mkdir -p -m 700 $@
	chown www:www $@

index.cgi: index.c libhttpd.h
	${CC} ${LDFLAGS} ${CFLAGS} -o $@ index.c
