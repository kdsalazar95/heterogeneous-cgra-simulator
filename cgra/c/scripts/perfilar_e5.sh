#!/usr/bin/env bash
# Mide cgra (CPU) en todas las mallas con perfilar.sh y guarda el desglose de
# E5 de cada muestra en <programa>_<malla>_E5_desglose.csv.
#
#   perfilar_e5.sh [muestras=120]
set -euo pipefail

muestras=${1:-120}
c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
salida=$c/perfilado
# Solo para entorno.txt: el binario es el build/cgra de perfilar_cpu.sh.
export CC=${CC:-gcc}
export OPT=-O2

for destino in "$c"/perfilado/entradas/*x*; do
    malla=$(basename "$destino")
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        desglose=$salida/${programa}_${malla}_E5_desglose.csv
        export CGRA_DESGLOSE_E5=$desglose.tmp
        rm -f "$CGRA_DESGLOSE_E5"
        echo "== $programa $malla" >&2
        "$c/scripts/perfilar.sh" E5 "$c/build/cgra" "$carpeta" "$destino/cgra.json" "$muestras"

        # Las 5 primeras líneas son del calentamiento de perfilar.sh.
        echo "muestra,conteo_ns,encabezado_ns,distribucion_ns,actividad_ns,ciclos_ns,memoria_final_ns" > "$desglose"
        tail -n +6 "$CGRA_DESGLOSE_E5" | awk '{print NR "," $0}' >> "$desglose"
        rm -f "$CGRA_DESGLOSE_E5"
    done
done
