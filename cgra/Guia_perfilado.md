# Guia de perfilado

## Que es el perfilado

Es medir cuanto tarda el simulador en C y en que parte se va el tiempo. Es como
cronometrar a alguien que hace una receta: no solo cuanto tarda en total, sino
cuanto en picar, cocinar y servir. Asi se sabe que parte conviene acelerar.

El simulador tiene cinco partes, y se cronometra cada una:

1. Leer la memoria (`memoria.bin`)
2. Leer los programas de los PEs (`PE*.txt`)
3. Revisar que los programas esten bien
4. Ejecutar la malla (casi seguro, la parte que mas tarda)
5. Escribir el reporte de ciclos

**Por que 120 veces.** Una sola medicion puede salir mas lenta o mas rapida por
casualidad (por ejemplo, si la computadora estaba haciendo otra cosa). Con 120
se saca un promedio confiable y se ve cuanto varia. El enunciado pide mas de
100.

**Que son las variantes.** Es el mismo simulador compilado de distintas formas:
el compilador puede optimizar nada (`-O0`), bastante (`-O2`) o mas (`-O3`). Midiendo cada una se
ve cuanto mejora. Eso es lo que el enunciado llama optimizaciones.

**Por que fijar la frecuencia.** La laptop cambia su velocidad sola para ahorrar
energia. Si cambia mientras mides, los tiempos salen desordenados. Fijarla es
como cronometrar siempre en la misma pista.

## Antes de empezar

El simulador en C (`cgra_c/`) ya esta en el repositorio: **no corras los skills
de `agent_skills_c/`**, porque reescribirian el simulador. Solo actualiza tu
copia:

```bash
git pull
```

Lo que no esta en el repositorio son los programas de los PEs (`PE*.txt`) y la
memoria de cada programa (`memoria.bin`). Se generan con los skills de
`agent_skills/`, uno por programa. Pidele a un agente:

> Corre los skills de `agent_skills/` para `src/matmul/matmul.c`.

y lo mismo para `src/convolucion/convolucion.c` y
`src/reduccion/reduccion.c`. Los programas quedan para la malla que dicen
`FILAS` y `COLUMNAS` en `src/run_cgra.py` (hoy 4x4).

Todos los comandos de abajo se corren desde la carpeta `Colas _reduccion/`.

## Pasos

### 1. Compilar el simulador de tres formas

```bash
make -C cgra_c OPT=-O0 BUILD=build/O0   # sin optimizar
make -C cgra_c OPT=-O2 BUILD=build/O2   # optimizado
make -C cgra_c OPT=-O3 BUILD=build/O3   # mas optimizado
```

### 2. Poner la laptop a velocidad fija

Conecta la laptop a la corriente y cierra los demas programas. Despues:

```bash
sudo cpupower frequency-set -g performance
```

Pide tu contrasena. Si dice que `cpupower` no existe, instalalo con
`sudo apt install linux-tools-common linux-tools-$(uname -r)`.

### 3. Medir

Primero borra las mediciones anteriores, para que no se mezclen con las
nuevas:

```bash
rm -f cgra_c/perfilado/*.csv
```

Despues corre el simulador 120 veces por cada programa y variante, y guarda
los tiempos:

```bash
cd cgra_c

./scripts/perfilar.sh O0 build/O0/cgra ../src/matmul 4 4
./scripts/perfilar.sh O2 build/O2/cgra ../src/matmul 4 4
./scripts/perfilar.sh O3 build/O3/cgra ../src/matmul 4 4

./scripts/perfilar.sh O0 build/O0/cgra ../src/convolucion 4 4
./scripts/perfilar.sh O2 build/O2/cgra ../src/convolucion 4 4
./scripts/perfilar.sh O3 build/O3/cgra ../src/convolucion 4 4

./scripts/perfilar.sh O0 build/O0/cgra ../src/reduccion 4 4
./scripts/perfilar.sh O2 build/O2/cgra ../src/reduccion 4 4
./scripts/perfilar.sh O3 build/O3/cgra ../src/reduccion 4 4

cd ..
```

Los dos numeros del final son la malla: tienen que ser `FILAS` y `COLUMNAS`
de `src/run_cgra.py`, la malla con la que se generaron los programas. No uses
la laptop mientras mide.

Tus mediciones quedan en `cgra_c/perfilado/`, solo en tu laptop: git ignora
esa carpeta, asi que no se suben al repositorio ni pisan las de otra persona.

### 4. Sacar promedios y graficos

```bash
uv run cgra_c/scripts/analizar_perfil.py
```

Los resultados quedan en `cgra_c/perfilado/`: una tabla con los promedios
(`resumen.csv`) y dos graficos.

El analisis tambien imprime las tablas en pantalla. Con eso termina el
perfilado.

### 5. Dejar la laptop como estaba

```bash
sudo cpupower frequency-set -g powersave
```

## Reporte escrito (opcional)

Si quieres un reporte completo en `cgra_c/REPORTE_PERFILADO.md`, despues del
paso 4 pidele a un agente lo siguiente. Si ya habia un reporte, hay que
regenerarlo asi despues de cada medicion nueva: si no, queda con los numeros
viejos.

> Sigue el skill `agent_skills_c/09-profiling-report.md`.

## Si algo falla

| Mensaje o problema | Que hacer |
|---|---|
| Los tiempos varian mucho | Repetir el paso 2 y volver a medir |
| `No existe src/<programa>/memoria.bin` | `cd src/<programa> && clang-18 <programa>.c -o <programa>.out && ./<programa>.out && rm <programa>.out` |
| `No existe .../pe_instructions/PE00.txt` | Faltan los programas de los PEs: generalos con `agent_skills/` (ver "Antes de empezar") |
| `PE03: no tiene vecino hacia east` | La malla del comando no es la de los programas: usa `FILAS` y `COLUMNAS` de `src/run_cgra.py`, o regenera los programas con `agent_skills/` |
