CC = gcc
CFLAGS = -Wall -Wextra -std=c17

all: mycat mygrep

mycat: main.c
	$(CC) $(CFLAGS) main.c -o mycat

mygrep: main.c
	$(CC) $(CFLAGS) main.c -o mygrep

clean:
	rm -f mycat mygrep