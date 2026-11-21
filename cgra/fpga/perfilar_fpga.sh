#!/usr/bin/env bash
# Perfila en la KV260 el simulador en su CPU (ARM Cortex-A53, línea base) y con
# E4 en la FPGA, con perfilar.sh (5 de calentamiento + N muestras). Guarda el
# desglose de E4 en la FPGA de cada muestra en <programa>_<malla>_<variante>_desglose.csv.
#
#   sudo -E perfilar_fpga.sh [muestras=120] [variante=FPGA_v2]
#   MALLAS="4x4 12x12 32x32"  mallas a medir (default: esas tres)
#   SOLO_FPGA=1               no repite la línea base de CPU
#   BIN=<binario>             otro binario (por ejemplo cgra/c/build/cgra_fpga_omp)
#
# Requisitos: preparar_entradas.sh, compilar_fpga.sh (MODO=hw) y cargar_overlay.py.
set -euo pipefail

muestras=${1:-120}
variante=${2:-FPGA_v2}
fpga=$(cd "$(dirname "$0")" && pwd)
raiz=$(cd "$fpga/.." && pwd)
c=$raiz/c
salida=$c/perfilado
MALLAS=${MALLAS:-"4x4 12x12 32x32"}
mkdir -p "$salida"

# La CPU de la Kria: mismo binario y flags que la línea base de la Jetson.
export CC=${CC:-gcc} OPT=-O2
mkdir -p "$c/build"
$CC -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L $OPT -ffp-contract=off -I"$c/include" \
    -DCGRA_RAIZ="\"$raiz\"" "$c"/src/*.c -o "$c/build/cgra" -lm

{
    echo "=== KV260 ($variante): $(date -Iseconds)"
    tr -d '\0' < /proc/device-tree/model 2>/dev/null; echo
    cat "$fpga/overlay/regs.txt" 2>/dev/null
} >> "$salida/entorno.txt"

for malla in $MALLAS; do
    destino=$c/perfilado/entradas/$malla
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        if [[ -z ${SOLO_FPGA:-} ]]; then
            echo "== CPU ARM $programa $malla" >&2
            "$c/scripts/perfilar.sh" ARM "$c/build/cgra" "$carpeta" "$destino/cgra.json" "$muestras"
        fi
        desglose=$salida/${programa}_${malla}_${variante}_desglose.csv
        export CGRA_DESGLOSE_FPGA=$desglose.tmp
        rm -f "$CGRA_DESGLOSE_FPGA"
        echo "== $variante $programa $malla" >&2
        "$c/scripts/perfilar.sh" "$variante" "${BIN:-$c/build/cgra_fpga}" "$carpeta" "$destino/cgra.json" "$muestras"
        echo "muestra,reservar_ns,empacar_ns,h2d_ns,kernel_ns,d2h_ns" > "$desglose"
        tail -n +6 "$CGRA_DESGLOSE_FPGA" | awk '{print NR "," $0}' >> "$desglose"
        rm -f "$CGRA_DESGLOSE_FPGA"
    done
done
