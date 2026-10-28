#!/usr/bin/env bash
# Mide cgra_gpu en todas las mallas con perfilar.sh, igual que perfilar_cpu.sh,
# y guarda el desglose de E4 de cada muestra en <programa>_<malla>_GPU_desglose.csv.
#
#   perfilar_gpu.sh [muestras=120]
set -euo pipefail

muestras=${1:-120}
c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
salida=$c/perfilado
export CC=$HOME/micromamba/envs/cuda/bin/nvcc
export OPT="-O2 --fmad=false -arch=sm_89"

"$c/scripts/compilar_gpu.sh" > /dev/null
nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv >> "$salida/entorno.txt"

for destino in "$c"/perfilado/entradas/*x*; do
    malla=$(basename "$destino")
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        desglose=$salida/${programa}_${malla}_GPU_desglose.csv
        export CGRA_DESGLOSE_GPU=$desglose.tmp
        rm -f "$CGRA_DESGLOSE_GPU"
        echo "== $programa $malla" >&2
        "$c/scripts/perfilar.sh" GPU "$c/build/cgra_gpu" "$carpeta" "$destino/cgra.json" "$muestras"

        # Las 5 primeras líneas son del calentamiento de perfilar.sh.
        echo "muestra,contexto_ns,h2d_ns,kernels_ns,d2h_ns" > "$desglose"
        tail -n +6 "$CGRA_DESGLOSE_GPU" | awk '{print NR "," $0}' >> "$desglose"
        rm -f "$CGRA_DESGLOSE_GPU"
    done
done
