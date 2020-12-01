# ffos makefile
# 2020, Simon Zolin

# set OS
ifndef $(OS)
	uname := $(shell uname)
	ifeq ($(uname),Linux)
		OS := linux
	else ifeq ($(uname),FreeBSD)
		OS := freebsd
	else ifeq ($(uname),Darwin)
		OS := apple
	else
		OS := win
	endif
endif

# set compiler
CPREFIX :=
# CPREFIX := x86_64-w64-mingw32-
COMPILER := gcc
ifeq ($(OS),freebsd)
	COMPILER := clang
endif
ifeq ($(OS),apple)
	COMPILER := clang
endif
C := $(CPREFIX)gcc -c
CXX := $(CPREFIX)g++ -c
LINKER := $(CPREFIX)gcc
ifeq ($(COMPILER),clang)
	C := clang -c
	CXX := clang++ -c
	LINKER := clang
endif

# set utils
RM := rm -f

FFOS_DIR := ..
FFBASE_DIR := ../../ffbase
HEADERS := $(wildcard $(FFOS_DIR)/FFOS/*.h)

TEST_OUT_DIR := $(FFOS_DIR)/test

TEST_EXE := $(TEST_OUT_DIR)/ffostest
ifeq ($(OS),win)
	TEST_EXE := $(TEST_OUT_DIR)/ffostest.exe
endif

TEST_OBJ := \
	$(TEST_OUT_DIR)/backtrace.o \
	$(TEST_OUT_DIR)/compat.o \
	$(TEST_OUT_DIR)/cpu.o \
	$(TEST_OUT_DIR)/dir.o \
	$(TEST_OUT_DIR)/dylib.o \
	$(TEST_OUT_DIR)/environ.o \
	$(TEST_OUT_DIR)/error.o \
	$(TEST_OUT_DIR)/file.o \
	$(TEST_OUT_DIR)/filemap.o \
	$(TEST_OUT_DIR)/kqueue.o \
	$(TEST_OUT_DIR)/mem.o \
	$(TEST_OUT_DIR)/path.o \
	$(TEST_OUT_DIR)/perf.o \
	$(TEST_OUT_DIR)/pipe.o \
	$(TEST_OUT_DIR)/process.o \
	$(TEST_OUT_DIR)/semaphore.o \
	$(TEST_OUT_DIR)/signal.o \
	$(TEST_OUT_DIR)/socket.o \
	$(TEST_OUT_DIR)/std.o \
	$(TEST_OUT_DIR)/test.o \
	$(TEST_OUT_DIR)/thread.o \
	$(TEST_OUT_DIR)/timer.o
# 	$(TEST_OUT_DIR)/fileaio.o
# 	$(TEST_OUT_DIR)/kqu.o
# TEST_OBJ += $(TEST_OUT_DIR)/ffos.o
ifeq ($(OS),win)
# 	TEST_OBJ += $(TEST_OUT_DIR)/ffwin.o $(TEST_OUT_DIR)/ffwin-skt.o
	TEST_OBJ += $(TEST_OUT_DIR)/winreg.o
else
# 	TEST_OBJ += $(TEST_OUT_DIR)/ffunix.o
	ifeq ($(OS),linux)
# 		TEST_OBJ += $(TEST_OUT_DIR)/fflinux.o
	else ifeq ($(OS),freebsd)
# 		TEST_OBJ += $(TEST_OUT_DIR)/ffbsd.o
	else ifeq ($(OS),apple)
# 		TEST_OBJ += $(TEST_OUT_DIR)/ffapple.o
	endif
endif

all: $(TEST_EXE)

clean:
	$(RM) $(TEST_EXE) $(TEST_OBJ)

TEST_CFLAGS := -I$(FFOS_DIR) -I$(FFBASE_DIR) -Wall -Wextra -Werror -fvisibility=hidden
TEST_CFLAGS += -DFF_DEBUG -O0 -g -DFFOS_NO_COMPAT
TEST_CXXFLAGS := $(TEST_CFLAGS)
TEST_CFLAGS += -std=gnu99
# TEST_CFLAGS += -fsanitize=address
# TEST_LDFLAGS += -fsanitize=address

ifeq ($(OS),win)
	# TEST_CFLAGS += -DFF_WIN=0x0501
	TEST_LDLIBS += -lws2_32 -liphlpapi
else
	TEST_LDFLAGS += -pthread
	ifeq ($(OS),linux)
		TEST_LDFLAGS += -ldl
	else ifeq ($(OS),freebsd)
		TEST_LDFLAGS += -lexecinfo
	endif
endif

$(TEST_OUT_DIR)/%.o: $(FFOS_DIR)/FFOS/%.c $(HEADERS) $(FFOS_DIR)/test/Makefile
	$(C) $(TEST_CFLAGS) $< -o $@

$(TEST_OUT_DIR)/%.o: $(FFOS_DIR)/test/%.c $(HEADERS) $(FFOS_DIR)/test/Makefile
	$(C) $(TEST_CFLAGS) $< -o $@

$(TEST_OUT_DIR)/%.o: $(FFOS_DIR)/test/%.cpp $(HEADERS) $(FFOS_DIR)/test/Makefile
	$(CXX) $(TEST_CXXFLAGS) $< -o $@

$(TEST_EXE): $(TEST_OBJ)
	$(LINKER) $(TEST_LDFLAGS) $+ $(TEST_LDLIBS) -o $@
