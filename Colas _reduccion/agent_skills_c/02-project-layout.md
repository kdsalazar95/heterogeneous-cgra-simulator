# Skill: Project Layout and Build

## Purpose

Create the `cgra_c/` folder with the layout from the README and a `Makefile`
that builds the code on the laptop.

## Actions

1. Create `cgra_c/` next to `src/` with the folders of the target layout.
2. Write `include/configuracion.h`:

   ```c
   #define FILAS_POR_DEFECTO 4     // keep equal to FILAS in src/run_cgra.py
   #define COLUMNAS_POR_DEFECTO 4  // keep equal to COLUMNAS in src/run_cgra.py
   #define NUM_REGISTROS 16
   #define CAPACIDAD_COLA 16
   ```

   The mesh size itself is a runtime value (`--filas`, `--columnas`); these are
   only the defaults.
3. Write the `Makefile`:

   ```make
   CC       ?= gcc
   OPT      ?= -O2
   CFLAGS   ?= -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
   CPPFLAGS += -Iinclude -I../src          # ../src holds memoria_cgra.h
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
   ```

   `OPT` and `BUILD` are variables on purpose: profiling builds several variants
   side by side, for example `make OPT=-O0 BUILD=build/O0`.
4. Add `cgra_c/build/` to the project `.gitignore`.
5. Build an empty `main.c` that returns 0 and check that `make` works with both
   `CC=gcc` and `CC=clang-18`.

Do not put architecture flags such as `-march=native` in the default `OPT`;
they are part of the later optimization study.

## Outputs

```text
cgra_c/Makefile
cgra_c/include/configuracion.h
cgra_c/src/main.c   (stub)
```
