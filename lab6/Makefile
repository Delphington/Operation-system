CC = gcc
CFLAGS = -Wextra -pedantic
TARGET = main
SRC = main.c utils.c pipe_demo.c fifo_demo.c

.PHONY: all build clean run_pipe run_fifo

all: build

build: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	-rm -f $(TARGET)
	-@fifo="/tmp/demo_fifo_$$(id -u)"; if [ -e $$fifo ]; then rm -f $$fifo; fi

run_pipe: build
	@echo "Running pipe demo (fork). Child will print after ~6 seconds..."
	./$(TARGET) pipe

run_fifo: build
	@echo "Starting fifo reader in background..."
	./$(TARGET) fifo-reader & \
	READ_PID=$$!; \
	sleep 1; \
	echo "Running fifo writer now..."; \
	./$(TARGET) fifo-writer; \
	echo "Waiting for reader (pid $$READ_PID) to finish..."; \
	wait $$READ_PID; \
	echo "FIFO demo complete."