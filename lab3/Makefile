.PHONY: all clean

FLAGS = -Wall -Wextra -O2

all: main

main: main.o
	gcc main.o -o main

main.o: main.c
	gcc main.c -c ${FLAGS}

clean:
	rm *.o main