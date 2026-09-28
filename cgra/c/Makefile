CC       ?= gcc
OPT      ?= -O2
CFLAGS   ?= -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -Iinclude -I../compartido          # ../compartido holds memoria_cgra.h
BUILD    ?= build

FUENTES  := $(wildcard src/*.c)
OBJETOS  := $(FUENTES:src/%.c=$(BUILD)/%.o)

$(BUILD)/cgra: $(OBJETOS)
	$(CC) $(OPT) $^ -o $@ -lm

$(BUILD)/%.o: src/%.c $(wildcard include/*.h) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OPT) -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: clean
