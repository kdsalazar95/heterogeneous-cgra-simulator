#!/usr/bin/env bash
# Compila build/cgra_gpu: el simulador con malla_cuda.cu en vez de malla.c.
set -euo pipefail

c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
raiz=$(cd "$c/.." && pwd)              # cgra
env=$HOME/micromamba/envs/cuda
CC=$env/bin/x86_64-conda-linux-gnu-gcc
CXX=$env/bin/x86_64-conda-linux-gnu-g++
NVCC=$env/bin/nvcc
obj=$c/build/gpu
mkdir -p "$obj"

# Los mismos flags que el Makefile; --fmad=false es el -ffp-contract=off de nvcc.
for f in "$c"/src/*.c; do
    [[ $(basename "$f") == malla.c ]] && continue
    $CC -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -O2 -ffp-contract=off \
        -I"$c/include" -DCGRA_RAIZ="\"$raiz\"" -c "$f" -o "$obj/$(basename "$f" .c).o"
done
$NVCC -O2 --fmad=false -arch=sm_89 -ccbin "$CXX" -I"$c/include" -c "$c/src/malla_cuda.cu" -o "$obj/malla_cuda.o"
$NVCC -arch=sm_89 -ccbin "$CXX" "$obj"/*.o -o "$c/build/cgra_gpu" -L"$env/lib" -lm

echo "$c/build/cgra_gpu"
