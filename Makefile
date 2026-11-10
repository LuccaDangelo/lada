CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g

.PHONY: all clean

all: banker

banker: banker.c
	$(CC) $(CFLAGS) -o banker banker.c

clean:
	rm -rf banker *.o result.txt banker.dSYM
