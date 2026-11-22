# E2 paralelo (OpenMP + parser propio) en la Jetson Nano

Media ± IC 95 % (ms), 120 muestras por configuración. Jetson en modo 5W con 2 núcleos activos (OpenMP usa 2 hilos).
CPU y GPU original: perfilado de octubre; GPU_O2: optimización de Keylor; *_E2: con el E2 de Katherine.

## E2 (lectura de los PE*.txt)

| programa | malla | CPU | CPU_E2 | aceleración E2 |
|---|---|---|---|---|
| reduccion | 4x4 | 162.3 | 28.5 ± 1.5 | 5.69x |
| reduccion | 6x8 | 262.8 | 44.4 ± 0.8 | 5.92x |
| reduccion | 12x12 | 383.8 | 67.2 ± 2.1 | 5.71x |
| reduccion | 12x16 | 493.4 | 84.0 ± 0.6 | 5.87x |
| reduccion | 24x24 | 839.2 | 152.0 ± 2.4 | 5.52x |
| reduccion | 32x32 | 1277.6 | 234.7 ± 1.4 | 5.44x |
| matmul | 4x4 | 1125.1 | 200.2 ± 2.1 | 5.62x |
| matmul | 6x8 | 808.6 | 137.7 ± 0.8 | 5.87x |
| matmul | 12x12 | 920.3 | 161.4 ± 0.8 | 5.70x |
| matmul | 12x16 | 1054.8 | 186.5 ± 1.5 | 5.65x |
| matmul | 24x24 | 1099.2 | 205.2 ± 2.9 | 5.36x |
| matmul | 32x32 | 1513.0 | 287.1 ± 1.5 | 5.27x |
| convolucion | 4x4 | 455.2 | 81.6 ± 1.2 | 5.58x |
| convolucion | 6x8 | 436.1 | 75.5 ± 0.6 | 5.77x |
| convolucion | 12x12 | 564.7 | 101.1 ± 0.6 | 5.59x |
| convolucion | 12x16 | 711.2 | 127.7 ± 0.8 | 5.57x |
| convolucion | 24x24 | 1015.0 | 189.2 ± 1.0 | 5.36x |
| convolucion | 32x32 | 1351.2 | 259.0 ± 2.0 | 5.22x |

## Tiempo total

| programa | malla | CPU | CPU_E2 | GPU original | GPU_O2 | **GPU_O2_E2** | GPU_O2_E2 vs CPU |
|---|---|---|---|---|---|---|---|
| reduccion | 4x4 | 575 | 454 | 614 | 288 | **204 ± 2** | **2.82x** |
| reduccion | 6x8 | 887 | 686 | 664 | 429 | **243 ± 1** | **3.66x** |
| reduccion | 12x12 | 1193 | 904 | 739 | 598 | **312 ± 3** | **3.83x** |
| reduccion | 12x16 | 1553 | 1178 | 879 | 871 | **379 ± 2** | **4.10x** |
| reduccion | 24x24 | 2520 | 1888 | 1384 | 1404 | **597 ± 2** | **4.22x** |
| reduccion | 32x32 | 3966 | 2928 | 2054 | 2147 | **896 ± 3** | **4.43x** |
| matmul | 4x4 | 4246 | 3455 | 3487 | 1795 | **674 ± 1** | **6.30x** |
| matmul | 6x8 | 2950 | 2358 | 1844 | 1345 | **503 ± 3** | **5.87x** |
| matmul | 12x12 | 3003 | 2320 | 1586 | 1518 | **575 ± 1** | **5.22x** |
| matmul | 12x16 | 3521 | 2727 | 1768 | 1755 | **689 ± 2** | **5.11x** |
| matmul | 24x24 | 3355 | 2533 | 1755 | 1800 | **743 ± 3** | **4.52x** |
| matmul | 32x32 | 4865 | 3647 | 2416 | 2507 | **1230 ± 7** | **3.95x** |
| convolucion | 4x4 | 1608 | 1277 | 1424 | 658 | **309 ± 2** | **5.20x** |
| convolucion | 6x8 | 1621 | 1300 | 1054 | 658 | **311 ± 1** | **5.21x** |
| convolucion | 12x12 | 1882 | 1465 | 1030 | 996 | **407 ± 2** | **4.62x** |
| convolucion | 12x16 | 2421 | 1898 | 1215 | 1186 | **496 ± 3** | **4.89x** |
| convolucion | 24x24 | 3159 | 2398 | 1626 | 1681 | **705 ± 3** | **4.48x** |
| convolucion | 32x32 | 4339 | 3311 | 2194 | 2295 | **1083 ± 16** | **4.00x** |
