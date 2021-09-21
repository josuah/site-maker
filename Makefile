BIN = src/page-home src/page-item src/page-category
OBJ = src/html.o src/util.o src/cgi.o src/info.o

all: ${BIN}

.c.o:
	${CC} -c ${CFLAGS} -o $@ $<

${BIN}: ${BIN:=.o} ${OBJ}
	${CC} -o $@ $@.o ${OBJ}

data cgi:
	mkdir -p $@

install: ${BIN} cgi data
	cp ${BIN} cgi

clean:
	rm -f src/*.o ${BIN}
