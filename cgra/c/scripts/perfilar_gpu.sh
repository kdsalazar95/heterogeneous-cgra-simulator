#!/usr/bin/env bash
# Mide cgra_gpu en todas las mallas con perfilar.sh, igual que perfilar_cpu.sh,
# y guarda el desglose de E4 de cada muestra en <programa>_<malla>_GPU_desglose.csv.
#
#   perfilar_gpu.sh [muestras=120] [variante=GPU]
#
# Compila con compilar_gpu.sh, que toma CC, CXX, NVCC, ARCH y CUDA_LIB del
# entorno (por defecto, la Jetson Nano).
set -euo pipefail

muestras=${1:-120}
variante=${2:-GPU}
c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
salida=$c/perfilado
mkdir -p "$salida"

# Antes de cambiar CC: compilar_gpu.sh lo usa para los .c.
"$c/scripts/compilar_gpu.sh" > /dev/null

# Para entorno.txt (perfilar.sh registra CC y OPT).
export CC=${NVCC:-/usr/local/cuda/bin/nvcc}
export OPT="-O2 --fmad=false -arch=${ARCH:-sm_53}"

# La GPU: nvidia-smi en una PC; en una Jetson no existe y se lee el modelo y L4T.
{
    echo "=== GPU ($variante): $(date -Iseconds)"
    if command -v nvidia-smi > /dev/null; then
        nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv
    else
        tr -d '\0' < /proc/device-tree/model 2>/dev/null && echo
        head -1 /etc/nv_tegra_release 2>/dev/null || echo "L4T: no disponible"
    fi
    "$CC" --version | tail -1
} >> "$salida/entorno.txt"

for destino in "$c"/perfilado/entradas/*x*; do
    malla=$(basename "$destino")
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        desglose=$salida/${programa}_${malla}_${variante}_desglose.csv
        desglose_e5=$salida/${programa}_${malla}_${variante}_E5_desglose.csv
        export CGRA_DESGLOSE_GPU=$desglose.tmp
        export CGRA_DESGLOSE_E5=$desglose_e5.tmp
        rm -f "$CGRA_DESGLOSE_GPU" "$CGRA_DESGLOSE_E5"
        echo "== $programa $malla" >&2
        "$c/scripts/perfilar.sh" "$variante" "$c/build/cgra_gpu" "$carpeta" "$destino/cgra.json" "$muestras"

        # Las 5 primeras líneas son del calentamiento de perfilar.sh.
        echo "muestra,contexto_ns,h2d_ns,kernels_ns,d2h_ns" > "$desglose"
        tail -n +6 "$CGRA_DESGLOSE_GPU" | awk '{print NR "," $0}' >> "$desglose"
        echo "muestra,conteo_ns,encabezado_ns,distribucion_ns,actividad_ns,ciclos_ns,memoria_final_ns" > "$desglose_e5"
        tail -n +6 "$CGRA_DESGLOSE_E5" | awk '{print NR "," $0}' >> "$desglose_e5"
        rm -f "$CGRA_DESGLOSE_GPU" "$CGRA_DESGLOSE_E5"
    done
done
