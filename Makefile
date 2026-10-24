
CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS = -lm

SRC = src/main.c src/position.c src/movegen.c src/makemove.c \
      src/attack.c src/legal.c src/evaluate.c src/cerebrum.c src/engine.c \
      src/learn.c src/pgn.c src/opening_book.c

TARGET = AlienPlays-C

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
