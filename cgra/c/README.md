# Simulador CGRA en C

Implementación independiente de Python del simulador de `../src/`.
Emula las 20 instrucciones escalares/vectoriales, los tipos y permisos del
JSON, memoria compartida, comunicaciones, traza y reporte completo.

Desde la raíz del repositorio:

```sh
make -C cgra/c
cgra/c/build/cgra todas
cgra/c/build/cgra matmul --ciclos
cgra/c/build/cgra reduccion --traza
```

También acepta `--cgra`, `--memoria`, `--instrucciones`, `--reporte-ciclos`
y `--write-back`. Las dimensiones se leen del JSON. Conserva el mapa efectivo
del Python, incluida la precedencia de `malla` y la adaptación de mallas
pequeñas; no modifica el archivo de configuración.

El binario incorpora la ruta absoluta de `cgra/` al compilar, para que los
valores predeterminados funcionen desde otro directorio. Si se mueve el
repositorio, se debe recompilar. Las rutas explícitas se interpretan desde
el directorio de ejecución.

## Verificación

```sh
make -C cgra/c test
make -C cgra/c CC=clang-18 BUILD=/tmp/cgra-clang
python3 cgra/c/test/equivalence.py --binary /tmp/cgra-clang/cgra
make -C cgra/c CC=clang-18 BUILD=/tmp/cgra-asan OPT='-O1 -g -fsanitize=address,undefined'
python3 cgra/c/test/equivalence.py --binary /tmp/cgra-asan/cgra
```

Las pruebas diferenciales usan copias temporales de memoria. Comparan salida,
traza, reportes completos y memoria final con el Python actual. Véase
[VALIDACION.md](VALIDACION.md) para los casos ejecutados y límites.

## Perfilado

```sh
OPT=-O2 cgra/c/scripts/perfilar.sh O2 cgra/c/build/cgra reduccion cgra/src/cgra.json 120
uv run cgra/c/scripts/analizar_perfil.py
```

El recolector requiere Python para administrar las muestras; el binario del
simulador no requiere Python ni bibliotecas JSON externas. Los resultados
van a `cgra/c/perfilado/`, ignorado por Git. Guarda la configuración, mapa,
hashes de entradas/binario, entorno y gobernador. Registra en `OPT` los flags
reales del binario: el recolector no puede deducirlos de un ejecutable.

`--perfil MUESTRA` imprime una línea CSV de 15 campos y conserva el reporte
sin traza. Incluye los tiempos de carga de imagen, lectura de programas,
validación/enlace, ejecución, comunicación, cómputo y salida. La configuración
y las asignaciones iniciales se incluyen en el total, fuera de E1–E5. Los
controles estructurales de ciclos ocurren durante la lectura en E2; E3 mide
comunicaciones, permisos y enlace de memoria. E4a/E4b son subconjuntos de E4
y no se suman otra vez al desglose. La instrumentación mide tramos contiguos
sin reordenar la ejecución; su overhead está incluido en E4.

El programa no cambia el gobernador ni aplica optimizaciones de CPU/GPU.
Las muestras de comprobación en `/tmp` no constituyen un estudio de rendimiento.

## Diseño

- `configuracion.c` / `json.c`: lectura C del hardware y mapa efectivo.
- `memoria.c`: imagen little-endian float32, memoria interna en double.
- `programa.c`: ISA completa, validación y direcciones enlazadas.
- `malla.c`: dos pasadas por ciclo, mensajes escalares/vectoriales copiados.
- `salida.c`: consola, traza y reporte completo.
- `main.c`: CLI, resolución de programas, `todas`, medición de etapas.
- `util.c`: asignaciones por corrida, formato y utilidades.

Las asignaciones pertenecen a una corrida y se liberan tanto al terminar como
al rechazar una entrada. Cada mensaje de un programa validado tiene una
recepción en el mismo ciclo; por ello un buzón por sentido de enlace basta,
sin imponer una capacidad arbitraria sobre una cola pendiente.
