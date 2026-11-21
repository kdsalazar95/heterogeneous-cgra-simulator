#!/usr/bin/env bash
# Arma cgra/c/perfilado/entradas/<malla>/ como perfilar_cpu.sh, pero sin medir:
# descomprime Programas_PEs/*.zip, genera memoria.bin con operaciones.c y el
# cgra.json de cada malla. Lo usan verificar_fpga.sh y perfilar_fpga.sh.
set -euo pipefail

fpga=$(cd "$(dirname "$0")" && pwd)
raiz=${CGRA_RAIZ_DIR:-$(cd "$fpga/.." && pwd)}   # cgra
repo=${CGRA_REPO_DIR:-$(cd "$raiz/.." && pwd)}
entradas=$raiz/c/perfilado/entradas
CC=${CC:-gcc}

memorias=$entradas/memorias
mkdir -p "$memorias"
$CC "$raiz/operaciones/operaciones.c" -o "$memorias/operaciones" -lm
for programa in reduccion matmul convolucion; do
    mkdir -p "$memorias/$programa"
    (cd "$memorias/$programa" && ../operaciones "$programa" > /dev/null)
done

for zip in "$repo"/Programas_PEs/pe_programas_*.zip; do
    malla=$(basename "$zip" .zip)
    malla=${malla#pe_programas_}
    filas=${malla%x*}
    columnas=${malla#*x}
    destino=$entradas/$malla
    [[ -d $destino ]] || { mkdir -p "$destino"; python3 -m zipfile -e "$zip" "$destino"; }
    sed -e "s/\"filas\": [0-9]*/\"filas\": $filas/" -e "s/\"columnas\": [0-9]*/\"columnas\": $columnas/" \
        "$raiz/src/cgra.json" > "$destino/cgra.json"
    for programa in reduccion matmul convolucion; do
        carpeta=$(find "$destino" -type d -path "*/$programa" | head -1)
        [[ -f $carpeta/memoria.bin ]] || cp "$memorias/$programa/memoria.bin" "$carpeta/"
    done
done
echo "$entradas"
