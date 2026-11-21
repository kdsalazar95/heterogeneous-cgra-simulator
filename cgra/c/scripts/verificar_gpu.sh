#!/usr/bin/env bash
# Compara CPU y GPU: salida, reporte y memoria final deben ser idénticos.
# Usa las entradas de perfilar_cpu.sh o las rutas indicadas en el entorno.
set -uo pipefail

c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
entradas=${ENTRADAS:-$c/perfilado/entradas}
bin_cpu=${BIN_CPU:-$c/build/cgra}
bin_gpu=${BIN:-$c/build/cgra_gpu}
if [[ $bin_cpu != /* ]]; then
    bin_cpu=$c/$bin_cpu
fi
if [[ $bin_gpu != /* ]]; then
    bin_gpu=$c/$bin_gpu
fi
if [[ ! -x $bin_cpu || ! -x $bin_gpu ]]; then
    echo "verificar_gpu.sh: indique binarios ejecutables con BIN_CPU y BIN" >&2
    exit 2
fi
shopt -s nullglob
destinos=("$entradas"/*x*)
if [[ ! -d $entradas || ${#destinos[@]} -eq 0 ]]; then
    echo "verificar_gpu.sh: no hay entradas de mallas en $entradas" >&2
    exit 2
fi
temporal=$(mktemp -d) || exit 1
trap 'rm -rf "$temporal"' EXIT
iguales=0
distintos=0

# Corre un binario en su carpeta, con una copia de la memoria.
correr() {
    local binario=$1 carpeta=$2 config=$3 destino=$4
    [[ -n $carpeta && -d $carpeta ]] || return 1
    mkdir -p "$destino" || return 1
    cp "$carpeta/memoria.bin" "$destino/mem.bin" || return 1
    (cd "$destino" && "$binario" "$carpeta" --cgra "$config" --memoria mem.bin --write-back \
        --reporte-ciclos rep.txt > salida.txt 2>&1) && [[ -s $destino/rep.txt ]]
}

for destino in "${destinos[@]}"; do
    malla=$(basename "$destino")
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        rm -rf "$temporal"/*
        fallo=0
        for version in cpu gpu; do
            binario=$bin_cpu
            [[ $version != gpu ]] || binario=$bin_gpu
            if ! correr "$binario" "$carpeta" "$destino/cgra.json" "$temporal/$version"; then
                echo "FALLÓ ${version^^}  $programa $malla"
                if [[ -s $temporal/$version/salida.txt ]]; then
                    cat "$temporal/$version/salida.txt"
                fi
                fallo=1
            fi
        done
        if [[ $fallo -ne 0 ]]; then
            distintos=$((distintos + 1))
        elif diff -r "$temporal/cpu" "$temporal/gpu" > /dev/null; then
            echo "igual     $programa $malla"
            iguales=$((iguales + 1))
        else
            echo "DISTINTO  $programa $malla"
            diff -r "$temporal/cpu" "$temporal/gpu" | head -5
            distintos=$((distintos + 1))
        fi
    done
done

echo "iguales: $iguales, distintos: $distintos"
[[ $iguales -eq 18 && $distintos -eq 0 ]]
