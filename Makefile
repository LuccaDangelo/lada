CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

TARGET = sync_car
SRCS = sync_car.c sync_car_runner.c
OBJS = $(SRCS:.c=.o)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c sync_car.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o
