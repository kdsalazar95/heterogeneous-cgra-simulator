#!/usr/bin/env bash
# Compara el simulador en CPU (cgra) con la versión FPGA en todas las mallas:
# salida, código de salida, reporte de ciclos y memoria final deben ser idénticos.
#
#   verificar_fpga.sh [binario_fpga=cgra/c/build/cgra_fpga]
#
# En la Kria se corre con sudo (el binario hw usa /dev/mem).
set -uo pipefail

fpga=$(cd "$(dirname "$0")" && pwd)
raiz=${CGRA_RAIZ_DIR:-$(cd "$fpga/.." && pwd)}
c=$raiz/c
binario=$(realpath "${1:-$c/build/cgra_fpga}")
temporal=$(mktemp -d)
trap 'rm -rf "$temporal"' EXIT
iguales=0
distintos=0

correr() {
    local bin=$1 carpeta=$2 config=$3 destino=$4
    mkdir -p "$destino"
    cp "$carpeta/memoria.bin" "$destino/mem.bin"
    (cd "$destino" && "$bin" "$carpeta" --cgra "$config" --memoria mem.bin --write-back \
        --reporte-ciclos rep.txt > salida.txt 2>&1; echo "código $?" >> salida.txt)
}

for destino in "$c"/perfilado/entradas/*x*; do
    malla=$(basename "$destino")
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        rm -rf "${temporal:?}"/*
        correr "$c/build/cgra" "$carpeta" "$destino/cgra.json" "$temporal/cpu"
        correr "$binario" "$carpeta" "$destino/cgra.json" "$temporal/fpga"
        if diff -r "$temporal/cpu" "$temporal/fpga" > /dev/null; then
            echo "igual     $programa $malla"
            iguales=$((iguales + 1))
        else
            echo "DISTINTO  $programa $malla"
            diff -r "$temporal/cpu" "$temporal/fpga" | head -5
            distintos=$((distintos + 1))
        fi
    done
done
echo "iguales: $iguales, distintos: $distintos"
[[ $distintos -eq 0 ]]
