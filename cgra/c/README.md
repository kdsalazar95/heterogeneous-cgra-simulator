# Simulador CGRA en C

Port a C11 del simulador en Python de `../src/`, con el mismo comportamiento:
las 20 instrucciones escalares y vectoriales, los tipos de PE y sus permisos
según el JSON, la memoria compartida, las comunicaciones, la traza y el
reporte completo. No necesita Python ni bibliotecas externas.

Desde la raíz del repositorio:

```sh
make -C cgra/c
cgra/c/build/cgra todas
cgra/c/build/cgra matmul --ciclos
cgra/c/build/cgra reduccion --traza
```

También acepta `--cgra`, `--memoria`, `--instrucciones`, `--reporte-ciclos`,
`--write-back` y `--perfil`. `cgra/c/build/cgra --mapa` solo muestra el tamaño,
el mapa de tipos de PE y la leyenda del JSON, sin cargar ningún programa: es
lo que usa el skill 01 de `agent_skills/` para saber para qué hardware compilar. El tamaño de la malla, la memoria, el ancho de
los vectores y los tipos de PE se leen del JSON (`cgra/src/cgra.json` por
defecto) en cada corrida; no hay nada fijo al compilar. El mapa de tipos es el
mismo que arma Python: primero `malla`, después la adaptación de mallas
pequeñas y por último `distribucion`. El simulador no modifica el JSON.

El binario guarda la ruta absoluta de `cgra/` al compilar, para que las rutas
por defecto funcionen desde cualquier directorio. Si se mueve el repositorio,
hay que recompilar. Las rutas que se pasan como argumento se interpretan
desde el directorio actual.

## Módulos

Un módulo por responsabilidad, cada uno con su `.h` en `include/` y su `.c`
en `src/`. Cada cabecera dice qué archivo de Python porta.

| Módulo | Qué hace | Python |
|---|---|---|
| `main` | Línea de comandos, etapas de una corrida, `todas` | `run_cgra.py` |
| `configuracion` | Lee y valida `cgra.json`; arma el mapa de tipos | `formato_cgra.py` |
| `json` | Lector de JSON con los errores de `json.loads()` | `json` |
| `isa` | Opcodes, registros y parseo de una instrucción | `isa.py` |
| `memoria` | `memoria.bin` y la memoria de datos (en `double`) | `memoria.py` |
| `programa` | Lee los `PE*.txt`, los valida y los enlaza con la memoria | `programa.py`, `cargador.py` |
| `malla` | PEs, colas y ejecución ciclo a ciclo | `malla.py`, `cgra.py` |
| `traza` | Qué escribió cada PE en cada ciclo | `traza.py` |
| `salida` | Títulos, cajas y regiones de memoria modificadas | `consola.py`, `resultados.py` |
| `reporte` | `reporte_ciclos.txt`, byte a byte igual al de Python | `reporte_ciclos.py` |
| `formato_numero` | `f"{round(valor, 6):g}"` | `formato_numero.py` |
| `perfil` | Tiempos por etapa y la línea CSV de `--perfil` | — |
| `texto`, `error` | Textos y listas de líneas; el mensaje del último error | — |

Las funciones devuelven 0 si todo salió bien y -1 si no, y dejan el mensaje
en `error.h`; `main.c` decide cómo mostrarlo, igual que `run_cgra.py`. Los
errores que Python no atrapa (una división entre cero, un registro que no
existe) devuelven `ERROR_EJECUCION` y terminan también una corrida de `todas`.

Cada cola entre dos PEs tiene un solo lugar: en un programa validado cada
envío tiene su recepción en el mismo ciclo, y todos los envíos se ejecutan
antes que el resto, así que el dato entra y sale en ese ciclo.

## Verificación

```sh
make -C cgra/c test
make -C cgra/c test CC=clang-18 BUILD=/tmp/cgra-clang
make -C cgra/c test CC=clang-18 BUILD=/tmp/cgra-asan OPT='-O1 -g -fsanitize=address,undefined'
```

`make test` compara el binario con el Python de `../src/` usando
`test/equivalence.py`: salida en consola, traza, reporte completo y memoria
final, con copias temporales de las entradas. Los programas de
`cgra/programas/` se comparan con `cgra/src/cgra.json`; si ese JSON cambió y
los programas todavía no se recompilaron con los skills, se pasa el JSON con
el que se compilaron: `make -C cgra/c test PRUEBA_CGRA=/ruta/al/cgra.json`.
Véase [VALIDACION.md](VALIDACION.md) para los casos ejecutados y los límites.

## Perfilado

```sh
make -C cgra/c OPT=-O2
OPT=-O2 cgra/c/scripts/perfilar.sh O2 cgra/c/build/cgra reduccion cgra/src/cgra.json 120
uv run cgra/c/scripts/analizar_perfil.py
```

`perfilar.sh` corre 5 veces de calentamiento y después las muestras pedidas
con `--perfil`, y deja en `cgra/c/perfilado/` (ignorado por git) un CSV por
programa, malla y variante, más `entorno.txt` con la laptop, el compilador,
el commit, el gobernador, el JSON, la configuración efectiva y los hashes de
las entradas. Registra en `OPT` los flags con los que se compiló el binario:
el script no puede deducirlos de un ejecutable. `analizar_perfil.py` escribe
`resumen.csv` y los gráficos por etapa.

`--perfil MUESTRA` imprime solo una línea CSV de 15 campos y guarda el
reporte sin traza. Las etapas son: E1 carga de `memoria.bin`, E2 lectura de
los programas, E3 validación y enlace, E4 ejecución (con E4a comunicación y
E4b cómputo, que son partes de E4) y E5 reporte y write-back. La lectura del
JSON va solo en el total. E4a y E4b se miden por tramos seguidos de
instrucciones, sin reordenar la ejecución.

El simulador no cambia el gobernador de la CPU.
