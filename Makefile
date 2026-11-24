CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

# Número de frames da memória física (padrão 128). Ex.: make FRAMES=256
FRAMES ?= 128
CFLAGS += -DNUM_FRAMES=$(FRAMES)

# TLB ligada (1, padrão) ou desligada (0). Ex.: make TLB=0
TLB ?= 1
CFLAGS += -DUSE_TLB=$(TLB)

TARGET = vm
OBJS = vm.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o correct.txt
