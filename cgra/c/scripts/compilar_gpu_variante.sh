#!/usr/bin/env bash
# Compila una variante GPU: build/cgra_gpu_<variante>.
#
#   compilar_gpu_variante.sh <o1|o2|o3>
#
#   o1  E4 en la CPU; E5 en la GPU con copias explícitas.
#   o2  o1 + contexto CUDA creado en un hilo durante E2.
#   o3  o2 + memoria mapeada (zero-copy) en E5.
#
# Variables: CC, CXX, NVCC, ARCH (sm_53 por defecto) y CUDA_LIB.
set -euo pipefail

variante=${1:?uso: compilar_gpu_variante.sh <o1|o2|o3>}
case $variante in
    o1) defs="" ;;
    o2) defs="-DCGRA_O2" ;;
    o3) defs="-DCGRA_O2 -DCGRA_O3" ;;
    *) echo "variante desconocida: $variante" >&2; exit 2 ;;
esac

c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
raiz=$(cd "$c/.." && pwd)              # cgra
CC=${CC:-gcc}
CXX=${CXX:-g++}
NVCC=${NVCC:-/usr/local/cuda/bin/nvcc}
ARCH=${ARCH:-sm_53}
CUDA_LIB=${CUDA_LIB:-}
# E2 (lectura de los PE*.txt) en paralelo, igual que el Makefile. OPENMP= compila E2 secuencial.
OPENMP=${OPENMP--fopenmp}
obj=$c/build/gpu_$variante
rm -rf "$obj"
mkdir -p "$obj"

for f in "$c"/src/*.c; do
    $CC -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -O2 -ffp-contract=off -DCGRA_GPU $defs $OPENMP \
        -I"$c/include" -DCGRA_RAIZ="\"$raiz\"" -c "$f" -o "$obj/$(basename "$f" .c).o"
done
$NVCC -O2 -arch="$ARCH" -ccbin "$CXX" $defs -I"$c/include" -c "$c/src/reporte_cuda.cu" -o "$obj/reporte_cuda.o"
$NVCC -arch="$ARCH" -ccbin "$CXX" ${OPENMP:+-Xcompiler "$OPENMP"} "$obj"/*.o -o "$c/build/cgra_gpu_$variante" \
    ${CUDA_LIB:+-L"$CUDA_LIB"} -lm -lpthread

echo "$c/build/cgra_gpu_$variante"
