CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = factory
SRC = src/main.c

all: $(TARGET)

$(TARGET): $(SRC) src/common.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) -lpthread

clean:
	rm -f $(TARGET)

.PHONY: all clean