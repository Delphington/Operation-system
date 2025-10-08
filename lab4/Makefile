CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = mychmod
SOURCE = main.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

.PHONY: clean