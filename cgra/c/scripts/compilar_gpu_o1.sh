#!/usr/bin/env bash
# Compila build/cgra_gpu_o1: E4 permanece en CPU y solo el reporte de ciclos
# se ejecuta en GPU.
# Con el argumento O2, genera build/cgra_gpu_o2 con el contexto CUDA en paralelo.
set -euo pipefail

c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
raiz=$(cd "$c/.." && pwd)              # cgra
CC=${CC:-gcc}
CXX=${CXX:-g++}
NVCC=${NVCC:-/usr/local/cuda/bin/nvcc}
ARCH=${ARCH:-sm_53}
CUDA_LIB=${CUDA_LIB:-}
case ${1:-O1} in
    O1) variante=o1; flags=() ;;
    O2) variante=o2; flags=(-DCGRA_CUDA_CONTEXTO_ASINCRONO -pthread) ;;
    *) echo "Uso: $0 [O1|O2]" >&2; exit 2 ;;
esac
build=${BUILD:-$c/build}
if [[ $build != /* ]]; then
    build=$c/$build
fi
obj=$build/gpu_$variante
mkdir -p "$obj"
rm -f "$obj"/*.o
cflags=(-std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -O2 -ffp-contract=off -DCGRA_GPU "${flags[@]}" -I"$c/include" -DCGRA_RAIZ="\"$raiz\"")
cuda_flags=(-O2 -arch="$ARCH" -ccbin "$CXX" -I"$c/include")

for f in "$c"/src/*.c; do
    $CC "${cflags[@]}" -c "$f" -o "$obj/$(basename "$f" .c).o"
done
$NVCC "${cuda_flags[@]}" -c "$c/src/reporte_cuda.cu" -o "$obj/reporte_cuda.o"
enlace=()
[[ $variante != o2 ]] || enlace=(-Xcompiler=-pthread)
binario=$build/cgra_gpu_$variante
ldflags=(-arch="$ARCH" -ccbin "$CXX" ${CUDA_LIB:+-L"$CUDA_LIB"} "${enlace[@]}" -lm)
$NVCC "$obj"/*.o -o "$binario" "${ldflags[@]}"
{
    sha256sum < "$binario"
    printf 'C (%s):' "$CC"; printf ' %q' "${cflags[@]}"; echo
    printf 'reporte_cuda.cu (%s):' "$NVCC"; printf ' %q' "${cuda_flags[@]}"; echo
    printf 'Enlace (%s):' "$NVCC"; printf ' %q' "${ldflags[@]}"; echo
    $CC --version
    "$CXX" --version
    $NVCC --version
} > "$binario.compilacion.txt"

echo "$binario"
