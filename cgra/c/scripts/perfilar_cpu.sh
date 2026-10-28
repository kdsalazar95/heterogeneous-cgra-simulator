#!/usr/bin/env bash
# Línea base de CPU para comparar con la GPU: mide los 3 programas en todas
# las mallas de Programas_PEs/ con perfilar.sh.
#
#   perfilar_cpu.sh [muestras=120]
#
# Descomprime los zips en cgra/c/perfilado/entradas/<malla>/ y, como solo el
# de 32x32 trae cgra.json, arma el de cada malla cambiando filas y columnas
# de cgra/src/cgra.json. Los CSV quedan en cgra/c/perfilado/.
set -euo pipefail

muestras=${1:-120}
c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
repo=$(cd "$c/../.." && pwd)
entradas=$c/perfilado/entradas
export CC=${CC:-$HOME/micromamba/envs/cuda/bin/x86_64-conda-linux-gnu-gcc}
export OPT=-O2

# Compilar el simulador (no hay make en el sistema).
mkdir -p "$c/build"
$CC -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L $OPT -ffp-contract=off \
    -I"$c/include" -DCGRA_RAIZ="\"$repo/cgra\"" "$c"/src/*.c -o "$c/build/cgra" -lm

# Solo el zip de 32x32 trae memoria.bin. No depende de la malla: se genera
# una vez con operaciones.c (igual byte a byte a la del zip).
memorias=$entradas/memorias
mkdir -p "$memorias"
$CC "$repo/cgra/operaciones/operaciones.c" -o "$memorias/operaciones" -lm
for programa in reduccion matmul convolucion; do
    mkdir -p "$memorias/$programa"
    (cd "$memorias/$programa" && ../operaciones "$programa" > /dev/null)
done

for zip in "$repo"/Programas_PEs/pe_programas_*.zip; do
    malla=$(basename "$zip" .zip)
    malla=${malla#pe_programas_}       # 4x4, 6x8, ...
    filas=${malla%x*}
    columnas=${malla#*x}
    destino=$entradas/$malla
    if [[ ! -d $destino ]]; then
        mkdir -p "$destino"
        python3 -m zipfile -e "$zip" "$destino"
    fi
    sed -e "s/\"filas\": [0-9]*/\"filas\": $filas/" \
        -e "s/\"columnas\": [0-9]*/\"columnas\": $columnas/" \
        "$repo/cgra/src/cgra.json" > "$destino/cgra.json"

    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        [[ -f $carpeta/memoria.bin ]] || cp "$memorias/$programa/memoria.bin" "$carpeta/"
        echo "== $programa $malla" >&2
        "$c/scripts/perfilar.sh" O2 "$c/build/cgra" "$carpeta" "$destino/cgra.json" "$muestras"
    done
done
