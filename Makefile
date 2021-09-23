BIN = src/show src/admin-add-cat
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

tmp:
	mkdir -p $@
	chown www:www $@

install: ${BIN} cgi tmp
	cp ${BIN} cgi

clean:
	rm -f src/*.o ${BIN}
