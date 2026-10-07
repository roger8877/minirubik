CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic

.PHONY: all check clean

all: ida verify

gen: gen.c cube.h
	$(CC) $(CFLAGS) $< -o $@

tables.h tables2.h tables.s: gen
	./gen

ida: ida.c cube.h search2.h tables2.h
	$(CC) $(CFLAGS) $< -o $@

verify: verify.c cube.h search.h search2.h tables.h tables2.h
	$(CC) $(CFLAGS) $< -o $@

check: ida verify
	./verify
	./ida 21345671111111

clean:
	rm -f gen ida verify tables.h tables2.h tables.s
