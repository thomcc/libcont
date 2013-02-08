CC = clang
CFLAGS = -g -Os -std=c11 -Wall -Werror-implicit-function-declaration

all: libcont.a test

rebuild: clean
rebuild: all

test: libcont.a test.o
	${CC} ${CFLAGS} -o $@ test.o libcont.a

libcont.a: CFLAGS += -fPIC
libcont.a: libcont.o
	ar rcs $@ $<
	ranlib $@

test.o: libcont.h test.c

libcont.o: libcont.h libcont.c

.PHONY: clean
clean:
	rm -f *.o *.a *~ test
	rm -rf `find . -name "*.dSYM" -print`

