# Perfilado del simulador CGRA en la Jetson Nano

Perfilado del simulador en C en el sistema empotrado, en CPU y con la GPU como
acelerador. Complementa `cgra/c/REPORTE_PERFILADO_GPU.md`, que se midió en la PC
de escritorio (i5-12400F + RTX 4060 Ti, WSL2).

## 1. Plataforma

| | |
|---|---|
| Placa | NVIDIA Jetson Nano 2GB Developer Kit (`jetson-181`) |
| Sistema | Ubuntu 18.04.6 LTS, L4T R32.7.1 (JetPack 4.6.1), kernel 4.9.253-tegra, aarch64 |
| CPU | 4 × ARM Cortex-A57 |
| GPU | Maxwell, 128 núcleos CUDA, `sm_53`; memoria compartida con la CPU (2 GB) |
| Compiladores | gcc 7.5 (`-O2 -ffp-contract=off`), nvcc 10.2.300 (`--fmad=false -arch=sm_53`) |
| Commit | `96b993d` (`feature/ronald-perfilado`) |

## 2. Estado

Terminado: **28/10/2026 07:15**

| Corrida | Script | Resultado | Duración |
|---|---|---|---|
| CPU | `perfilar_cpu.sh 120` | ✅ 18/18 | 102.1 min |
| Desglose de E5 | `perfilar_e5.sh 120` | ✅ 18/18 | 101.6 min |
| Compilación GPU | `compilar_gpu.sh` | ✅ sin errores con nvcc 10.2 / `sm_53` | |
| Verificación CPU vs GPU | `verificar_gpu.sh` | ✅ 18 iguales, 0 distintos | |
| GPU | `perfilar_gpu.sh 120 GPU` | ✅ 18/18 | 60.9 min |

## 3. Metodología

- 5 corridas de calentamiento + **120 muestras** por configuración (`perfilar.sh`).
- 3 programas × 6 mallas = 18 configuraciones por versión (CPU, E5, GPU).
- Media, desviación estándar e IC 95 % (media ± 1.96·σ/√n).
- Sin `sudo` en la cuenta compartida: no se fijaron los relojes (`nvpmodel` /
  `jetson_clocks`); se usó el modo de energía por defecto. La carga del sistema
  se mantuvo en ~1.0 durante la medición (solo el perfilado).

Reproducir, desde `cgra/c` en la Jetson:

    CC=gcc bash scripts/perfilar_cpu.sh 120
    bash scripts/perfilar_e5.sh 120
    bash scripts/compilar_gpu.sh
    bash scripts/verificar_gpu.sh
    bash scripts/perfilar_gpu.sh 120 GPU

## 4. Resultados en CPU (O2)

Media ± IC 95 %, en ms. E1 (carga de `memoria.bin`) es ≤ 0.5 ms en todas las
configuraciones. "vs PC" es el total de la Jetson dividido entre el total de la PC.

| programa | malla | E1 | E2 programas | E3 validación | E4 ejecución | E5 reporte | total | % E2 | % E4 | % E5 | vs PC |
|---|---|---|---|---|---|---|---|---|---|---|---|
| convolucion | 4x4 | 0.30 | 455.2 ± 1.4 | 47.7 ± 0.15 | 10.5 ± 0.04 | 1093.3 ± 0.9 | 1607.7 ± 2.0 | 28.3 | 0.7 | 68.0 | 13.5x |
| convolucion | 6x8 | 0.30 | 436.1 ± 1.8 | 38.0 ± 0.17 | 11.7 ± 0.03 | 1134.1 ± 2.4 | 1620.9 ± 3.1 | 26.9 | 0.7 | 70.0 | 13.1x |
| convolucion | 12x12 | 0.30 | 564.7 ± 2.4 | 51.7 ± 0.16 | 15.3 ± 0.03 | 1249.7 ± 1.4 | 1882.4 ± 3.1 | 30.0 | 0.8 | 66.4 | 12.5x |
| convolucion | 12x16 | 0.30 | 711.2 ± 3.1 | 63.1 ± 0.23 | 19.8 ± 0.03 | 1625.5 ± 2.0 | 2420.8 ± 4.2 | 29.4 | 0.8 | 67.1 | 12.5x |
| convolucion | 24x24 | 0.30 | 1015.0 ± 3.9 | 98.8 ± 0.18 | 33.6 ± 0.07 | 2009.7 ± 3.1 | 3158.6 ± 5.5 | 32.1 | 1.1 | 63.6 | 12.1x |
| convolucion | 32x32 | 0.31 | 1351.2 ± 5.8 | 130.1 ± 2.05 | 48.2 ± 1.13 | 2807.4 ± 3.6 | 4338.8 ± 9.2 | 31.1 | 1.1 | 64.7 | 11.9x |
| matmul | 4x4 | 0.31 | 1125.1 ± 4.0 | 113.3 ± 0.20 | 29.9 ± 0.11 | 2976.9 ± 3.7 | 4246.1 ± 6.5 | 26.5 | 0.7 | 70.1 | 13.4x |
| matmul | 6x8 | 0.33 | 808.6 ± 3.1 | 66.6 ± 0.16 | 20.8 ± 0.04 | 2053.2 ± 1.7 | 2950.2 ± 4.0 | 27.4 | 0.7 | 69.6 | 13.1x |
| matmul | 12x12 | 0.32 | 920.3 ± 3.7 | 83.4 ± 0.20 | 23.9 ± 0.04 | 1974.7 ± 2.1 | 3003.3 ± 4.8 | 30.6 | 0.8 | 65.7 | 12.5x |
| matmul | 12x16 | 0.31 | 1054.8 ± 4.1 | 99.3 ± 0.19 | 31.8 ± 0.05 | 2334.2 ± 2.8 | 3521.2 ± 5.2 | 30.0 | 0.9 | 66.3 | 12.4x |
| matmul | 24x24 | 0.32 | 1099.2 ± 4.3 | 106.9 ± 0.22 | 32.8 ± 0.07 | 2114.6 ± 2.3 | 3355.1 ± 5.3 | 32.8 | 1.0 | 63.0 | 12.3x |
| matmul | 32x32 | 0.29 | 1513.0 ± 5.8 | 170.2 ± 1.77 | 68.8 ± 0.39 | 3111.4 ± 5.5 | 4865.3 ± 9.0 | 31.1 | 1.4 | 64.0 | 11.8x |
| reduccion | 4x4 | 0.45 | 162.3 ± 0.4 | 18.5 ± 0.02 | 3.1 ± 0.02 | 390.1 ± 0.4 | 575.1 ± 0.7 | 28.2 | 0.5 | 67.8 | 13.9x |
| reduccion | 6x8 | 0.48 | 262.8 ± 0.9 | 24.2 ± 0.14 | 6.1 ± 0.03 | 592.8 ± 0.6 | 887.2 ± 1.2 | 29.6 | 0.7 | 66.8 | 13.1x |
| reduccion | 12x12 | 0.46 | 383.8 ± 1.4 | 32.2 ± 0.08 | 8.1 ± 0.01 | 767.8 ± 1.1 | 1193.1 ± 2.0 | 32.2 | 0.7 | 64.3 | 12.9x |
| reduccion | 12x16 | 0.53 | 493.4 ± 1.8 | 38.0 ± 0.12 | 10.1 ± 0.02 | 1010.4 ± 1.3 | 1553.3 ± 2.5 | 31.8 | 0.6 | 65.0 | 12.6x |
| reduccion | 24x24 | 0.49 | 839.2 ± 3.3 | 58.7 ± 0.17 | 18.5 ± 0.04 | 1601.5 ± 2.1 | 2519.7 ± 4.2 | 33.3 | 0.7 | 63.6 | 12.4x |
| reduccion | 32x32 | 0.48 | 1277.6 ± 5.5 | 102.9 ± 0.48 | 36.5 ± 0.30 | 2547.2 ± 3.7 | 3966.3 ± 7.5 | 32.2 | 0.9 | 64.2 | 12.1x |

## 5. Desglose de E5 (`seccion_ciclos`)

Media de 120 muestras, en ms (corrida `perfilar_e5.sh`, variante E5).

| programa | malla | `seccion_ciclos` | E5 | total | % de E5 | % del total |
|---|---|---|---|---|---|---|
| convolucion | 4x4 | 1035.1 | 1096.1 | 1611.8 | 94.4 | 64.2 |
| convolucion | 6x8 | 1072.0 | 1136.2 | 1624.2 | 94.3 | 66.0 |
| convolucion | 12x12 | 1174.6 | 1251.2 | 1883.5 | 93.9 | 62.4 |
| convolucion | 12x16 | 1532.0 | 1627.6 | 2424.6 | 94.1 | 63.2 |
| convolucion | 24x24 | 1861.4 | 1995.4 | 3132.4 | 93.3 | 59.4 |
| convolucion | 32x32 | 2625.8 | 2802.9 | 4319.8 | 93.7 | 60.8 |
| matmul | 4x4 | 2837.4 | 2985.9 | 4260.0 | 95.0 | 66.6 |
| matmul | 6x8 | 1952.8 | 2060.6 | 2958.9 | 94.8 | 66.0 |
| matmul | 12x12 | 1858.7 | 1973.1 | 3000.3 | 94.2 | 61.9 |
| matmul | 12x16 | 2198.5 | 2329.4 | 3502.8 | 94.4 | 62.8 |
| matmul | 24x24 | 1971.2 | 2113.3 | 3344.9 | 93.3 | 58.9 |
| matmul | 32x32 | 2887.6 | 3079.7 | 4783.4 | 93.8 | 60.4 |
| reduccion | 4x4 | 344.3 | 391.4 | 577.3 | 88.0 | 59.6 |
| reduccion | 6x8 | 537.7 | 594.7 | 889.6 | 90.4 | 60.4 |
| reduccion | 12x12 | 693.8 | 765.6 | 1188.8 | 90.6 | 58.4 |
| reduccion | 12x16 | 925.9 | 1010.4 | 1549.5 | 91.6 | 59.8 |
| reduccion | 24x24 | 1472.3 | 1605.4 | 2523.4 | 91.7 | 58.3 |
| reduccion | 32x32 | 2336.6 | 2527.1 | 3928.6 | 92.5 | 59.5 |

## 6. Verificación CPU vs GPU

`verificar_gpu.sh`: salida, código de salida, `reporte_ciclos.txt` y memoria
final idénticos entre `cgra` y `cgra_gpu` en las 18 configuraciones
(**18 iguales, 0 distintos**). El código CUDA compiló sin cambios con nvcc 10.2.

## 7. Resultados con GPU

Media ± IC 95 %, en ms. E4 y `seccion_ciclos` (dentro de E5) corren en la GPU.
E1 ≤ 0.5 ms en todas.

| programa | malla | E2 | E3 | E4 | E5 | total GPU | total CPU | aceleración total |
|---|---|---|---|---|---|---|---|---|
| convolucion | 4x4 | 461.3 ± 2.4 | 48.0 ± 0.2 | 819.8 ± 34.4 | 93.5 ± 1.2 | 1423.8 ± 34.6 | 1607.7 | 1.13x |
| convolucion | 6x8 | 444.2 ± 2.9 | 38.2 ± 0.2 | 471.4 ± 15.9 | 99.1 ± 0.2 | 1054.0 ± 16.0 | 1620.9 | 1.54x |
| convolucion | 12x12 | 571.8 ± 3.4 | 53.1 ± 0.3 | 275.3 ± 7.2 | 128.6 ± 0.4 | 1030.0 ± 7.8 | 1882.4 | 1.83x |
| convolucion | 12x16 | 726.6 ± 4.7 | 65.6 ± 0.6 | 262.6 ± 5.5 | 158.9 ± 0.5 | 1214.9 ± 6.8 | 2420.8 | 1.99x |
| convolucion | 24x24 | 1032.5 ± 6.6 | 95.5 ± 0.6 | 256.1 ± 0.7 | 239.9 ± 0.6 | 1626.0 ± 6.7 | 3158.6 | 1.94x |
| convolucion | 32x32 | 1385.3 ± 9.4 | 141.9 ± 3.1 | 328.8 ± 0.9 | 336.3 ± 0.5 | 2194.2 ± 10.1 | 4338.8 | 1.98x |
| matmul | 4x4 | 1152.7 ± 7.2 | 114.1 ± 0.2 | 2000.3 ± 96.0 | 219.0 ± 0.4 | 3487.2 ± 96.4 | 4246.1 | 1.22x |
| matmul | 6x8 | 818.4 ± 5.5 | 67.2 ± 0.2 | 785.3 ± 25.3 | 171.5 ± 0.4 | 1843.6 ± 25.4 | 2950.2 | 1.60x |
| matmul | 12x12 | 940.6 ± 6.0 | 94.2 ± 0.4 | 358.9 ± 11.0 | 191.2 ± 0.6 | 1586.1 ± 12.6 | 3003.3 | 1.89x |
| matmul | 12x16 | 1089.1 ± 8.6 | 122.0 ± 0.8 | 333.7 ± 9.4 | 221.7 ± 1.3 | 1767.8 ± 13.6 | 3521.2 | 1.99x |
| matmul | 24x24 | 1120.9 ± 7.3 | 110.5 ± 0.5 | 265.9 ± 0.7 | 255.4 ± 0.5 | 1754.5 ± 7.6 | 3355.1 | 1.91x |
| matmul | 32x32 | 1537.8 ± 9.8 | 159.2 ± 1.9 | 357.0 ± 0.6 | 359.6 ± 0.6 | 2415.7 ± 10.3 | 4865.3 | 2.01x |
| reduccion | 4x4 | 164.1 ± 0.8 | 18.5 ± 0.0 | 370.6 ± 12.4 | 60.1 ± 0.3 | 614.5 ± 12.5 | 575.1 | **0.94x** |
| reduccion | 6x8 | 265.7 ± 1.4 | 24.2 ± 0.1 | 293.2 ± 9.2 | 79.5 ± 0.3 | 663.9 ± 9.3 | 887.2 | 1.34x |
| reduccion | 12x12 | 388.3 ± 2.4 | 33.3 ± 0.2 | 204.6 ± 5.1 | 111.6 ± 0.3 | 739.3 ± 5.4 | 1193.1 | 1.61x |
| reduccion | 12x16 | 498.2 ± 3.1 | 40.1 ± 0.5 | 205.0 ± 4.1 | 134.4 ± 0.4 | 879.2 ± 5.1 | 1553.3 | 1.77x |
| reduccion | 24x24 | 852.8 ± 5.7 | 65.4 ± 0.7 | 232.4 ± 0.8 | 231.2 ± 0.5 | 1383.7 ± 6.1 | 2519.7 | 1.82x |
| reduccion | 32x32 | 1294.8 ± 8.3 | 90.2 ± 0.3 | 312.0 ± 0.6 | 354.2 ± 0.6 | 2053.6 ± 8.6 | 3966.3 | 1.93x |

`seccion_ciclos` y desglose de E4 en la GPU (media, ms):

| programa | malla | `seccion_ciclos` CPU | `seccion_ciclos` GPU | aceleración | contexto CUDA | H2D | kernels | D2H |
|---|---|---|---|---|---|---|---|---|
| convolucion | 4x4 | 1035.1 | 48.1 | 21.5x | 114.8 | 2.65 | 701.3 | 0.59 |
| convolucion | 6x8 | 1072.0 | 50.8 | 21.1x | 118.0 | 3.32 | 348.9 | 0.59 |
| convolucion | 12x12 | 1174.6 | 68.1 | 17.2x | 116.8 | 4.19 | 153.2 | 0.59 |
| convolucion | 12x16 | 1532.0 | 85.0 | 18.0x | 120.1 | 5.27 | 136.1 | 0.58 |
| convolucion | 24x24 | 1861.4 | 130.3 | 14.3x | 126.8 | 6.74 | 121.3 | 0.58 |
| convolucion | 32x32 | 2625.8 | 189.5 | 13.9x | 141.0 | 9.52 | 175.9 | 0.66 |
| matmul | 4x4 | 2837.4 | 116.0 | 24.5x | 124.5 | 5.72 | 1868.9 | 0.58 |
| matmul | 6x8 | 1952.8 | 90.8 | 21.5x | 127.5 | 5.36 | 651.3 | 0.60 |
| matmul | 12x12 | 1858.7 | 101.0 | 18.4x | 122.4 | 6.07 | 229.3 | 0.57 |
| matmul | 12x16 | 2198.5 | 118.6 | 18.5x | 128.4 | 7.12 | 197.0 | 0.59 |
| matmul | 24x24 | 1971.2 | 139.8 | 14.1x | 128.8 | 7.15 | 128.6 | 0.61 |
| matmul | 32x32 | 2887.6 | 199.5 | 14.5x | 144.6 | 10.39 | 199.7 | 0.62 |
| reduccion | 4x4 | 344.3 | 18.9 | 18.2x | 109.4 | 0.97 | 259.2 | 0.50 |
| reduccion | 6x8 | 537.7 | 29.9 | 18.0x | 111.3 | 2.22 | 178.7 | 0.49 |
| reduccion | 12x12 | 693.8 | 47.8 | 14.5x | 112.3 | 2.98 | 88.3 | 0.48 |
| reduccion | 12x16 | 925.9 | 60.1 | 15.4x | 114.3 | 3.64 | 86.0 | 0.48 |
| reduccion | 24x24 | 1472.3 | 115.8 | 12.7x | 123.8 | 6.09 | 101.2 | 0.52 |
| reduccion | 32x32 | 2336.6 | 186.9 | 12.5x | 139.5 | 8.93 | 161.3 | 0.57 |

## 8. Observaciones

- **E5 (reporte) es el cuello de botella en CPU: 63.0–70.1 % del total** en las
  18 configuraciones, más que en la PC (~61–65 %).
- **`seccion_ciclos` sola es el 88.0–95.0 % de E5 y el 58.3–66.6 % del total**:
  por sí sola cumple el criterio de descargar al menos el 50 % del tiempo en el
  sistema empotrado, igual que en la PC (53.2–61.9 %).
- **E2 (lectura de `PE*.txt`) es el segundo: 26.5–33.3 %** en CPU. Crece con la
  malla, porque hay más archivos. Con E5 acelerado pasa a ser la etapa más
  pesada en las mallas medianas y grandes (≈ 55–65 % del total con GPU).
- **E4 (ejecución de la malla) es solo 0.5–1.4 %** en CPU.
- **`seccion_ciclos` es 12.5–24.5x más rápida en la GPU**, con el mismo resultado
  byte a byte. La aceleración es mayor en mallas pequeñas (más ciclos, más hilos).
- **En la Jetson la versión con GPU reduce el total en 17 de 18 configuraciones
  (1.13–2.01x)**, al contrario que en la PC, donde el total con GPU era mayor en
  17 de 18. La CPU ARM es tan lenta en `seccion_ciclos` que lo ganado en E5
  compensa el costo de E4. La excepción es `reduccion 4x4` (0.94x).
- **E4 en GPU es 5–120x más lento que en CPU.** Tiene un costo fijo de
  ~109–145 ms de contexto CUDA, y los kernels crecen con el número de ciclos:
  en 4x4 (miles de ciclos, 3 lanzamientos por ciclo) los kernels llegan a
  259–1869 ms. Por eso la aceleración total es menor en mallas pequeñas.
- La CPU de la Jetson es **11.8–13.9x más lenta** que la de la PC en el total.
- El contexto CUDA en la Nano (~110–145 ms) es menor que en la PC bajo WSL2 (~150 ms).
- Variabilidad baja: el IC 95 % es < 0.5 % de la media en todos los totales de
  CPU; en GPU sube en mallas pequeñas por E4 (hasta ~2.8 % en `matmul 4x4`).

## 9. Siguientes pasos

| # | Paso | Dato que lo justifica |
|---|---|---|
| 1 | Dejar E4 en CPU (o crear el contexto CUDA una sola vez) | E4 en GPU 204–2000 ms vs 3–69 ms en CPU; ~110–145 ms son contexto |
| 2 | Un solo kernel por ciclo o CUDA Graphs en E4 | Los kernels de E4 crecen con los ciclos: 1869 ms en `matmul 4x4` |
| 3 | Paralelizar `pasada_memoria` (solo los PEs M) | Un solo hilo recorre todos los PEs en cada ciclo |
| 4 | Acelerar E2 (lectura de `PE*.txt`) | Con E5 en GPU es la etapa más pesada (26.5–33.3 % en CPU) |
