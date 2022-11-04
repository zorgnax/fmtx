CFLAGS = -Wall

all: fmtx

fmtx: fmtx.c
	clang $(CFLAGS) fmtx.c -o fmtx

clean:
	rm -rvf fmtx *.o

.PHONY: all clean

