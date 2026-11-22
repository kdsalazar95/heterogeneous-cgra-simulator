# Perfilado del simulador de la CGRA en GPU (G2)

Perfilado del simulador en C (la aplicación) en CPU y con la GPU como acelerador,
en la misma máquina, con la CGRA sin PEs C (commit `de28e55` de `feature/vector-pes`).

## 1. Plataforma

| | |
|---|---|
| CPU | Intel Core i5-12400F, 6 núcleos / 12 hilos |
| GPU | NVIDIA GeForce RTX 4060 Ti, 16 GB, driver 617.14 (sm_89) |
| RAM | 15 GiB |
| Sistema | WSL2 (Linux 6.6.87.2-microsoft-standard) sobre Windows |
| Compiladores | gcc 13.4.0 (conda-forge), nvcc 12.9 |
| Perfilador | Nsight Systems 2026.3 |

## 2. Metodología

| Etapa | Qué mide |
|---|---|
| E1 | cargar `memoria.bin` |
| E2 | leer y parsear los `PE*.txt` |
| E3 | validación y enlace |
| E4 | ejecución de la malla |
| E5 | armar y guardar el reporte de ciclos |

| Desglose | Variable | Columnas |
|---|---|---|
| E4 en GPU | `CGRA_DESGLOSE_GPU` | contexto, H2D, kernels, D2H |
| E5 | `CGRA_DESGLOSE_E5` | conteo, encabezado, distribución, actividad, ciclos, memoria final |

- 5 corridas de calentamiento + **120 muestras** por configuración, con `perfilar.sh`.
- 3 programas × 6 mallas = 18 configuraciones por versión.
- Estadística: media, desviación estándar e IC 95 % (media ± 1.96·σ/√n).
- CPU con `-O2 -ffp-contract=off`; GPU con `--fmad=false` para el mismo redondeo.

| Versión | Ejecutable | En GPU |
|---|---|---|
| CPU | `cgra` | nada |
| GPU2 | `cgra_gpu` | E4 (`malla_cuda.cu`) y `seccion_ciclos` de E5 (`reporte_cuda.cu`) |

## 3. Cuellos de botella en CPU

Parte del tiempo total de la CPU (media de 120 muestras):

| Configuración | E2 | E3 | E4 | E5 | `seccion_ciclos` |
|---|---|---|---|---|---|
| matmul 4x4 | 29 % | 4 % | 0.7 % | 65 % | 61.9 % |
| matmul 32x32 | 31 % | 6 % | 1.7 % | 61 % | 56.5 % |

- E5 casi no es disco: escribir el archivo cuesta 1–18 ms (1–7 % de E5).
- Dentro de E5, `seccion_ciclos` es el 89–94 %.
- **`seccion_ciclos` sola es el 53.2–61.9 % del total en las 18 configuraciones.**

## 4. Criterios de la descarga de trabajo

| Función | % del total (CPU) | Paralelismo | Decisión |
|---|---|---|---|
| `seccion_ciclos` (E5) | 53–62 % | cada ciclo es independiente | **a la GPU** |
| E2, leer `PE*.txt` | ~30 % | lectura de archivos | se queda en CPU |
| E4, ejecución de la malla | 0.2–1.7 % | un hilo por PE | en GPU desde la primera versión |

`seccion_ciclos` cumple por sí sola el criterio de abarcar al menos el 50 % del tiempo.

**Kernel de `seccion_ciclos`.** Un hilo por ciclo, en dos pasadas: la primera mide
cuántos bytes ocupa cada ciclo y la segunda escribe cada uno en su lugar. La CPU prepara
los textos (instrucciones, nombre y tipo de cada PE) y la GPU arma las tablas.

**Kernel de E4.** Un hilo por PE. Cada ciclo: `pasada_envios` (SEND en paralelo),
`pasada_resto` (cómputo y RECV en paralelo) y `pasada_memoria` (LD/ST en un hilo, en orden).

## 5. Verificación

| Comparación (CPU vs GPU2) | Resultado |
|---|---|
| salida, código de salida, `reporte_ciclos.txt` y memoria final | **18/18 idénticas** |

## 6. Resultados: CPU vs GPU2

Media en ms, 120 muestras; ± es el IC 95 %.

| programa | malla | `seccion_ciclos` CPU | `seccion_ciclos` GPU | aceleración | total CPU | total GPU2 |
|---|---|---|---|---|---|---|
| convolucion | 4x4 | 69.7 | 6.4 ± 0.1 | 10.9x | 119.0 | 294.7 ± 2.2 |
| convolucion | 6x8 | 74.5 | 7.1 ± 0.1 | 10.5x | 123.3 | 249.5 ± 1.4 |
| convolucion | 12x12 | 85.6 | 9.5 ± 0.2 | 9.0x | 150.3 | 249.8 ± 1.5 |
| convolucion | 12x16 | 112.1 | 11.9 ± 0.2 | 9.5x | 193.5 | 278.5 ± 1.4 |
| convolucion | 24x24 | 140.4 | 18.5 ± 0.2 | 7.6x | 261.9 | 321.8 ± 1.0 |
| convolucion | 32x32 | 204.4 | 28.5 ± 0.3 | 7.2x | 364.7 | 394.6 ± 3.7 |
| matmul | 4x4 | 196.1 | 13.0 ± 0.2 | 15.1x | 317.4 | 525.7 ± 3.9 |
| matmul | 6x8 | 135.0 | 11.0 ± 0.1 | 12.3x | 225.6 | 327.3 ± 1.9 |
| matmul | 12x12 | 135.3 | 12.8 ± 0.2 | 10.5x | 241.0 | 309.4 ± 1.5 |
| matmul | 12x16 | 161.9 | 14.5 ± 0.3 | 11.1x | 283.1 | 332.5 ± 1.5 |
| matmul | 24x24 | 149.3 | 19.5 ± 0.2 | 7.7x | 273.7 | 333.7 ± 1.5 |
| matmul | 32x32 | 228.7 | 29.6 ± 0.3 | 7.7x | 411.6 | 412.4 ± 2.8 |
| reduccion | 4x4 | 23.4 | 3.5 ± 0.1 | 6.7x | 41.5 | 203.9 ± 1.4 |
| reduccion | 6x8 | 37.1 | 4.6 ± 0.1 | 8.0x | 67.6 | 206.3 ± 1.0 |
| reduccion | 12x12 | 50.4 | 7.1 ± 0.2 | 7.1x | 92.7 | 224.4 ± 2.9 |
| reduccion | 12x16 | 67.5 | 8.8 ± 0.1 | 7.6x | 123.0 | 236.2 ± 1.5 |
| reduccion | 24x24 | 107.9 | 16.1 ± 0.2 | 6.7x | 202.4 | 297.1 ± 1.4 |
| reduccion | 32x32 | 177.7 | 27.4 ± 0.2 | 6.5x | 328.2 | 377.2 ± 2.1 |

Desglose por etapa, matmul (ms):

| Etapa | 4x4 CPU | 4x4 GPU2 | 32x32 CPU | 32x32 GPU2 |
|---|---|---|---|---|
| E2 programas | 93.2 | 90.2 | 129.3 | 125.4 |
| E3 validación | 14.0 | 13.4 | 25.4 | 25.4 |
| E4 ejecución | 2.3 | 399.1 | 7.0 | 217.0 |
| — contexto CUDA | | 149.1 | | 153.0 |
| — H2D | | 1.4 | | 2.2 |
| — kernels | | 247.9 | | 60.0 |
| — D2H | | 0.2 | | 0.3 |
| E5 reporte | 207.8 | 22.8 | 249.7 | 44.3 |
| Total | 317.4 | 525.7 | 411.6 | 412.4 |

- `seccion_ciclos` en GPU es **6.5x a 15.1x más rápida**, con el mismo resultado.
- E5 baja de 208–250 ms a 23–44 ms en matmul.
- El total de GPU2 sigue siendo mayor que el de CPU en 17 de 18 configuraciones
  (en matmul 32x32 quedan iguales): lo que se gana en E5 se pierde en E4.
- E4 en GPU cuesta ~150 ms de contexto CUDA por corrida más los kernels.

## 7. Análisis de E4 con Nsight Systems

Medido con la primera versión de GPU (solo E4) y la CGRA anterior (con PEs C).

| Kernel | 4x4 (13 024 ciclos) | 32x32 (332 ciclos) |
|---|---|---|
| `pasada_memoria` | 50.7 ms (55 %), 3.9 µs c/u | **58.9 ms (97.5 %)**, 177 µs c/u |
| `pasada_resto` | 23.0 ms (25 %), 1.8 µs c/u | 0.9 ms (1.5 %), 2.8 µs c/u |
| `pasada_envios` | 17.9 ms (20 %), 1.4 µs c/u | 0.6 ms (1.0 %), 1.8 µs c/u |

| API CUDA | 4x4 | 32x32 |
|---|---|---|
| `cudaLaunchKernel` | **310 ms** (39 072 llamadas, ~8 µs c/u) | 7.4 ms (996 llamadas) |
| `cudaFree` (contexto) | 173 ms | 140 ms |
| `cudaMemcpy` | 1.5 ms | 2.2 ms |

| Malla | Domina en E4 | Por qué |
|---|---|---|
| chica | lanzar kernels | 3 kernels por ciclo; lanzar (8 µs) cuesta más que ejecutar |
| grande | `pasada_memoria` | un solo hilo recorre todos los PEs en cada ciclo |
| todas | contexto CUDA | ~150 ms fijos por corrida |

## 8. Siguientes pasos

| # | Paso | Dato que lo justifica |
|---|---|---|
| 1 | Dejar E4 en CPU o crear el contexto una sola vez | E4 GPU 173–399 ms vs 0.3–7 ms en CPU; ~150 ms son contexto |
| 2 | Paralelizar `pasada_memoria` | 97.5 % del tiempo de GPU de E4 en 32x32 |
| 3 | Menos lanzamientos en E4 (un kernel o CUDA Graphs) | 310 ms de lanzamientos en 4x4 |
| 4 | Más hilos en `seccion_ciclos` (por fila de tabla) | en 32x32 hay solo ~330 ciclos, pocos hilos |
| 5 | Acelerar E2 (lectura de `PE*.txt`) | ~30 % del total, siguiente cuello de botella |

## 9. Limitaciones

- WSL2 no deja fijar la frecuencia de la CPU (`cpupower`); la controla Windows.
- Los totales de CPU variaron entre corridas (p. ej. matmul 32x32: 375.7 vs 429.3 ms
  con la CGRA anterior); la comparación de totales cercanos debe leerse con cuidado.
- Nsight Compute (`ncu`) no se usó: WSL2 bloquea los contadores de la GPU sin permiso
  de administrador en Windows (`ERR_NVGPUCTRPERM`).
- Los datos de `nsys` (sección 7) son de la CGRA anterior y de la primera versión de GPU.
- `--traza` usa la versión de CPU de `seccion_ciclos`; E4 en GPU no la soporta.
- La tabla de la sección 6 se calculó de los CSV directamente; `resumen_cpu_gpu.py`
  todavía resume solo las variantes `O2` y `GPU`.

## 10. Reproducir

Desde `cgra/c`:

    bash scripts/perfilar_cpu.sh 120          # compila cgra y mide en CPU
    bash scripts/perfilar_e5.sh 120           # desglose de E5 en CPU
    bash scripts/compilar_gpu.sh              # compila cgra_gpu
    bash scripts/verificar_gpu.sh             # CPU vs GPU (18/18)
    bash scripts/perfilar_gpu.sh 120 GPU2     # mide en GPU con desgloses de E4 y E5

Si cambian los zips de `Programas_PEs/`, borrar antes `perfilado/entradas/`.

| Archivo | Qué es |
|---|---|
| `src/malla_cuda.cu` | E4 en GPU |
| `src/reporte_cuda.cu` | `seccion_ciclos` en GPU |
| `src/reporte.c` | marcas de tiempo de E5 y llamada a la GPU (`CGRA_GPU`) |
| `scripts/compilar_gpu.sh` | compila `cgra_gpu` |
| `scripts/perfilar_cpu.sh` / `perfilar_e5.sh` / `perfilar_gpu.sh` | 120 muestras por configuración |
| `scripts/verificar_gpu.sh` | equivalencia CPU vs GPU |
| `scripts/resumen_cpu_gpu.py` | media, σ, IC 95 % (variantes `O2` y `GPU`) |
