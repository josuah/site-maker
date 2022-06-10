LDFLAGS = -static
CFLAGS = -g -pedantic -std=c99 -Wall -Wextra -Wno-unused-function

V = v1.10.3

all: index.cgi tmp db/category db/item db/image

stripe:
	mkdir -p go/bin
	GOPATH=${PWD}/go go install github.com/stripe/stripe-cli/...
	cd go/pkg/mod/github.com/stripe/stripe-cli@$V && make build
	cp go/pkg/mod/github.com/stripe/stripe-cli@v1.10.3/stripe go/bin

tmp db/category db/item db/image:
	mkdir -p -m 700 $@
	chown www:www $@

index.cgi: index.c libhttpd.h
	${CC} ${LDFLAGS} ${CFLAGS} -o $@ index.c
