.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: bin/pnlc
bin/:; mkdir bin/
clean:; rm -rf bin/

# `$emit` under pnlc.c produces C code understood to serve as portable assembly
# language. when compiling it, optimize enough that the resulting machine code
# is reasonable, but not so much that the C compiler is carrying us.
EMIT=-O1 -fno-inline -fno-toplevel-reorder -foptimize-sibling-calls

bin/pnlc: bin/ pnlc.c; $(CC) $(CFLAGS) -o $@ pnlc.c -Wno-parentheses -Wno-sign-compare -Wno-unused-value -Wno-implicit-fallthrough -Wno-missing-field-initializers
bin/emit: bin/ emit.c; $(CC) $(CFLAGS) -o $@ emit.c -Wno-parentheses $(EMIT)
