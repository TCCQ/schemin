CFLAGS=-std=c99 -x c -static -g -pg -Og

r-linux: lisp.c linux-backend.c
	gcc ${CFLAGS} $^ -o $@
