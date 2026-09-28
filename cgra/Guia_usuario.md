# Guia de uso

## Requerimientos

| Entorno | Herramienta |
|---|---|
| Python | UV y `python3` |
| Compilacion e IR | `clang-18` |
| Graficas LLVM | `opt-18` |
| Simulador en C | `gcc` y `make` |

No hace falta preparar un entorno virtual: el proyecto no tiene dependencias de
Python fuera de la biblioteca estandar, y `uv run` corre los scripts
directamente (el analisis del perfilado instala sus propias dependencias la
primera vez). Verifica que esten disponibles:

```bash
uv --version
clang-18 --version
opt-18 --version
gcc --version
```

## Estructura del proyecto

Todos los comandos se corren desde esta carpeta, `cgra/`:

```text
cgra/
├── compartido/        lo que usan los dos simuladores
│   ├── memoria_cgra.h formato de memoria.bin (lo incluyen los .c y el simulador en C)
│   ├── matmul/        un programa por carpeta: el .c y lo que generan los skills
│   ├── convolucion/
│   └── reduccion/
├── python/            simulador en Python
│   ├── run_cgra.py    punto de entrada; aqui se configura la malla
│   ├── ...            los demas modulos del simulador
│   └── test/          pruebas
├── c/                 simulador en C (Prototipo en C)
│   ├── src/, include/ codigo
│   ├── scripts/       perfilado
│   └── Makefile
├── agent_skills/      skills del compilador: generan los programas de compartido/
├── agent_skills_c/    skills que crearon c/ (no se vuelven a correr)
├── Guia_usuario.md
└── Guia_perfilado.md
```

Los dos simuladores ejecutan exactamente los mismos programas: se les pasa la
misma carpeta de `compartido/` (por ejemplo `compartido/matmul`).

## Fuentes permitidas

El flujo de skills usa solamente estas fuentes C:

| Operacion | Fuente | Parametros |
|---|---|---|
| Reduccion | `compartido/reduccion/reduccion.c` | `TAMANO_VECTOR` |
| Multiplicacion de matrices | `compartido/matmul/matmul.c` | `TAMANO_MATRIZ` |
| Convolucion | `compartido/convolucion/convolucion.c` | `TAMANO_IMAGEN`, `TAMANO_KERNEL` |

Los tamanos se cambian en los `#define` al inicio de cada fuente. Al cambiar un
tamano hay que volver a ejecutar los skills, porque el horario de los PEs lleva
los indices de memoria ya calculados: si no, la CGRA falla con un indice fuera
de rango. Los skills tambien vuelven a generar `memoria.bin` (ver
[Memoria](#memoria)).

## Configuracion de la CGRA

En `python/run_cgra.py` solo se configura la malla:

```python
FILAS = 4
COLUMNAS = 4
```

`FILAS x COLUMNAS` define la cantidad de PEs. Los skills leen estos valores y
los conservan durante todo el pipeline, y `run_cgra.py` los usa como valores
por defecto al ejecutar.

La CGRA funciona como un procesador: **no se le indica que operacion va a
ejecutar**. Cada PE es un procesador pequeno que ejecuta su programa
(`PE{fila}{columna}.txt`) sobre la memoria (`memoria.bin`), y ambos los
producen los skills a partir del `.c`. La operacion solo se elige al pedir que
se corran los skills, indicando la fuente (por ejemplo, "corre los skills para
`compartido/matmul/matmul.c`").

La malla y el programa no tienen que ser del mismo tamano:

- **Programa mayor que la malla:** se procesa por tiles del tamano de la malla,
  uno tras otro (ver [Operaciones por tiles](#operaciones-por-tiles)).
- **Malla mayor que el programa:** los PEs que sobran quedan en `NOP`. En una
  reduccion arrancan igual con `MOV acc, 0.0`, para no alterar la ruta.

## Preparar el pipeline de skills

Los skills se ejecutan en este orden:

1. Recibir la fuente indicada y leer la malla de `run_cgra.py`.
2. Generar el archivo LLVM con `clang-18`, y compilar y ejecutar el `.c` para
   generar la memoria inicial `memoria.bin`.
3. Generar las graficas CFG y DDG con `opt-18`.
4. Analizar el programa, detectar paralelismo y mapearlo a la malla.
5. Calendarizar las instrucciones.
6. Generar y validar un archivo por PE.

Todo lo que genera el pipeline queda en la carpeta del programa:

```text
compartido/<programa>/
├── <programa>.c             fuente, con sus #define e inicializar_memoria()
├── <programa>.ll            skill 02
├── memoria.bin              skill 02, memoria inicial que escribe el .c
├── .main.dot                skill 03 (CFG)
├── ddg.main..dot            skill 03 (DDG)
├── pe_instructions/         skill 08
│   └── PE{fila}{columna}.txt
└── reporte_ciclos.txt       reporte de la ultima corrida
```

Cada programa tiene su carpeta, asi que compilar uno no afecta a los horarios
de los demas. Al regenerar con la misma malla, los archivos se
sobrescriben. **Si cambias el tamano de la malla, borra antes los `PE*.txt`
anteriores:** con 10 o mas filas o columnas cada coordenada lleva dos digitos
(`PE0100.txt`), asi que los nombres nuevos no pisan a los viejos y quedan los
dos juegos mezclados en la carpeta.

`python/test/datos/reduccion_2x2` no forma parte de esa salida: es el horario de
referencia `2x2` de la reduccion, usado por las pruebas (ver
[Pruebas](#pruebas)).

Los detalles de cada paso estan en `agent_skills/README.md`.

## Operaciones por tiles

La salida de `matmul` o de `convolucion` puede ser mayor que la malla. En ese
caso se procesa por tiles con memoria compartida. Por defecto, el tile tiene el
mismo tamano que la malla. Por ejemplo, una malla `2x2` procesa tiles `2x2`:

```text
PE00 -> elemento local [0][0]
PE01 -> elemento local [0][1]
PE10 -> elemento local [1][0]
PE11 -> elemento local [1][1]
```

Cada PE acumula su elemento y al final lo escribe con `ST`. Un operando que
comparten varios PEs no lo lee cada uno de memoria: entra a la malla una sola
vez y viaja de vecino en vecino con `SEND`/`RECV`, un salto por ciclo:

- **matmul:** el valor de `a` entra por la columna 0 y viaja hacia el este por
  su fila; el de `b` entra por la fila 0 y viaja hacia el sur por su columna.
- **convolucion:** el coeficiente del kernel entra por `PE00` y llega a toda la
  malla; cada PE carga su propio pixel con `LD`.

Los tiles de salida se procesan uno por uno hasta cubrir todo el resultado; en
un tile de borde, los PEs que quedan fuera hacen `NOP`. El detalle esta en
`agent_skills/06-map-to-mesh.md`.

En una reduccion el tiling es distinto: cada PE acumula localmente su parte del
vector y despues se corre una sola vez la ruta de reduccion hacia `PE00`.

## Ejecutar la CGRA

`run_cgra.py` recibe la carpeta del programa, igual que a un procesador se le
da un ejecutable, y carga de ahi `pe_instructions/` y `memoria.bin`:

```bash
uv run python/run_cgra.py compartido/convolucion
```

No hay que indicar la operacion ni el tamano del resultado: al terminar se
muestran las regiones de memoria que escribieron los PEs, cada una con la forma
que declara `memoria.bin` (un valor, un vector o una matriz).

Para correr con una malla distinta de la configurada:

```bash
uv run python/run_cgra.py compartido/reduccion --filas 8 --columnas 8
```

Los `PE*.txt` de la carpeta tienen que corresponder a esa malla.

### Reporte de ciclos

Cada corrida guarda un reporte en `compartido/<programa>/reporte_ciclos.txt`, con un
solo nombre por programa que se reescribe en la corrida siguiente. Contiene:

- La malla, el programa y las regiones de su memoria.
- Cuantos ciclos se van en computo, en comunicacion y cuantos quedan inactivos.
- Cada desplazamiento de datos entre PEs: ciclo, emisor, receptor y direccion.

Con `--ciclos` se muestra ademas en pantalla, y con `--reporte-ciclos RUTA` se
guarda en otra ruta. Si un programa no manda datos entre PEs, el reporte lo
dice explicitamente.

### Memoria

La memoria de la CGRA es binaria, como la de un procesador, y no la arma
Python: la genera el propio `.c`. Cada fuente tiene una funcion
`inicializar_memoria()` que crea e inicializa los arreglos y al final los guarda
en `memoria.bin` con `guardar_memoria()` de `compartido/memoria_cgra.h`. El skill 02
compila y ejecuta el `.c` para producirla; a mano seria:

```bash
cd compartido/matmul
clang-18 matmul.c -o matmul.out && ./matmul.out && rm matmul.out
```

El archivo es una memoria plana de `float32` con una tabla de simbolos al
inicio: el nombre de cada region, su primera palabra y su forma. El formato
exacto esta documentado en `compartido/memoria_cgra.h`.

| Programa | Regiones |
|---|---|
| Reduccion | `a`, `b`, `c`, `result` |
| Matmul | `a`, `b`, `result` (la matriz C) |
| Convolucion | `imagen`, `kernel`, `result` |

Las matrices se guardan aplanadas fila por fila. Los programas de los PEs usan
estos nombres de region en sus `LD`/`ST`.

Para usar otra memoria u otros programas de PE:

```bash
uv run python/run_cgra.py compartido/matmul --memoria otra_memoria.bin
uv run python/run_cgra.py compartido/matmul --instrucciones otra_carpeta/
```

### Opciones utiles

```bash
# Mostrar en pantalla los ciclos de computo y comunicacion
uv run python/run_cgra.py compartido/convolucion --ciclos

# Guardar el reporte en otra ruta
uv run python/run_cgra.py compartido/convolucion \
	--reporte-ciclos programas/reportes_ciclos/reporte.txt

# Guardar la memoria resultante, con los resultados, en el memoria.bin usado
uv run python/run_cgra.py compartido/convolucion --write-back
```

## Simulador en C

`c/` es el mismo simulador escrito en C (el Prototipo en C). Hace lo mismo
que `run_cgra.py`, con las mismas opciones y el mismo reporte de ciclos, y da
los mismos resultados:

```bash
make -C c
c/build/cgra compartido/convolucion
c/build/cgra compartido/reduccion --filas 8 --columnas 8
```

Solo hay que volver a compilarlo si cambia el codigo de `c/`. **No corras
los skills de `agent_skills_c/`:** se usaron una sola vez para crear el
simulador, y volver a correrlos lo reescribiria. Para medir sus tiempos, sigue
`Guia_perfilado.md`.

## Archivos generados

Lo que producen los skills, `run_cgra.py` y el simulador en C no se versiona:
`.gitignore` excluye los `.ll`, los `.dot`, las carpetas `pe_instructions/`,
los `memoria.bin`, los ejecutables `.out`, `c/build/` y los resultados del
perfilado (`c/perfilado/`). Todo eso se vuelve a crear ejecutando el
pipeline, la CGRA y el perfilado.

## Pruebas

```bash
uv run python/test/run_tests.py
```

Las pruebas de ejecucion y de ciclos (`TestEjecucionCGRA` y
`TestEstadisticasCiclos`) usan el horario de referencia de
`python/test/datos/reduccion_2x2/`. Hoy esa carpeta esta vacia, asi que esas 8 pruebas
fallan con `No existe .../PE00.txt` hasta que se vuelva a generar ese horario
(una reduccion de 12 elementos en una malla 2x2) con los skills de
`agent_skills/`.
