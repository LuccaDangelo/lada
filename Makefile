CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

# Número de frames da memória física (padrão 128). Ex.: make FRAMES=256
FRAMES ?= 128
CFLAGS += -DNUM_FRAMES=$(FRAMES)

# TLB ligada (1, padrão) ou desligada (0). Ex.: make TLB=0
TLB ?= 1
CFLAGS += -DUSE_TLB=$(TLB)

# Busca na TLB com uma thread por entrada (1, padrão) ou sequencial (0). Só tem efeito com TLB=1.
THREADS ?= 1
CFLAGS += -DUSE_THREADS=$(THREADS)

TARGET = vm
OBJS = vm.o

.PHONY: all clean tsan

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compila com ThreadSanitizer e executa, para checar condições de corrida.
tsan:
	$(CC) $(CFLAGS) -fsanitize=thread -g -O1 -o vm_tsan vm.c
	./vm_tsan addresses.txt lru fifo

clean:
	rm -rf $(TARGET) vm_tsan vm_tsan.dSYM *.o correct.txt
