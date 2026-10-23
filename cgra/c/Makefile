CC       ?= gcc
OPT      ?= -O2
CFLAGS   ?= -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
BUILD    ?= build

# cgra/, para encontrar src/cgra.json, programas/ y operaciones/ desde
# cualquier directorio. Si se mueve el repositorio hay que recompilar.
RAIZ     := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)
CPPFLAGS += -Iinclude -DCGRA_RAIZ='"$(RAIZ)"'

MODULOS  := error json texto formato_numero isa configuracion memoria programa \
            traza malla salida reporte perfil main
OBJETOS  := $(MODULOS:%=$(BUILD)/%.o)

# Sin -ffp-contract el compilador puede fusionar a * b + c en una sola
# instrucción (FMA) y el resultado ya no coincide con el de Python.
$(BUILD)/cgra: $(OBJETOS)
	$(CC) $(CFLAGS) $(OPT) $(LDFLAGS) $^ -o $@ -lm

$(BUILD)/%.o: src/%.c $(wildcard include/*.h) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OPT) -ffp-contract=off -c $< -o $@

$(BUILD):
	mkdir -p $@

# El JSON con el que se compilaron los programas de programas/.
PRUEBA_CGRA ?= $(RAIZ)/src/cgra.json

test: $(BUILD)/cgra
	python3 test/equivalence.py --binary $(abspath $(BUILD)/cgra) --cgra $(abspath $(PRUEBA_CGRA))

clean:
	rm -rf $(BUILD)

.PHONY: test clean
