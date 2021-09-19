BIN = src/page-home src/page-item src/page-search

all: ${BIN}

${BIN}: ${BIN:=.c}
	${CC} -o $@ $@.c

data cgi:
	mkdir -p $@

install: ${BIN} cgi data
	cp ${BIN} cgi

clean:
	rm -f src/*.o ${BIN}
