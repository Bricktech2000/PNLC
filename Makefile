.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: bin/pnlc
bin/:; mkdir bin/
clean:; rm -rf bin/

bin/pnlc: bin/ pnlc.c; $(CC) $(CFLAGS) -o $@ pnlc.c -Wno-parentheses -Wno-sign-compare -Wno-unused-value -Wno-implicit-fallthrough -Wno-missing-field-initializers
