CC ?= gcc
OPT ?= -O2
CFLAGS ?= -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -Iinclude -DCGRA_ROOT='"$(abspath ..)"'
BUILD ?= build
SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:src/%.c=$(BUILD)/%.o)
$(BUILD)/cgra: $(OBJECTS)
	$(CC) $(CFLAGS) $(OPT) $(LDFLAGS) $^ -o $@ $(LDLIBS) -lm
$(BUILD)/%.o: src/%.c include/cgra.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OPT) -ffp-contract=off -c $< -o $@
$(BUILD):
	mkdir -p $@
.PHONY: test
test: $(BUILD)/cgra
	python3 test/equivalence.py --binary $(abspath $(BUILD)/cgra)
