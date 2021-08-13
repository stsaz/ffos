# ffos makefile
# 2020, Simon Zolin

FFOS_DIR := ..
FFBASE_DIR := ../../ffbase
HEADERS := $(wildcard $(FFOS_DIR)/FFOS/*.h)

include $(FFOS_DIR)/test/makeconf2

TEST_EXE := ffostest$(DOTEXE)

TEST_OBJ := \
	backtrace.o \
	compat.o \
	cpu.o \
	dir.o \
	dylib.o \
	environ.o \
	error.o \
	file.o \
	filemap.o \
	kqueue.o \
	mem.o \
	path.o \
	perf.o \
	pipe.o \
	process.o \
	semaphore.o \
	signal.o \
	socket.o \
	std.o \
	test.o \
	thread.o \
	timer.o \
	timerqueue.o
# 	fileaio.o
# 	kqu.o
# TEST_OBJ += ffos.o
HEADERS += $(FFOS_DIR)/test/Makefile
ifeq "$(OS)" "windows"
# 	TEST_OBJ += ffwin.o ffwin-skt.o
	TEST_OBJ += winreg.o
else
# 	TEST_OBJ += ffunix.o
	ifeq ($(OS),linux)
# 		TEST_OBJ += fflinux.o
	else ifeq ($(OS),freebsd)
# 		TEST_OBJ += ffbsd.o
	else ifeq ($(OS),apple)
# 		TEST_OBJ += ffapple.o
	endif
endif

all: $(TEST_EXE)

clean:
	$(RM) $(TEST_EXE) $(TEST_OBJ)

TEST_CFLAGS := -I$(FFOS_DIR) -I$(FFBASE_DIR) \
	-Wall -Wextra -fno-strict-aliasing \
	-DFF_DEBUG -DFFOS_NO_COMPAT -g
ifeq "$(OPT)" "0"
	TEST_CFLAGS += -O0 -DFFBASE_DEBUG
else
	TEST_CFLAGS += -O3
endif
TEST_CXXFLAGS := $(TEST_CFLAGS)
TEST_CFLAGS += -std=gnu99
# TEST_CFLAGS += -fsanitize=address
# TEST_LDFLAGS += -fsanitize=address

ifeq "$(OS)" "windows"
	# TEST_CFLAGS += -DFF_WIN_APIVER=0x0501
	TEST_LDLIBS += -lws2_32 -liphlpapi
else
	TEST_LDLIBS += -pthread
	ifeq ($(OS),linux)
		TEST_LDLIBS += -ldl
	else ifeq ($(OS),freebsd)
		TEST_LDLIBS += -lexecinfo
	endif
endif

%.o: $(FFOS_DIR)/FFOS/%.c
	$(C) $(TEST_CFLAGS) $< -o $@

%.o: $(FFOS_DIR)/test/%.c $(HEADERS)
	$(C) $(TEST_CFLAGS) $< -o $@

%.o: $(FFOS_DIR)/test/%.cpp $(HEADERS)
	$(CXX) $(TEST_CXXFLAGS) $< -o $@

$(TEST_EXE): $(TEST_OBJ)
	$(LINK) $(TEST_LDFLAGS) $+ $(TEST_LDLIBS) -o $@
