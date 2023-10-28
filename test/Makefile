# ffos makefile

ROOT := ../..
FFOS_DIR := $(ROOT)/ffos
FFBASE_DIR := $(ROOT)/ffbase

include $(FFBASE_DIR)/conf.mk

TEST_EXE := ffostest$(DOTEXE)

OBJ := \
	backtrace.o \
	compat.o \
	dir.o \
	dylib.o \
	environ.o \
	error.o \
	file.o \
	filemap.o \
	kcall.o \
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
	timerqueue.o \
	vol.o
# 	fileaio.o
ifeq "$(OS)" "windows"
	OBJ += \
		filemon.o \
		winreg.o
else
	ifeq "$(OS)" "linux"
		OBJ += \
			filemon.o \
			netlink.o
	else ifeq "$(OS)" "freebsd"
	else ifeq "$(OS)" "apple"
	endif
endif

default: $(TEST_EXE)

clean:
	$(RM) $(TEST_EXE) $(OBJ)

CFLAGS := -MMD -MP -I$(FFOS_DIR) -I$(FFBASE_DIR) \
	-Wall -Wextra -Wno-array-bounds -fno-strict-aliasing \
	-DFFBASE_HAVE_FFERR_STR -DFF_DEBUG -g
ifeq "$(DEBUG)" "1"
	CFLAGS += -O0 -DFFBASE_DEBUG
else
	CFLAGS += -O3
endif
CXXFLAGS := $(CFLAGS)
CFLAGS += -std=gnu99

ifeq "$(ASAN)" "1"
	CFLAGS += -fsanitize=address
	LINKFLAGS += -fsanitize=address
endif

ifeq "$(OS)" "windows"
	# CFLAGS += -DFF_WIN_APIVER=0x0501
	LINKFLAGS += -lws2_32 -liphlpapi
else
	LINKFLAGS += -pthread
	ifeq "$(OS)" "linux"
		LINKFLAGS += -ldl
	else ifeq "$(OS)" "freebsd"
		LINKFLAGS += -lexecinfo
	endif
endif

-include $(wildcard *.d)

%.o: $(FFOS_DIR)/FFOS/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: $(FFOS_DIR)/test/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: $(FFOS_DIR)/test/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

$(TEST_EXE): $(OBJ)
	$(LINK) $+ $(LINKFLAGS) -o $@
