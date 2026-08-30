# Makefile - projeto Mandelbrot (serial, OpenMP, pthreads x2)
#
# macOS: o "gcc" do sistema e na verdade o clang da Apple, que nao aceita
# -fopenmp. Instale o GCC real via Homebrew (brew install gcc) e compile com:
#     make CC=gcc-14
# (ajuste o numero da versao conforme a que o Homebrew instalar).
#
# Nao usamos -march=native de proposito: o binario precisa rodar em outra maquina.

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c11
OMPFLAG = -fopenmp
LDLIBS = -lm

TARGET = mandelbrot
OBJS = mandelbrot.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OMPFLAG) -pthread -o $@ $(OBJS) $(LDLIBS)

mandelbrot.o: mandelbrot.c
	$(CC) $(CFLAGS) $(OMPFLAG) -pthread -c -o $@ mandelbrot.c

clean:
	rm -f $(TARGET) *.o *.pgm times.txt
