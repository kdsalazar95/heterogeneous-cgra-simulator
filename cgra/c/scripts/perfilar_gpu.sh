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
[[ $muestras =~ ^[0-9]+$ ]] && ((10#$muestras >= 2)) || {
    echo "perfilar_gpu.sh: se necesitan al menos 2 muestras" >&2
    exit 2
}
muestras=$((10#$muestras))
filas_esperadas=$((muestras + 5))
c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
salida=$c/perfilado
mkdir -p "$salida"

# Si se especifica BIN, perfila ese binario sin recompilarlo. Si no, conserva
# el comportamiento anterior y compila el build GPU predeterminado.
bin_gpu=${BIN:-$c/build/cgra_gpu}
if [[ $bin_gpu != /* ]]; then
    bin_gpu=$c/$bin_gpu
fi
if [[ -z ${BIN:-} ]]; then
    "$c/scripts/compilar_gpu.sh" > /dev/null
fi

# perfilar.sh registra el binario y su hash. Los flags van en la ficha GPU.
CC_COMPILACION=${CC:-gcc}
export CC=
export OPT="ver ficha GPU en entorno.txt"

# La GPU: nvidia-smi en una PC; en una Jetson no existe y se lee el modelo y L4T.
{
    echo "=== GPU ($variante): $(date -Iseconds)"
    if command -v nvidia-smi > /dev/null; then
        nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv
    else
        tr -d '\0' < /proc/device-tree/model 2>/dev/null && echo
        head -1 /etc/nv_tegra_release 2>/dev/null || echo "L4T: no disponible"
    fi
    registro=$bin_gpu.compilacion.txt
    if [[ -z ${BIN:-} ]]; then
        echo "C ($CC_COMPILACION): -O2 -ffp-contract=off -DCGRA_GPU"
        echo "malla_cuda.cu (${NVCC:-/usr/local/cuda/bin/nvcc}): -O2 --fmad=false -arch=${ARCH:-sm_53}"
        echo "reporte_cuda.cu (${NVCC:-/usr/local/cuda/bin/nvcc}): -O2 -arch=${ARCH:-sm_53}"
    elif [[ -f $registro && $(head -n 1 "$registro") == "$(sha256sum < "$bin_gpu")" ]]; then
        cat "$registro"
    else
        echo "Flags y compiladores del binario: no disponibles"
    fi
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
        "$c/scripts/perfilar.sh" "$variante" "$bin_gpu" "$carpeta" "$destino/cgra.json" "$muestras"

        if [[ ! -f $CGRA_DESGLOSE_E5 ]] || [[ $(wc -l < "$CGRA_DESGLOSE_E5") -ne $filas_esperadas ]]; then
            echo "perfilar_gpu.sh: desglose E5 inválido para $programa $malla (se esperaban $filas_esperadas filas)" >&2
            rm -f "$CGRA_DESGLOSE_GPU" "$CGRA_DESGLOSE_E5"
            exit 1
        fi

        # Las 5 primeras líneas son del calentamiento de perfilar.sh.
        if [[ -f $CGRA_DESGLOSE_GPU ]]; then
            echo "muestra,contexto_ns,h2d_ns,kernels_ns,d2h_ns" > "$desglose"
            tail -n +6 "$CGRA_DESGLOSE_GPU" | awk '{print NR "," $0}' >> "$desglose"
        fi
        echo "muestra,conteo_ns,encabezado_ns,distribucion_ns,actividad_ns,ciclos_ns,memoria_final_ns" > "$desglose_e5"
        tail -n +6 "$CGRA_DESGLOSE_E5" | awk '{print NR "," $0}' >> "$desglose_e5"
        rm -f "$CGRA_DESGLOSE_GPU" "$CGRA_DESGLOSE_E5"
    done
done
