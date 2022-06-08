LDFLAGS = -static
CFLAGS = -g -pedantic -std=c99 -Wall -Wextra -Wno-unused-function

all: index.cgi
index.cgi: libhttpd.h

.SUFFIXES: .cgi

.c.cgi:
	${CC} ${LDFLAGS} ${CFLAGS} -o $@ $<
