CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

TARGET = sync_car
SRCS = sync_car.c sync_car-runner.c
OBJS = $(SRCS:.c=.o)

.PHONY: all run clean stress tsan

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

# ---- stress: roda o teste N vezes e conta acertos/falhas ----
N ?= 100

stress: $(TARGET)
	@ok=0; fail=0; i=0; \
	while [ $$i -lt $(N) ]; do \
		if ./$(TARGET) >/dev/null 2>&1; then ok=$$((ok + 1)); \
		else fail=$$((fail + 1)); fi; \
		i=$$((i + 1)); \
	done; \
	echo "stress: $$ok/$(N) ok, $$fail falhas"; \
	[ $$fail -eq 0 ]

# ---- tsan: build separado com ThreadSanitizer (não toca no sync_car normal) ----
TSAN_TARGET = sync_car_tsan
TSAN_CFLAGS = -Wall -Wextra -g -O1 -pthread -fsanitize=thread
TSAN_OBJS = $(SRCS:.c=.tsan.o)

$(TSAN_TARGET): $(TSAN_OBJS)
	$(CC) $(TSAN_CFLAGS) -o $@ $(TSAN_OBJS)

%.tsan.o: %.c sync_car.h
	$(CC) $(TSAN_CFLAGS) -c $< -o $@

sync_car-runner.tsan.o: TSAN_CFLAGS += -Wno-unused-parameter

tsan: $(TSAN_TARGET)
	@ok=0; fail=0; i=1; \
	while [ $$i -le 10 ]; do \
		echo "===== tsan execução $$i ====="; \
		if ./$(TSAN_TARGET); then ok=$$((ok + 1)); \
		else echo "(exit $$?)"; fail=$$((fail + 1)); fi; \
		i=$$((i + 1)); \
	done; \
	echo "tsan: $$ok/10 ok, $$fail falhas"; \
	[ $$fail -eq 0 ]

clean:
	rm -f $(TARGET) $(TSAN_TARGET) *.o
