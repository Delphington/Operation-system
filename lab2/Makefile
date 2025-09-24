CC = gcc
CFLAGS = -Wall -Wextra -std=c17

all: myls 

myls: main.c
	$(CC) $(CFLAGS) main.c -o myls

clean:
	rm -f myls

.PHONY: all clean