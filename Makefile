# FFOS makefile

ROOT := ..
FFOS := $(ROOT)/ffos
FFOS_OBJ_DIR = .
DEBUG := 1
OPT := LTO

include $(FFOS)/makeconf

ifeq ($(OS),win)
TEST_BIN := ffostest.exe
else
TEST_BIN := ffostest
endif

FFOS_CFLAGS := $(CFLAGS) -DFF_FFOS_ONLY -Werror
CFLAGS += -Werror -Wall \
	-I$(FFOS)


all: $(TEST_BIN)

clean:
	rm -vf $(TEST_BIN) \
		$(FFOS_TEST_OBJ) $(FFOS_OBJ)

include $(FFOS)/makerules


# test
FFOS_TEST_HDR := $(wildcard $(FFOS)/test/*.h)
FFOS_TEST_SRC := \
	$(FFOS)/test/cpu.c \
	$(FFOS)/test/dir.c \
	$(FFOS)/test/file.c \
	$(FFOS)/test/kqu.c \
	$(FFOS)/test/mem.c \
	$(FFOS)/test/socket.c \
	$(FFOS)/test/test.c \
	$(FFOS)/test/timer.c \
	$(FFOS)/test/types.c

ifeq ($(OS),win)
FFOS_TEST_SRC += $(FFOS)/test/reg.c
endif

FFOS_TEST_OBJ := $(addprefix $(FFOS_OBJ_DIR)/, $(addsuffix .o, $(notdir $(basename $(FFOS_TEST_SRC)))))

$(FFOS_OBJ_DIR)/%.o: $(FFOS)/test/%.c $(FFOS_HDR) $(FFOS_TEST_HDR)
	$(C) $(CFLAGS)  $< -o$@

TEST_O := $(FFOS_TEST_OBJ) $(FFOS_WREG) \
	$(FFOS_SKT) \
	$(FFOS_OBJ) $(FFOS_THD) $(FFOS_OBJ_DIR)/fftest.o

$(TEST_BIN): $(FFOS_HDR) $(FFOS_TEST_HDR) $(TEST_O)
	$(LD) $(TEST_O) $(LDFLAGS) $(LD_LDL) $(LD_LWS2_32) $(LD_LPTHREAD) -o$@
