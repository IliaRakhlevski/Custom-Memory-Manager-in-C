CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic -std=c11 -g
SANITIZE=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all

all: memory_manager test_memory_manager

# demo program: prints the memory layout after each step
memory_manager: main.c memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -o $@ main.c memory_manager.c

# unit tests: return a non-zero exit code if a check fails
test_memory_manager: tests/test_memory_manager.c memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -I. -o $@ tests/test_memory_manager.c memory_manager.c

test: test_memory_manager
	./test_memory_manager

# the same tests with AddressSanitizer and UndefinedBehaviorSanitizer
sanitize: clean
	$(MAKE) test CFLAGS="$(CFLAGS) $(SANITIZE)"

clean:
	rm -f *.o memory_manager test_memory_manager

.PHONY: all test sanitize clean
