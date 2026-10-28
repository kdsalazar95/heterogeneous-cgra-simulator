# Perfilado del simulador de la CGRA en GPU (G2)

Perfilado del simulador en C con la etapa de ejecución de la malla (E4) en CPU y en GPU,
en la misma máquina. Los números salen de `perfilado/resumen_cpu_gpu.csv` (ver sección 8).

## 1. Plataforma

| | |
|---|---|
| CPU | Intel Core i5-12400F, 6 núcleos / 12 hilos |
| GPU | NVIDIA GeForce RTX 4060 Ti, 16 GB, driver 617.14 (sm_89) |
| RAM | 15 GiB |
| Sistema | WSL2 (Linux 6.6.87.2-microsoft-standard) sobre Windows |
| Compiladores | gcc 13.4.0 (conda-forge), nvcc 12.9 |
| Perfilador | Nsight Systems 2026.3 |
| Commit base | `eda2b43` (`feature/vector-pes`) |

## 2. Metodología

| Etapa | Qué mide | CPU | GPU |
|---|---|---|---|
| E1 | cargar `memoria.bin` | igual | igual |
| E2 | leer y parsear los `PE*.txt` | igual | igual |
| E3 | validación y enlace | igual | igual |
| E4 | ejecución de la malla | `malla.c` | `malla_cuda.cu` |
| E5 | reporte de ciclos | igual | igual |

| Desglose de E4 en GPU | Qué mide |
|---|---|
| contexto | crear el contexto CUDA y reservar memoria |
| H2D | copiar instrucciones y memoria a la GPU |
| kernels | todos los ciclos (3 kernels por ciclo) |
| D2H | traer la memoria de vuelta |

- Dos ejecutables con el mismo código salvo E4: `cgra` (CPU) y `cgra_gpu` (GPU).
- 5 corridas de calentamiento + **120 muestras** por configuración, con `perfilar.sh`.
- 3 programas × 6 mallas = 18 configuraciones por versión, 4320 corridas en total.
- Estadística por etapa: media, desviación estándar e IC 95 % (media ± 1.96·σ/√n).
- Ambos con `-O2`; la GPU con `--fmad=false`, igual que el `-ffp-contract=off` de la CPU.

**Kernel GPU.** Un hilo por PE. Cada ciclo: `pasada_envios` (SEND en paralelo),
`pasada_resto` (cómputo y RECV en paralelo) y `pasada_memoria` (LD/ST en un hilo,
en orden de PE, para conservar la semántica del simulador original).

## 3. Verificación

| Comparación | Resultado |
|---|---|
| salida en pantalla, código de salida, `reporte_ciclos.txt` y memoria final | **18/18 idénticas** |

Los dos ejecutables simulan la misma CGRA, así que la comparación de tiempos es válida.

## 4. Resultados: CPU vs GPU

Media ± IC 95 %, en ms, 120 muestras por celda.

| programa | malla | E4 CPU | E4 GPU | kernels GPU | total CPU | total GPU | E4 CPU/GPU |
|---|---|---|---|---|---|---|---|
| convolucion | 4x4 | 0.93 ± 0.03 | 297.9 ± 5.1 | 132.2 | 136.7 | 428.6 | 0.003x |
| convolucion | 6x8 | 0.79 ± 0.02 | 197.0 ± 1.7 | 36.9 | 102.2 | 299.9 | 0.004x |
| convolucion | 12x12 | 1.46 ± 0.03 | 186.1 ± 1.1 | 25.2 | 125.5 | 315.9 | 0.008x |
| convolucion | 12x16 | 1.89 ± 0.04 | 193.9 ± 1.5 | 30.2 | 164.5 | 362.1 | 0.010x |
| convolucion | 24x24 | 3.76 ± 0.10 | 202.2 ± 1.4 | 36.7 | 242.6 | 429.6 | 0.019x |
| convolucion | 32x32 | 5.90 ± 0.08 | 227.7 ± 1.9 | 57.6 | 383.0 | 594.0 | 0.026x |
| matmul | 4x4 | 2.19 ± 0.06 | 483.8 ± 8.0 | 316.9 | 328.8 | 791.9 | 0.005x |
| matmul | 6x8 | 1.45 ± 0.03 | 235.9 ± 3.5 | 70.8 | 191.7 | 431.1 | 0.006x |
| matmul | 12x12 | 2.26 ± 0.03 | 203.2 ± 1.7 | 39.0 | 201.3 | 412.7 | 0.011x |
| matmul | 12x16 | 2.77 ± 0.06 | 205.9 ± 1.0 | 41.3 | 232.6 | 443.9 | 0.013x |
| matmul | 24x24 | 3.64 ± 0.07 | 205.8 ± 1.6 | 39.6 | 254.4 | 462.7 | 0.018x |
| matmul | 32x32 | 6.60 ± 0.11 | 234.2 ± 1.3 | 63.9 | 429.3 | 643.1 | 0.028x |
| reduccion | 4x4 | 0.62 ± 0.02 | 238.8 ± 3.2 | 78.2 | 89.4 | 323.1 | 0.003x |
| reduccion | 6x8 | 0.45 ± 0.02 | 183.3 ± 1.3 | 21.9 | 66.1 | 244.6 | 0.002x |
| reduccion | 12x12 | 0.63 ± 0.02 | 177.4 ± 1.2 | 17.8 | 83.8 | 264.0 | 0.004x |
| reduccion | 12x16 | 0.91 ± 0.03 | 184.4 ± 1.4 | 22.0 | 111.8 | 300.1 | 0.005x |
| reduccion | 24x24 | 1.99 ± 0.05 | 198.4 ± 1.1 | 34.3 | 192.4 | 396.1 | 0.010x |
| reduccion | 32x32 | 6.06 ± 0.13 | 242.6 ± 1.0 | 72.5 | 475.1 | 676.3 | 0.025x |

Desglose por etapa, matmul (ms):

| Etapa | 4x4 CPU | 4x4 GPU | 32x32 CPU | 32x32 GPU |
|---|---|---|---|---|
| E2 programas | 92.9 | 88.4 | 137.9 | 131.9 |
| E3 validación | 13.0 | 13.0 | 25.1 | 25.6 |
| E4 ejecución | 2.2 | 483.8 | 6.6 | 234.2 |
| — contexto | | 164.6 | | 165.9 |
| — H2D | | 1.6 | | 2.4 |
| — kernels | | 316.9 | | 63.9 |
| — D2H | | 0.2 | | 0.3 |
| E5 reporte | 220.5 | 206.3 | 259.4 | 251.1 |
| Total | 328.8 | 791.9 | 429.3 | 643.1 |

- La GPU es entre 35x y 500x más lenta en E4. Los IC son angostos: la diferencia es clara.
- La relación mejora con el tamaño: 0.003x en 4x4 → 0.026–0.028x en 32x32.
- En el total de la aplicación, E4 pesa poco en CPU (1–2 %); dominan E5 y E2.

## 5. Análisis con Nsight Systems

Una corrida de matmul con `nsys profile -t cuda`.

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

Cuellos de botella:

| Malla | Domina | Por qué |
|---|---|---|
| chica (4x4) | lanzar kernels | 3 kernels por ciclo × 13 024 ciclos; lanzar (8 µs) cuesta más que ejecutar (1–4 µs) |
| grande (32x32) | `pasada_memoria` | un solo hilo recorre los 1024 PEs en cada ciclo |
| todas | contexto CUDA | ~140–170 ms fijos por corrida |

Las transferencias H2D/D2H no son un problema (< 3 ms).

## 6. Optimizaciones propuestas

| # | Optimización | Ataca | Dato que la justifica |
|---|---|---|---|
| 1 | Paralelizar `pasada_memoria` (LD/ST en paralelo cuando no comparten dirección) | malla grande | 97.5 % del tiempo de GPU en 32x32 |
| 2 | Menos lanzamientos: un kernel para todos los ciclos o CUDA Graphs | malla chica | 310 ms de lanzamientos en 4x4 |
| 3 | Crear el contexto una vez si se ejecutan varios programas | todas | ~160 ms fijos |

## 7. Limitaciones

- WSL2 no deja fijar la frecuencia de la CPU (`cpupower`); la controla Windows.
- Nsight Compute (`ncu`) no se usó: WSL2 bloquea los contadores de la GPU sin
  permiso de administrador en Windows (`ERR_NVGPUCTRPERM`). Tampoco se usó en el lab de GPU.
- Bajo `nsys` los tiempos suben por el rastreo; sus números se usan solo para proporciones.
- `--traza` no está soportado en `cgra_gpu`.

## 8. Reproducir

Desde la raíz del repositorio:

    cgra/c/scripts/perfilar_cpu.sh 120          # compila cgra y mide en CPU
    bash cgra/c/scripts/verificar_gpu.sh        # compara CPU vs GPU (18/18)
    bash cgra/c/scripts/perfilar_gpu.sh 120     # compila cgra_gpu y mide en GPU
    python3 cgra/c/scripts/resumen_cpu_gpu.py   # estadísticas y tabla

| Archivo | Qué es |
|---|---|
| `src/malla_cuda.cu` | E4 en GPU |
| `scripts/compilar_gpu.sh` | compila `cgra_gpu` |
| `scripts/perfilar_cpu.sh` / `perfilar_gpu.sh` | 120 muestras por configuración |
| `scripts/verificar_gpu.sh` | equivalencia CPU vs GPU |
| `scripts/resumen_cpu_gpu.py` | media, σ, IC 95 % |
