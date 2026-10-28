#!/usr/bin/env bash
# Compara cgra (CPU) con cgra_gpu en todas las mallas: salida, reporte y
# memoria final tienen que ser idénticos. Usa las entradas de perfilar_cpu.sh.
set -uo pipefail

c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
temporal=$(mktemp -d)
trap 'rm -rf "$temporal"' EXIT
iguales=0
distintos=0

# Corre un binario en su carpeta, con una copia de la memoria.
correr() {
    local binario=$1 carpeta=$2 config=$3 destino=$4
    mkdir -p "$destino"
    cp "$carpeta/memoria.bin" "$destino/mem.bin"
    (cd "$destino" && "$binario" "$carpeta" --cgra "$config" --memoria mem.bin --write-back \
        --reporte-ciclos rep.txt > salida.txt 2>&1; echo "código $?" >> salida.txt)
}

for destino in "$c"/perfilado/entradas/*x*; do
    malla=$(basename "$destino")
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        rm -rf "$temporal"/*
        correr "$c/build/cgra" "$carpeta" "$destino/cgra.json" "$temporal/cpu"
        correr "$c/build/cgra_gpu" "$carpeta" "$destino/cgra.json" "$temporal/gpu"
        if diff -r "$temporal/cpu" "$temporal/gpu" > /dev/null; then
            echo "igual     $programa $malla"
            iguales=$((iguales + 1))
        else
            echo "DISTINTO  $programa $malla"
            diff -r "$temporal/cpu" "$temporal/gpu" | head -5
            distintos=$((distintos + 1))
        fi
    done
done

echo "iguales: $iguales, distintos: $distintos"
[[ $distintos -eq 0 ]]
