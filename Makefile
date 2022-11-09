CFLAGS = -Wall

all: fmtx test

fmtx: fmtx.c
	clang $(CFLAGS) fmtx.c -o fmtx

test: test.c
	clang $(CFLAGS) test.c -o test

clean:
	rm -rvf fmtx test *.o

.PHONY: all clean

