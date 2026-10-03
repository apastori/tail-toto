# tail-toto — GNU tail-compatible CLI in C11.
#
# Targets: all (default), debug, test, clean, install.
# All artefacts live under build/ and build/tests/.

# Prefer gcc, fall back to clang.
CC := $(shell command -v gcc >/dev/null 2>&1 && echo gcc || echo clang)

EXE :=
ifeq ($(OS),Windows_NT)
  EXE := .exe
  # Git Bash may otherwise find a minimal MinGW gcc first on PATH.
  ifneq ($(wildcard C:/msys64/ucrt64/bin/gcc.exe),)
    CC := C:/msys64/ucrt64/bin/gcc
  endif
endif

BUILD_DIR      := build
TEST_BUILD_DIR := $(BUILD_DIR)/tests

TARGET       := $(BUILD_DIR)/tail-toto$(EXE)
DEBUG_TARGET := $(BUILD_DIR)/tail-toto-debug$(EXE)
TEST_CORE    := $(TEST_BUILD_DIR)/test_core$(EXE)

BINDIR ?= /usr/local/bin

# -Werror: every warning is a bug in this project; a warning-free build on
# both gcc and clang is part of the definition of done.
WARNFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS  := -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64 -Iinclude

CFLAGS       := $(WARNFLAGS) $(CPPFLAGS) -O2
CFLAGS_DEBUG := $(WARNFLAGS) $(CPPFLAGS) -g -O1 \
                -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS      :=

# Platform probe: Windows runtimes provide <io.h> with _setmode(); there
# stdin/stdout must be switched to binary mode so bytes pass through
# untranslated. \043 is '#', which make would otherwise treat as a comment.
WIN32_IO_PROBE := $(shell printf '\043include <io.h>\n\043include <fcntl.h>\nint main(void) { return _setmode(0, _O_BINARY); }\n' \
    | $(CC) $(WARNFLAGS) $(CPPFLAGS) -fsyntax-only -x c - >/dev/null 2>&1 \
    && echo yes)

ifeq ($(WIN32_IO_PROBE),yes)
  CFLAGS       += -DTAIL_TOTO_WIN32_IO
  CFLAGS_DEBUG += -DTAIL_TOTO_WIN32_IO
endif

SRCS := src/main.c \
        src/tail_toto_emit.c \
        src/tail_toto_cli.c \
        src/tail_toto_io.c \
        src/tail_toto_tail.c \
        src/tail_toto_run.c

OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
HDRS := $(wildcard include/*.h)

TEST_SRCS := tests/test_runner.c \
             tests/test_util.c \
             tests/test_count_parse.c \
             tests/test_lines_last.c \
             tests/test_lines_from.c \
             tests/test_bytes_last.c \
             tests/test_stream.c
             
TEST_OBJS := $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_HDRS := $(wildcard tests/*.h)

# Tests link every production object except the one providing main().
LIB_OBJS := $(filter-out $(BUILD_DIR)/main.o,$(OBJS))

.PHONY: all debug test clean install

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

debug: $(DEBUG_TARGET)

# Built straight from sources so release objects are never reused.
$(DEBUG_TARGET): $(SRCS) $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_DEBUG) -o $@ $(SRCS) $(LDFLAGS)

$(TEST_BUILD_DIR)/%.o: tests/%.c $(HDRS) $(TEST_HDRS) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -Itests -c -o $@ $<

$(TEST_CORE): $(TEST_OBJS) $(LIB_OBJS) $(HDRS) $(TEST_HDRS) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(LIB_OBJS) $(LDFLAGS)

test: $(TEST_CORE)
	./$(TEST_CORE)

$(BUILD_DIR) $(TEST_BUILD_DIR):
	mkdir -p $@

clean:
	rm -f $(BUILD_DIR)/*.o $(TEST_BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/tail-toto $(BUILD_DIR)/tail-toto.exe
	rm -f $(BUILD_DIR)/tail-toto-debug $(BUILD_DIR)/tail-toto-debug.exe
	rm -f $(TEST_BUILD_DIR)/test_core $(TEST_BUILD_DIR)/test_core.exe

install: $(TARGET)
	install -m 755 $(TARGET) $(BINDIR)
