BIN = src/page-home src/page-item src/page-search
OBJ = src/html.o

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
