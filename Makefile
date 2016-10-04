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

override FFOS_CFLAGS += -c -g -Wall -Werror -I$(FFOS) -pthread -DFF_FFOS_ONLY
CFLAGS := $(FFOS_CFLAGS)
override LDFLAGS += -pthread


all: $(TEST_BIN)

clean:
	rm -vf $(TEST_BIN) \
		$(FFOS_TEST_OBJ) $(FFOS_OBJ)

include $(FFOS)/makerules


# test
FFOS_TEST_HDR := $(wildcard $(FFOS)/test/*.h)
FFOS_TEST_SRC := $(wildcard $(FFOS)/test/*.c)
FFOS_TEST_OBJ := $(addprefix $(FFOS_OBJ_DIR)/, $(addsuffix .o, $(notdir $(basename $(FFOS_TEST_SRC)))))

$(FFOS_OBJ_DIR)/%.o: $(FFOS)/test/%.c $(FFOS_HDR) $(FFOS_TEST_HDR)
	$(C) $(CFLAGS)  $< -o$@

TEST_O := $(FFOS_TEST_OBJ) \
	$(FFOS_OBJ) $(FFOS_OBJ_DIR)/fftest.o

$(TEST_BIN): $(FFOS_HDR) $(FFOS_TEST_HDR) $(TEST_O)
	$(LD) $(TEST_O) $(LDFLAGS) $(LD_LDL) $(LD_LWS2_32) -o$@
