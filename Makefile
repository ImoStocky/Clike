CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g -Isrc
BUILD_DIR = build
TARGET = $(BUILD_DIR)/ifj

SOURCES = \
	src/main.c \
	src/compiler.c \
	src/util/str.c \
	src/util/types.c \
	src/util/ial.c \
	src/frontend/lexal.c \
	src/frontend/syntal.c \
	src/frontend/ast.c \
	src/interp/interp.c \
	src/interp/sem.c \
	src/interp/builtins.c

OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS = $(OBJECTS:.o=.d)

.PHONY: all clean test memcheck

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

test: $(TARGET)
	IFJ_BIN="$(abspath $(TARGET))" tests/run.sh

memcheck: $(TARGET)
	IFJ_MEMCHECK=1 IFJ_BIN="$(abspath $(TARGET))" tests/run.sh

clean:
	rm -rf $(BUILD_DIR)
