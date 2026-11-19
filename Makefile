CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

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
