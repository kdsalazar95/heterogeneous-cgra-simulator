# Reporte de perfilado: simulador de la CGRA en C

Prototipo en C del simulador de la CGRA, ejecutado solo en CPU en una laptop.
Todos los números de este reporte salen de `perfilado/resumen.csv` y
`perfilado/entorno.txt`; se regeneran con los comandos de la sección 6.

## 1. Metodología

**Etapas medidas.** Cada corrida del simulador mide sus etapas con
`clock_gettime(CLOCK_MONOTONIC)`:

| Etapa | Columna | Qué se mide |
|---|---|---|
| E1 | `memoria_ns` | cargar `memoria.bin` (y la copia del estado inicial) |
| E2 | `programas_ns` | leer y parsear los `PE*.txt` |
| E3 | `validacion_ns` | validación (ciclos consecutivos, `SEND`/`RECV` emparejados) |
| E4 | `ejecucion_ns` | todo el ciclo de ejecución de la malla |
| E4a | `comunicacion_ns` | pasada de `SEND` y pasada de `RECV`, acumuladas por ciclo |
| E4b | `computo_ns` | pasada del resto de instrucciones, acumulada por ciclo |
| E5 | `salida_ns` | armar y escribir `reporte_ciclos.txt` (y el write-back, si se pide) |

E4a y E4b se acumulan por fase de cada ciclo, no por instrucción: leer el reloj
por instrucción costaría más que la instrucción. La diferencia E4 − E4a − E4b es
la creación de la malla y el control del ciclo; `total_ns` además incluye la
lectura de argumentos y la liberación de memoria ("fuera de etapas" en la
gráfica de desglose).

**Muestras.** Con `--perfil <muestra>` el simulador no imprime nada salvo una
línea CSV con los tiempos y, de `getrusage()`, `user_us`, `sys_us` y `rss_kb`.
`scripts/perfilar.sh` hace 5 corridas de calentamiento descartadas y luego
**120 muestras por configuración**, así que cada etapa tiene 120 muestras (más
de las 100 que pide el prototipo). Se midieron 3 programas × 3 variantes = 9
configuraciones, 1080 corridas en total.

**Estadística.** `scripts/analizar_perfil.py` calcula por programa, malla,
variante y etapa: media, mediana, desviación estándar, mínimo, máximo, p95,
intervalo de confianza del 95 % (media ± 1.96·σ/√n), la parte del total que
ocupa cada etapa y la aceleración de cada variante sobre `O0` (media de `O0` /
media de la variante).

**Reloj de la CPU.** El gobernador registrado en `entorno.txt` al recolectar es
`performance`. No se fijó con `sudo cpupower frequency-set -g performance`
porque la sesión no tenía `sudo`; conviene fijarlo así antes de repetir la
medición, con la laptop enchufada y los demás programas cerrados.

**Herramientas complementarias.** `perf stat` y `perf record` no se pudieron
usar: el kernel tiene `perf_event_paranoid = 4`, que bloquea los contadores sin
privilegios. `gprof` tampoco sirvió: una corrida dura unos pocos milisegundos,
menos que su intervalo de muestreo (10 ms), así que no acumula tiempo, y con
`-O2` atribuye las llamadas de funciones en línea a la función equivocada. En su
lugar se usó `valgrind --tool=callgrind`, que cuenta las instrucciones
ejecutadas por función de forma exacta; su salida está en
`perfilado/funciones_<programa>_O2.txt`.

**Equivalencia.** Antes de medir, el simulador en C se comparó con el de Python
(skill 08) en los tres programas: el `reporte_ciclos.txt` y la memoria final
son idénticos byte a byte, los mensajes de error coinciden, y compila sin
advertencias con `gcc` y `clang-18` y sin reportes con
`-fsanitize=address,undefined`.

## 2. Plataforma

Datos de `perfilado/entorno.txt`:

| | |
|---|---|
| CPU | AMD Ryzen 7 7730U with Radeon Graphics, 8 núcleos, 2 hilos por núcleo (16 CPUs), máx. 4547.9 MHz |
| Caché | L2 4 MiB (8 instancias), L3 16 MiB |
| RAM | 14 GiB |
| Sistema operativo | Linux 7.0.0-31-generic (Ubuntu), x86_64 |
| Compilador | gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |
| Gobernador | `performance` |
| Commit | `416b9c6` (el código de `cgra_c/` aún no estaba confirmado en ese commit) |

## 3. Resultados

Media en µs con su intervalo de confianza del 95 % y, entre paréntesis, la
parte del tiempo total. 120 muestras por celda.

### reduccion, malla 8x8 (42 ciclos)

| Etapa | O0 | O2 | O3 |
|---|---:|---:|---:|
| E1 memoria | 18.19 [17.44, 18.94] (1.3 %) | 16.31 [15.66, 16.97] (1.6 %) | 15.25 [14.75, 15.76] (1.6 %) |
| E2 programas | 1194.08 [1165.41, 1222.74] (84.5 %) | 858.12 [836.30, 879.94] (83.5 %) | 779.00 [765.63, 792.38] (83.3 %) |
| E3 validación | 55.81 [53.62, 58.00] (3.9 %) | 40.27 [38.04, 42.51] (3.9 %) | 37.16 [35.76, 38.55] (4.0 %) |
| E4 ejecución | 39.86 [38.71, 41.01] (2.8 %) | 18.62 [17.59, 19.64] (1.8 %) | 16.89 [16.35, 17.42] (1.8 %) |
| E4a comunicación | 16.63 [16.21, 17.06] (1.2 %) | 5.21 [4.79, 5.64] (0.5 %) | 4.67 [4.52, 4.83] (0.5 %) |
| E4b cómputo | 16.26 [15.84, 16.68] (1.2 %) | 7.35 [7.00, 7.69] (0.7 %) | 6.72 [6.56, 6.89] (0.7 %) |
| E5 salida | 71.31 [69.32, 73.30] (5.0 %) | 61.30 [58.05, 64.54] (6.0 %) | 55.39 [53.40, 57.39] (5.9 %) |
| **Total** | **1413.00 [1380.24, 1445.75]** | **1027.76 [1000.59, 1054.92]** | **935.20 [919.25, 951.16]** |

### matmul, malla 4x4 (828 ciclos)

| Etapa | O0 | O2 | O3 |
|---|---:|---:|---:|
| E1 memoria | 19.02 [18.15, 19.89] (0.4 %) | 16.99 [15.79, 18.20] (0.6 %) | 17.32 [16.44, 18.19] (0.7 %) |
| E2 programas | 3564.98 [3503.11, 3626.84] (72.2 %) | 1725.36 [1685.15, 1765.58] (62.6 %) | 1557.26 [1498.83, 1615.70] (60.0 %) |
| E3 validación | 429.03 [418.98, 439.08] (8.7 %) | 319.33 [313.71, 324.96] (11.6 %) | 322.96 [314.05, 331.88] (12.4 %) |
| E4 ejecución | 211.21 [205.87, 216.55] (4.3 %) | 83.01 [81.61, 84.41] (3.0 %) | 80.52 [79.02, 82.01] (3.1 %) |
| E4a comunicación | 105.30 [102.72, 107.89] (2.1 %) | 30.19 [29.49, 30.89] (1.1 %) | 30.57 [29.94, 31.20] (1.2 %) |
| E4b cómputo | 84.66 [82.53, 86.78] (1.7 %) | 34.30 [33.75, 34.85] (1.2 %) | 30.85 [30.07, 31.62] (1.2 %) |
| E5 salida | 627.06 [614.93, 639.19] (12.7 %) | 537.42 [528.91, 545.94] (19.5 %) | 545.84 [536.32, 555.36] (21.0 %) |
| **Total** | **4935.32 [4866.99, 5003.66]** | **2756.53 [2710.48, 2802.58]** | **2597.42 [2528.83, 2666.01]** |

### convolucion, malla 4x4 (1458 ciclos)

| Etapa | O0 | O2 | O3 |
|---|---:|---:|---:|
| E1 memoria | 19.71 [18.81, 20.61] (0.3 %) | 16.81 [16.03, 17.59] (0.4 %) | 16.94 [16.34, 17.54] (0.5 %) |
| E2 programas | 5848.74 [5770.00, 5927.47] (77.8 %) | 2800.85 [2757.01, 2844.70] (70.7 %) | 2480.13 [2437.87, 2522.38] (68.2 %) |
| E3 validación | 565.64 [553.92, 577.36] (7.5 %) | 411.49 [402.04, 420.94] (10.4 %) | 413.06 [405.37, 420.75] (11.4 %) |
| E4 ejecución | 355.12 [349.23, 361.00] (4.7 %) | 148.63 [146.17, 151.09] (3.8 %) | 144.38 [141.84, 146.92] (4.0 %) |
| E4a comunicación | 166.01 [163.02, 169.01] (2.2 %) | 50.11 [49.08, 51.15] (1.3 %) | 50.71 [49.80, 51.62] (1.4 %) |
| E4b cómputo | 153.95 [151.25, 156.65] (2.0 %) | 66.21 [64.96, 67.46] (1.7 %) | 60.72 [59.25, 62.18] (1.7 %) |
| E5 salida | 596.10 [582.52, 609.69] (7.9 %) | 465.04 [450.81, 479.26] (11.7 %) | 461.91 [453.43, 470.38] (12.7 %) |
| **Total** | **7517.87 [7428.34, 7607.40]** | **3962.23 [3904.40, 4020.05]** | **3637.59 [3587.31, 3687.87]** |

Las demás estadísticas (mediana, σ, mínimo, máximo, p95, `rss_kb`) están en
`perfilado/resumen.csv` y en los CSV crudos.

![Tiempo por etapa, diagrama de cajas](perfilado/cajas_por_etapa.png)

![Desglose del tiempo total por etapa](perfilado/desglose_por_etapa.png)

## 4. Optimizaciones aplicadas y no aplicadas

Aceleración de la media sobre `O0`:

| Programa | Variante | Total | E2 programas | E4 ejecución |
|---|---|---:|---:|---:|
| reduccion 8x8 | O2 | 1.37 | 1.39 | 2.14 |
| reduccion 8x8 | O3 | 1.51 | 1.53 | 2.36 |
| matmul 4x4 | O2 | 1.79 | 2.07 | 2.54 |
| matmul 4x4 | O3 | 1.90 | 2.29 | 2.62 |
| convolucion 4x4 | O2 | 1.90 | 2.09 | 2.39 |
| convolucion 4x4 | O3 | 2.07 | 2.36 | 2.46 |

| Optimización | Estado | Justificación |
|---|---|---|
| Flags del compilador `-O2` | **Aplicada** (valor por defecto del `Makefile`) | Acelera el total entre 1.37 y 1.90 veces y el ciclo de ejecución (E4) entre 2.14 y 2.54 veces, sin cambiar los resultados (skill 08). |
| Flags del compilador `-O3` | Medida, no adoptada todavía | Sobre `-O2` gana poco: el total baja de 2756.53 a 2597.42 µs en matmul y de 3962.23 a 3637.59 µs en convolución, pero E3 y E5 no mejoran (matmul E5: 537.42 contra 545.84 µs). Se puede adoptar tras repetir la medición con el reloj fijado. |
| `-march=native` | No aplicada | Queda para el estudio de optimizaciones: ata el binario a esta CPU y todavía no se midió. |
| Paralelismo de tareas (OpenMP) | No aplicada | La parte paralelizable, E4, ocupa entre 1.8 % y 4.7 % del total, así que su efecto máximo sobre el total es pequeño. Además, cada ciclo tiene solo 16 o 64 PEs y dos barreras (después de los `SEND` y al final), así que sincronizar hilos por ciclo costaría más que el trabajo del ciclo. Lo que domina, E2, se reparte naturalmente por archivo: es el primer candidato para paralelizar. |
| Ordenamiento y alineamiento de datos | No aplicada | Las instrucciones ya están resueltas al cargar (sin textos en el ciclo) y la memoria es plana. El arreglo `por_pe` está ordenado por PE (`[pe * ciclos + ciclo]`) y el ciclo lo recorre por PE dentro de cada ciclo; ordenarlo por ciclo (`[ciclo * pes + pe]`) volvería contiguos los accesos. No se aplicó porque E4 no es el cuello de botella; queda como la primera optimización de E4. |

## 5. Cuello de botella y siguientes pasos

**E2, leer y parsear los `PE*.txt`, domina en todas las configuraciones**: entre
60.0 % (matmul O3) y 84.5 % (reduccion O0) del total. Callgrind lo confirma: en
`-O2`, `leer_programas` encabeza las instrucciones ejecutadas en los tres
programas, seguida de funciones de la biblioteca de C: `strtoul`, `strcmp`,
`strstr` y `strlen` del parser, y el `snprintf` que arma los IDs de PE y las
líneas del reporte (`perfilado/funciones_*_O2.txt`). Le siguen E5 (entre
5.0 % y 21.0 %), que arma y escribe el reporte de ciclos con una línea por
`SEND`, y E3 (entre 3.9 % y 12.4 %).

**Dentro de E4**, con `O0` la comunicación pesa más que el cómputo en matmul
(105.30 contra 84.66 µs) y en convolución (166.01 contra 153.95 µs), y en
reducción quedan parejos (16.63 contra 16.26 µs). Con `O2` y `O3` se invierte:
la pasada de comunicación mejora entre 3.19 y 3.56 veces y la de cómputo entre
2.21 y 2.74 veces, y el cómputo pasa a pesar más en los tres programas
(matmul O2: 30.19 de comunicación contra 34.30 µs de cómputo). E4a y E4b
incluyen el costo de leer el reloj tres veces por ciclo (2484 lecturas en los
828 ciclos de matmul), que a esta escala no es despreciable y todavía no se
midió aparte.

**Implicaciones para los prototipos en GPU y FPGA.** Lo que domina en CPU
(E2, E3, E5) es preparación y reporte, no la CGRA: se hace una sola vez por
programa y corresponde al host. Lo que se lleva al acelerador es E4, que en
estos programas tarda decenas o cientos de µs. En GPU, un ciclo de 16 o 64 PEs
con dos sincronizaciones por ciclo no llena el dispositivo: conviene simular
muchas memorias o muchos programas a la vez, o mallas mucho más grandes, y
cargar los programas ya codificados en binario en vez de texto. En FPGA, las
fases de `SEND`/`RECV` y de cómputo corresponden a hardware que trabaja en
paralelo en cada ciclo, así que el costo de E4 pasa a ser un ciclo de reloj
por ciclo de la CGRA.

**Siguientes pasos**, en orden: (1) repetir la medición con el reloj fijado con
`cpupower`; (2) acelerar E2 (lectura del archivo completo con un parser sin
`strtoul`/`strcmp` por token, o un formato binario de programas) y medirlo
contra esta línea base; (3) paralelizar E2 por archivo; (4) medir el costo del
reloj en E4 y reordenar `por_pe` por ciclo; (5) repetir el skill 08 después de
cada cambio.

## 6. Reproducibilidad

Desde `cgra_c/`:

```sh
# Variantes
make OPT=-O0 BUILD=build/O0
make OPT=-O2 BUILD=build/O2
make OPT=-O3 BUILD=build/O3

# Reloj fijo (requiere sudo), laptop enchufada, demás programas cerrados
sudo cpupower frequency-set -g performance

# Muestras: 5 de calentamiento + 120 por configuración
for v in O0 O2 O3; do
    OPT=-$v CC=gcc scripts/perfilar.sh $v build/$v/cgra ../src/reduccion 8 8 120
    OPT=-$v CC=gcc scripts/perfilar.sh $v build/$v/cgra ../src/matmul 4 4 120
    OPT=-$v CC=gcc scripts/perfilar.sh $v build/$v/cgra ../src/convolucion 4 4 120
done

# Estadísticas, resumen.csv y gráficas
uv run scripts/analizar_perfil.py

# Funciones calientes (instrucciones por función)
make OPT="-O2 -g" BUILD=build/O2g
valgrind --tool=callgrind --callgrind-out-file=/tmp/matmul.cg \
    build/O2g/cgra ../src/matmul --reporte-ciclos /tmp/r.txt --perfil 1
callgrind_annotate --inclusive=no /tmp/matmul.cg > perfilado/funciones_matmul_O2.txt
```

Después de cada cambio en el simulador, repetir el skill 08 (equivalencia con
Python) antes de medir, y luego este reporte (skill 09).
