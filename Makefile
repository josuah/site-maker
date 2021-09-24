BIN = src/show src/admin src/cart
OBJ = src/html.o src/util.o src/cgi.o src/info.o

LDFLAGS = -static
CFLAGS = -g -pedantic -std=c99 -Wall -Werror

all: ${BIN}

.c.o:
	${CC} -c ${CFLAGS} -o $@ $<

${BIN}: ${BIN:=.o} ${OBJ}
	${CC} -o $@ ${LDFLAGS} $@.o ${OBJ}

cgi:
	mkdir -p $@

tmp data:
	mkdir -p $@
	chown -R www:www $@

install: ${BIN} cgi tmp data
	cp ${BIN} cgi
	touch data/info

clean:
	rm -rf src/*.o ${BIN} tmp/*
