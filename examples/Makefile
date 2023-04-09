# ffos: Makefile for example code

ROOT_DIR := ../..
FFBASE_DIR := $(ROOT_DIR)/ffbase
FFOS_DIR := $(ROOT_DIR)/ffos
DIR := $(FFOS_DIR)/examples
LINKER := gcc
CFLAGS := -I$(FFOS_DIR) -I$(FFBASE_DIR)
CFLAGS += -O0 -g

all: tcp-echo-server \
	tcp-msg-router

%: $(DIR)/%.c
	$(LINKER) $(CFLAGS) $+ -o $@
