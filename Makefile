CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=c11

all: memory_manager

memory_manager: main.o memory_manager.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o memory_manager