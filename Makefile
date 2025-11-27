CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99

all: bin/pnlc

bin/pnlc: pnlc.c | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -Wno-unused-parameter -Wno-missing-field-initializers $^ -o $@

bin/:
	mkdir bin/

clean:
	rm -rf bin/
