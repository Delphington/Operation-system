CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic
LDFLAGS = -lrt
TARGET = ipc
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean