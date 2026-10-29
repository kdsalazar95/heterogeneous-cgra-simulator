#!/usr/bin/env bash
# Compila build/cgra_gpu: el simulador con malla_cuda.cu en vez de malla.c.
#
# Por defecto usa los compiladores del sistema y sm_53 (Jetson Nano, CUDA 10.2).
# En la PC con la RTX 4060 Ti y el entorno de micromamba:
#
#   env=$HOME/micromamba/envs/cuda
#   CC=$env/bin/x86_64-conda-linux-gnu-gcc CXX=$env/bin/x86_64-conda-linux-gnu-g++ \
#   NVCC=$env/bin/nvcc ARCH=sm_89 CUDA_LIB=$env/lib scripts/compilar_gpu.sh
set -euo pipefail

c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
raiz=$(cd "$c/.." && pwd)              # cgra
CC=${CC:-gcc}
CXX=${CXX:-g++}
NVCC=${NVCC:-/usr/local/cuda/bin/nvcc}
ARCH=${ARCH:-sm_53}
CUDA_LIB=${CUDA_LIB:-}                 # carpeta de libcudart, si nvcc no la encuentra solo
obj=$c/build/gpu
mkdir -p "$obj"

# Los mismos flags que el Makefile; --fmad=false es el -ffp-contract=off de nvcc.
for f in "$c"/src/*.c; do
    [[ $(basename "$f") == malla.c ]] && continue
    $CC -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -O2 -ffp-contract=off -DCGRA_GPU \
        -I"$c/include" -DCGRA_RAIZ="\"$raiz\"" -c "$f" -o "$obj/$(basename "$f" .c).o"
done
$NVCC -O2 --fmad=false -arch="$ARCH" -ccbin "$CXX" -I"$c/include" -c "$c/src/malla_cuda.cu" -o "$obj/malla_cuda.o"
$NVCC -O2 -arch="$ARCH" -ccbin "$CXX" -I"$c/include" -c "$c/src/reporte_cuda.cu" -o "$obj/reporte_cuda.o"
$NVCC -arch="$ARCH" -ccbin "$CXX" "$obj"/*.o -o "$c/build/cgra_gpu" ${CUDA_LIB:+-L"$CUDA_LIB"} -lm

echo "$c/build/cgra_gpu"
