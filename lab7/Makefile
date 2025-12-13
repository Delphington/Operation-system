CC = gcc
TARGETS = producer consumer

all: $(TARGETS)

producer: producer.c
	$(CC) -o producer producer.c

consumer: consumer.c
	$(CC) -o consumer consumer.c
clean:
	rm -f $(TARGETS)
