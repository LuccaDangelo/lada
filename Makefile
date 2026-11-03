CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

TARGET = sync_car
SRCS = sync_car.c sync_car-runner.c
OBJS = $(SRCS:.c=.o)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c sync_car.h
	$(CC) $(CFLAGS) -c $< -o $@

# silencia só o parâmetro 'foo' não usado em alarm_handler (arquivo do professor);
# variável específica de alvo: não afeta sync_car.o
sync_car-runner.o: CFLAGS += -Wno-unused-parameter

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o
