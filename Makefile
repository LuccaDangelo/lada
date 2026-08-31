CC := $(shell ls /opt/homebrew/bin/gcc-[0-9]* /usr/local/bin/gcc-[0-9]* 2>/dev/null | head -n 1 || echo gcc)

mandelbrot: mandelbrot.o
	$(CC) mandelbrot.o -o mandelbrot -fopenmp -pthread -lm

mandelbrot.o: mandelbrot.c
	$(CC) -c mandelbrot.c -O2 -Wall -Wextra -std=c11 -fopenmp -pthread

run: mandelbrot
	./mandelbrot 800 800 1000 4

clean:
	rm -f mandelbrot *.o *.pgm times.txt

.PHONY: run clean