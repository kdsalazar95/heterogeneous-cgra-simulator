#!/usr/bin/env bash
# Toma las muestras de tiempo del simulador en C (skill 07).
#
#   perfilar.sh <variante> <binario> <programa> <cgra.json> [muestras=120]
#
# variante nombra el build (O0, O2...) y va como última columna del CSV.
# Los resultados quedan en cgra/c/perfilado/:
#
#   <programa>_<filas>x<columnas>_<variante>.csv   una línea por muestra
#   entorno.txt                                    laptop, compilador, commit, entradas
#
# No cambia el gobernador de la CPU ni los archivos de entrada: el reporte
# de cada corrida va a una carpeta temporal.
set -euo pipefail

error() {
    echo "perfilar.sh: $*" >&2
    exit 2
}

[[ $# -ge 4 && $# -le 5 ]] || error "uso: perfilar.sh <variante> <binario> <programa> <cgra.json> [muestras=120]"
variante=$1
binario=$(realpath "$2")
programa=$3
config=$(realpath "$4")
muestras=${5:-120}
[[ $programa != todas ]] || error "mide una operación por ejecución, no 'todas'"
[[ $muestras =~ ^[0-9]+$ && $muestras -ge 2 ]] || error "se necesitan al menos 2 muestras"

c=$(cd "$(dirname "$0")/.." && pwd)    # cgra/c
raiz=$(cd "$c/.." && pwd)              # cgra
salida=$c/perfilado
carpeta=$programa
[[ -d $carpeta ]] || carpeta=$raiz/programas/$programa
[[ -d $carpeta ]] || error "no existe la carpeta del programa: $programa"
carpeta=$(realpath "$carpeta")
nombre=$(basename "$carpeta")

temporal=$(mktemp -d)
trap 'rm -rf "$temporal"' EXIT
mkdir -p "$salida"

cat >&2 <<'AVISO'
Antes de medir:
  - fija el reloj:     sudo cpupower frequency-set -g performance
  - deja la laptop conectada a la corriente
  - cierra los demás programas
AVISO

# Una corrida de --perfil, revisando que imprima los 15 campos.
medir() {
    local linea
    linea=$("$binario" "$carpeta" --cgra "$config" --perfil "$1" --reporte-ciclos "$temporal/reporte.txt")
    local campos
    IFS=, read -r -a campos <<< "$linea"
    [[ ${#campos[@]} -eq 15 ]] || error "el simulador no imprimió una línea CSV de 15 campos: $linea"
    echo "$linea"
}

# Filas y columnas salen del formato efectivo, en la primera corrida; las
# 5 primeras son de calentamiento y no se guardan.
primera=$(medir 0)
IFS=, read -r _ filas columnas _ <<< "$primera"
for _ in 1 2 3 4; do
    medir 0 > /dev/null
done

csv=$salida/${nombre}_${filas}x${columnas}_${variante}.csv
echo "programa,filas,columnas,muestra,memoria_ns,programas_ns,validacion_ns,ejecucion_ns,comunicacion_ns,computo_ns,salida_ns,total_ns,user_us,sys_us,rss_kb,variante" > "$csv"
for ((i = 1; i <= muestras; i++)); do
    linea=$(medir "$i")
    echo "$linea,$variante" >> "$csv"
done

{
    echo "=== ${nombre}_${filas}x${columnas}_${variante}: $(date -Iseconds)"
    echo "Commit:      $(git -C "$raiz" rev-parse --short HEAD 2>/dev/null || echo 'no disponible')"
    echo "Gobernador:  $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'no disponible')"
    echo "Binario:     $binario"
    echo "             $(sha256sum "$binario" | cut -d' ' -f1)"
    echo "OPT:         ${OPT:-no declarado (registrar los flags con los que se compiló el binario)}"
    echo "Muestras:    $muestras, después de 5 corridas de calentamiento"
    echo "--- Compilador (${CC:-cc})"
    "${CC:-cc}" --version 2>/dev/null | head -1 || echo "no disponible"
    echo "--- $config"
    cat "$config"
    echo
    echo "--- Configuración efectiva (sección 1 del reporte)"
    sed -n '/^1\. CONFIGURACIÓN/,/^2\. RESULTADO/p' "$temporal/reporte.txt" | sed '$d'
    echo "--- Entradas (sha256)"
    sha256sum "$carpeta/memoria.bin" "$carpeta"/pe_instructions/PE*.txt
    echo "--- lscpu"
    lscpu
    echo "--- free -h"
    free -h
    echo "--- uname -a"
    uname -a
    echo
} >> "$salida/entorno.txt"

echo "$csv"
