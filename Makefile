BIN = src/show
OBJ = src/html.o src/util.o src/cgi.o src/info.o

LDFLAGS = --static

all: ${BIN}

.c.o:
	${CC} -c ${CFLAGS} -o $@ $<

${BIN}: ${BIN:=.o} ${OBJ}
	${CC} -o $@ ${LDFLAGS} $@.o ${OBJ}

cgi:
	mkdir -p $@

install: ${BIN} cgi
	cp ${BIN} cgi

clean:
	rm -f src/*.o ${BIN}
