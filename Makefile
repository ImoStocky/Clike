CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g
BUILD_DIR = build
TARGET = $(BUILD_DIR)/ifj
SOURCES = main.c lexal.c syntal.c str.c types.c ial.c interp.c ast.c compiler.c sem.c builtins.c
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test memcheck

main_HEADERS = ast.h compiler.h interp.h lexal.h syntal.h types.h
lexal_HEADERS = lexal.h str.h types.h
syntal_HEADERS = syntal.h compiler.h ast.h lexal.h types.h
interp_HEADERS = interp.h interp_priv.h compiler.h ial.h ast.h types.h
sem_HEADERS = interp_priv.h compiler.h ial.h ast.h types.h
builtins_HEADERS = interp_priv.h compiler.h ial.h types.h
ast_HEADERS = ast.h compiler.h types.h
compiler_HEADERS = compiler.h ast.h lexal.h types.h
str_HEADERS = str.h
types_HEADERS = types.h
ial_HEADERS = ial.h types.h

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/main.o: $(main_HEADERS)
$(BUILD_DIR)/lexal.o: $(lexal_HEADERS)
$(BUILD_DIR)/syntal.o: $(syntal_HEADERS)
$(BUILD_DIR)/interp.o: $(interp_HEADERS)
$(BUILD_DIR)/ast.o: $(ast_HEADERS)
$(BUILD_DIR)/compiler.o: $(compiler_HEADERS)
$(BUILD_DIR)/str.o: $(str_HEADERS)
$(BUILD_DIR)/types.o: $(types_HEADERS)
$(BUILD_DIR)/ial.o: $(ial_HEADERS)
$(BUILD_DIR)/sem.o: $(sem_HEADERS)
$(BUILD_DIR)/builtins.o: $(builtins_HEADERS)

$(BUILD_DIR):
	mkdir -p $@

test: $(TARGET)
	IFJ_BIN="$(abspath $(TARGET))" tests/run.sh

memcheck: $(TARGET)
	IFJ_MEMCHECK=1 IFJ_BIN="$(abspath $(TARGET))" tests/run.sh

clean:
	rm -rf $(BUILD_DIR)
