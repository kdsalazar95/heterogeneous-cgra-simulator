# Guia de uso

## Requerimientos

| Entorno | Herramienta |
|---|---|
| Python | UV |
| Compilacion e IR | `clang-18` |
| Graficas LLVM | `opt-18` |

Desde la carpeta del proyecto, sincroniza el entorno:

```bash
uv sync
uv venv
```

El proyecto usa `uv run`, por lo que no es necesario activar manualmente el
entorno virtual. Verifica que tu sistema tenga disponibles `python3`,
`clang-18` y `opt-18`.

## Fuentes permitidas

El flujo de skills usa solamente estas fuentes C:

| Operacion | Fuente | Parametros |
|---|---|---|
| Reduccion | `src/reduccion/reduccion.c` | `TAMANO_VECTOR` |
| Multiplicacion de matrices | `src/matmul/matmul.c` | `TAMANO_MATRIZ` |
| Convolucion | `src/convolucion/convolucion.c` | `TAMANO_IMAGEN`, `TAMANO_KERNEL` |

Los tamanos se cambian en los `#define` al inicio de cada fuente. Al cambiar un
tamano hay que volver a ejecutar los skills, porque el horario de los PEs lleva
los indices de memoria ya calculados: si no, la CGRA falla con un indice fuera
de rango. Los skills tambien vuelven a generar `memoria.bin` (ver
[Memoria](#memoria)).

## Configuracion de la CGRA

En `src/run_cgra.py` solo se configura la malla:

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
`src/matmul/matmul.c`").

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
src/<programa>/
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

`test/datos/reduccion_2x2` no forma parte de esa salida: es el horario de
referencia `2x2` de la reduccion, usado por las pruebas.

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

Cada PE lee directamente sus operandos mediante `LD`, acumula su resultado
local y escribe una posicion del resultado mediante `ST`. No se usan
`SEND`/`RECV` para transportar operandos. Los tiles de salida se procesan uno
por uno hasta cubrir todo el resultado; en un tile de borde, los PEs que quedan
fuera hacen `NOP`.

En una reduccion el tiling es distinto: cada PE acumula localmente su parte del
vector y despues se corre una sola vez la ruta de reduccion hacia `PE00`.

## Ejecutar la CGRA

`run_cgra.py` recibe la carpeta del programa, igual que a un procesador se le
da un ejecutable, y carga de ahi `pe_instructions/` y `memoria.bin`:

```bash
uv run src/run_cgra.py src/convolucion
```

No hay que indicar la operacion ni el tamano del resultado: al terminar se
muestran las regiones de memoria que escribieron los PEs, cada una con la forma
que declara `memoria.bin` (un valor, un vector o una matriz).

Para correr con una malla distinta de la configurada:

```bash
uv run src/run_cgra.py src/reduccion --filas 8 --columnas 8
```

Los `PE*.txt` de la carpeta tienen que corresponder a esa malla.

### Reporte de ciclos

Cada corrida guarda un reporte en `src/<programa>/reporte_ciclos.txt`, con un
solo nombre por programa que se reescribe en la corrida siguiente. Contiene:

- La malla, el programa y las regiones de su memoria.
- Cuantos ciclos se van en computo, en comunicacion y cuantos quedan inactivos.
- Cada desplazamiento de datos entre PEs: ciclo, emisor, receptor y direccion.

Con `--ciclos` se muestra ademas en pantalla, y con `--reporte-ciclos RUTA` se
guarda en otra ruta. En `matmul` y `convolucion` no hay comunicacion entre PEs,
asi que el reporte lo dice explicitamente.

### Memoria

La memoria de la CGRA es binaria, como la de un procesador, y no la arma
Python: la genera el propio `.c`. Cada fuente tiene una funcion
`inicializar_memoria()` que crea e inicializa los arreglos y al final los guarda
en `memoria.bin` con `guardar_memoria()` de `src/memoria_cgra.h`. El skill 02
compila y ejecuta el `.c` para producirla; a mano seria:

```bash
cd src/matmul
clang-18 matmul.c -o matmul.out && ./matmul.out && rm matmul.out
```

El archivo es una memoria plana de `float32` con una tabla de simbolos al
inicio: el nombre de cada region, su primera palabra y su forma. El formato
exacto esta documentado en `src/memoria_cgra.h`.

| Programa | Regiones |
|---|---|
| Reduccion | `a`, `b`, `c`, `result` |
| Matmul | `a`, `b`, `result` (la matriz C) |
| Convolucion | `imagen`, `kernel`, `result` |

Las matrices se guardan aplanadas fila por fila. Los programas de los PEs usan
estos nombres de region en sus `LD`/`ST`.

Para usar otra memoria u otros programas de PE:

```bash
uv run src/run_cgra.py src/matmul --memoria otra_memoria.bin
uv run src/run_cgra.py src/matmul --instrucciones otra_carpeta/
```

### Opciones utiles

```bash
# Mostrar en pantalla los ciclos de computo y comunicacion
uv run src/run_cgra.py src/convolucion --ciclos

# Guardar el reporte en otra ruta
uv run src/run_cgra.py src/convolucion \
	--reporte-ciclos programas/reportes_ciclos/reporte.txt

# Guardar la memoria resultante, con los resultados, en el memoria.bin usado
uv run src/run_cgra.py src/convolucion --write-back
```

## Archivos generados

Lo que producen los skills y `run_cgra.py` no se versiona: `.gitignore` excluye
los `.ll`, los `.dot`, las carpetas `pe_instructions/`, los `memoria.bin` y los
ejecutables `.out`.
Todo eso se vuelve a crear ejecutando el pipeline y la CGRA.

## Pruebas

```bash
uv run test/run_tests.py
```
