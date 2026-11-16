CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pedantic

SRC = src/main.c src/io.c src/bnb.c src/dp.c

.PHONY: all test clean

all: coloring

coloring: $(SRC) src/graph.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: coloring
	python3 tests/run_tests.py

clean:
	rm -f coloring
