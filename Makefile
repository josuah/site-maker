LDFLAGS = -static
CFLAGS = -g -pedantic -std=c99 -Wall -Wextra -Wno-unused-function

all: index.cgi

index.cgi: index.c libhttpd.h
	${CC} ${LDFLAGS} ${CFLAGS} -o $@ index.c
