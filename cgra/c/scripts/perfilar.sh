#!/usr/bin/env bash
# Recolecta las muestras de perfilado de una variante del simulador (skill 07).
#
#   perfilar.sh <variante> <binario> <programa> <filas> <columnas> [muestras=120]
#
# Ejemplo, desde c/:
#   make OPT=-O0 BUILD=build/O0
#   OPT=-O0 scripts/perfilar.sh O0 build/O0/cgra ../compartido/matmul 4 4
#
# Escribe perfilado/<programa>_<f>x<c>_<variante>.csv (una fila por corrida,
# la cabecera primero; volver a correrlo reemplaza el archivo) y
# perfilado/entorno.txt. OPT y CC, si están definidas, se anotan como los
# flags y el compilador de la variante.
#
# Antes de medir: sudo cpupower frequency-set -g performance, laptop
# enchufada y los demás programas cerrados.

set -euo pipefail

if [ $# -lt 5 ]; then
    sed -n '4,6p' "$0" >&2
    exit 2
fi
variante=$1
binario=$2
programa=$3
filas=$4
columnas=$5
muestras=${6:-120}
calentamiento=5

raiz=$(cd "$(dirname "$0")/.." && pwd)
perfilado="$raiz/perfilado"
mkdir -p "$perfilado"
nombre=$(basename "$programa")
csv="$perfilado/${nombre}_${filas}x${columnas}_${variante}.csv"
reporte=$(mktemp)
trap 'rm -f "$reporte"' EXIT

correr() {
    "$binario" "$programa" --filas "$filas" --columnas "$columnas" \
        --reporte-ciclos "$reporte" --perfil "$1"
}

for ((i = 1; i <= calentamiento; i++)); do
    correr 0 > /dev/null
done

{
    echo "programa,filas,columnas,muestra,memoria_ns,programas_ns,validacion_ns,ejecucion_ns,comunicacion_ns,computo_ns,salida_ns,total_ns,user_us,sys_us,rss_kb,variante"
    for ((i = 1; i <= muestras; i++)); do
        echo "$(correr "$i"),$variante"
    done
} > "$csv.tmp"
mv "$csv.tmp" "$csv"
echo "$muestras muestras en $csv"

# entorno.txt: se reescribe, pero conserva la línea de cada variante ya medida.
entorno="$perfilado/entorno.txt"
linea_variante="variante $variante: CC=${CC:-gcc} OPT=${OPT:-sin indicar} binario=$binario"
anteriores=$(grep '^variante ' "$entorno" 2>/dev/null | grep -v "^variante $variante:" || true)
compilador=${CC:-gcc}
gobernador=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "desconocido")
{
    echo "Fecha: $(date '+%Y-%m-%d %H:%M:%S %z')"
    echo "Commit: $(git -C "$raiz" rev-parse --short HEAD 2>/dev/null || echo desconocido)"
    echo "Gobernador de la CPU: $gobernador"
    echo "Calentamiento: $calentamiento corridas descartadas por configuración"
    echo
    echo "== Variantes =="
    { [ -n "$anteriores" ] && echo "$anteriores"; echo "$linea_variante"; } | sort
    echo
    echo "== Compilador ($compilador --version) =="
    "$compilador" --version | head -1
    echo
    echo "== uname -a =="
    uname -a
    echo
    echo "== lscpu =="
    lscpu
    echo
    echo "== free -h =="
    free -h
} > "$entorno"
