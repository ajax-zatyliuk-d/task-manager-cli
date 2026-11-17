CC := cc
CPPFLAGS := -Isrc $(shell pkg-config --cflags glib-2.0)
CFLAGS := -Wall -Wextra -Werror
LDLIBS := $(shell pkg-config --libs glib-2.0)

.PHONY: all test clean

all: build/test_task build/test_task_manager build/test_cli

clean:
	rm -rf build

build/test_task: tests/test_task.c src/task.c src/task.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_task.c src/task.c -o $@ $(LDLIBS)

build/test_task_manager: tests/test_task_manager.c src/task_manager.c src/task_manager.h src/task.c src/task.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_task_manager.c src/task_manager.c src/task.c -o $@ $(LDLIBS)

build/test_cli: tests/test_cli.c src/cli.c src/cli.h src/task_manager.c src/task_manager.h src/task.c src/task.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_cli.c src/cli.c src/task_manager.c src/task.c -o $@ $(LDLIBS)

test: all
	./build/test_task
	./build/test_task_manager
	./build/test_cli
