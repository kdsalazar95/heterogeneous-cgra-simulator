#!/usr/bin/env bash
# Compila el simulador con E4 en la FPGA: malla_fpga.c en vez de malla.c.
#
#   MODO=hw compilar_fpga.sh   -> cgra/c/build/cgra_fpga        (en la Kria; usa XRT y /dev/mem)
#   MODO=sw compilar_fpga.sh   -> cgra/c/build/cgra_fpga_sw_v2  (en cualquier PC; emula el kernel)
#   MODO=hw OMP=1 ...          -> cgra/c/build/cgra_fpga_omp    (host v3: empaca con OpenMP)
#
# En modo sw, VARIANTE=1|2 elige la variante del kernel (en hw la variante
# está en el bitstream). E5 (el reporte) queda en la CPU.
set -euo pipefail

fpga=$(cd "$(dirname "$0")" && pwd)              # cgra/fpga
raiz=${CGRA_RAIZ_DIR:-$(cd "$fpga/.." && pwd)}   # cgra
c=$raiz/c
MODO=${MODO:-hw}
VARIANTE=${VARIANTE:-2}
CC=${CC:-gcc}
CXX=${CXX:-g++}
# E2 (lectura de los PE*.txt) en paralelo, igual que el Makefile. OPENMP= compila E2 secuencial.
OPENMP=${OPENMP--fopenmp}
obj=$c/build/fpga_${MODO}_v$VARIANTE
mkdir -p "$obj"

# Los mismos flags que el Makefile (sin -ffp-contract el resultado no coincide con Python).
flags=(-std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -O2 -ffp-contract=off $OPENMP -I"$c/include" -I"$fpga"
       -DCGRA_RAIZ="\"$raiz\"")
for f in "$c"/src/*.c; do
    [[ $(basename "$f") == malla.c ]] && continue
    $CC "${flags[@]}" -c "$f" -o "$obj/$(basename "$f" .c).o"
done

if [[ $MODO == hw ]]; then
    # OMP=1: empaca las instrucciones con los 4 núcleos (host v3) -> cgra_fpga_omp
    sufijo=""; omp=()
    if [[ ${OMP:-0} == 1 ]]; then sufijo=_omp; omp=(-fopenmp -DCGRA_FPGA_OMP); fi
    $CC "${flags[@]}" "${omp[@]}" -DCGRA_FPGA_HW -c "$fpga/malla_fpga.c" -o "$obj/malla_fpga.o"
    $CC $OPENMP "${omp[@]}" "$obj"/*.o -o "$c/build/cgra_fpga$sufijo" -L/usr/lib -lxrt_core -lm
    echo "$c/build/cgra_fpga$sufijo"
else
    $CC "${flags[@]}" -c "$fpga/malla_fpga.c" -o "$obj/malla_fpga.o"
    $CXX -std=c++14 -O2 -ffp-contract=off -DVARIANTE="$VARIANTE" -I"$fpga" -c "$fpga/cgra_hls.cpp" -o "$obj/cgra_hls.o"
    $CXX $OPENMP "$obj"/*.o -o "$c/build/cgra_fpga_sw_v$VARIANTE" -lm
    echo "$c/build/cgra_fpga_sw_v$VARIANTE"
fi
