BIN=burniso
CC=gcc
CFLAGS=-std=c99 -Wall -Wno-unused-variable -Wno-pointer-arith -Werror=vla -pedantic

.PHONY: all
all: debug

.PHONY: debug
debug: main.c
	$(CC) -ggdb $(CFLAGS) $^ -o $(BIN)

.PHONY: release
release: main.c
	$(CC) -O2 $(CFLAGS) $^ -o $(BIN)
