CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
TARGET = main
SRCS = main.c

all: $(TARGET)
$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)
run: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)

.PHONY: all clean