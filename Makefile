# FFOS v1.4 makefile for GNU make and GCC

TEST_BIN = ffostest
OSTYPE = unix
# linux | bsd
OS = 

FFOS = .
FFOS_OBJ_DIR = .

C = gcc
LD = gcc
O = -o
O_LD = -o

DBG = -g

override CFLAGS += -c $(DBG) -Wall -Werror -I$(FFOS) -pthread
override LDFLAGS += -lrt -pthread


all: $(TEST_BIN)

clean:
	rm -vf $(TEST_BIN) \
		$(FFOS_TEST_OBJ) $(FFOS_OBJ)

include ./makerules
