# ffos: Makefile for echo-server
# Usage:
# ./echo-server [LISTEN_PORT]

FFBASE_DIR := ../../ffbase
FFOS_DIR := ..
C := gcc -c
LINKER := gcc
CFLAGS := -O2 -I$(FFOS_DIR) -I$(FFBASE_DIR)

all: echo-server

main.o: ./main.c
	$(C) $(CFLAGS) $< -o $@

echo-server: ./main.o
	$(LINKER) $+ -o $@
