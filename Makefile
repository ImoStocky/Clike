CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g
BUILD_DIR = build
TARGET = $(BUILD_DIR)/ifj
SOURCES = main.c lexal.c syntal.c str.c types.c ial.c interp.c ast.c
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test memcheck

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

test: $(TARGET)
	IFJ_BIN="$(abspath $(TARGET))" tests/run.sh

memcheck: $(TARGET)
	IFJ_MEMCHECK=1 IFJ_BIN="$(abspath $(TARGET))" tests/run.sh

clean:
	rm -rf $(BUILD_DIR)
