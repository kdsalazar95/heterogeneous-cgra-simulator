# Perfilado en la KV260: CPU ARM contra E4 en la FPGA

Media ± IC 95 % en ms; n = muestras por configuración. Aceleración = tiempo ARM / tiempo variante.

## E4 (ejecución de la malla) y total

| programa | malla | variante | n | E4 | aceleración E4 | total | aceleración total |
|---|---|---|---|---|---|---|---|
| reduccion | 4x4 | ARM | 120 | 5.7 ± 0.0 | 1.00x | 634.9 ± 2.3 | 1.00x |
| reduccion | 4x4 | FPGA_v1 | 120 | 68.6 ± 0.1 | 0.08x | 691.5 ± 0.5 | 0.92x |
| reduccion | 4x4 | FPGA_v2 | 120 | 14.2 ± 0.2 | 0.40x | 637.6 ± 1.2 | 1.00x |
| reduccion | 4x4 | FPGA_v3 | 120 | 12.1 ± 0.2 | 0.47x | 642.4 ± 1.4 | 0.99x |
| reduccion | 12x12 | ARM | 120 | 16.8 ± 0.0 | 1.00x | 1323.7 ± 0.5 | 1.00x |
| reduccion | 12x12 | FPGA_v1 | 120 | 211.8 ± 0.2 | 0.08x | 1513.4 ± 2.0 | 0.87x |
| reduccion | 12x12 | FPGA_v2 | 120 | 31.4 ± 0.1 | 0.53x | 1333.8 ± 1.7 | 0.99x |
| reduccion | 12x12 | FPGA_v3 | 120 | 21.5 ± 0.1 | 0.78x | 1337.1 ± 0.7 | 0.99x |
| reduccion | 32x32 | ARM | 120 | 72.1 ± 0.0 | 1.00x | 4602.9 ± 7.3 | 1.00x |
| reduccion | 32x32 | FPGA_v1 | 120 | 816.3 ± 0.2 | 0.09x | 5317.0 ± 14.6 | 0.87x |
| reduccion | 32x32 | FPGA_v2 | 120 | 240.9 ± 99.6 | 0.30x | 5073.3 ± 131.7 | 0.91x |
| reduccion | 32x32 | FPGA_v3 | 120 | 63.8 ± 0.7 | 1.13x | 4614.0 ± 11.0 | 1.00x |
| matmul | 4x4 | ARM | 120 | 47.0 ± 0.0 | 1.00x | 4868.1 ± 9.0 | 1.00x |
| matmul | 4x4 | FPGA_v1 | 120 | 509.2 ± 0.5 | 0.09x | 5210.4 ± 12.9 | 0.93x |
| matmul | 4x4 | FPGA_v2 | 120 | 88.1 ± 0.8 | 0.53x | 4893.3 ± 15.6 | 0.99x |
| matmul | 4x4 | FPGA_v3 | 120 | 58.0 ± 0.2 | 0.81x | 4879.3 ± 13.5 | 1.00x |
| matmul | 12x12 | ARM | 120 | 46.5 ± 0.0 | 1.00x | 3547.7 ± 10.1 | 1.00x |
| matmul | 12x12 | FPGA_v1 | 120 | 531.7 ± 0.3 | 0.09x | 3922.8 ± 11.9 | 0.90x |
| matmul | 12x12 | FPGA_v2 | 120 | 73.8 ± 0.2 | 0.63x | 3486.6 ± 26.1 | 1.02x |
| matmul | 12x12 | FPGA_v3 | 120 | 44.6 ± 0.4 | 1.04x | 3471.0 ± 13.9 | 1.02x |
| matmul | 32x32 | ARM | 120 | 116.9 ± 0.2 | 1.00x | 5432.1 ± 8.8 | 1.00x |
| matmul | 32x32 | FPGA_v1 | 120 | 954.7 ± 0.3 | 0.12x | 6360.0 ± 18.8 | 0.85x |
| matmul | 32x32 | FPGA_v2 | 120 | 132.3 ± 0.2 | 0.88x | 5414.7 ± 13.3 | 1.00x |
| matmul | 32x32 | FPGA_v3 | 120 | 74.0 ± 0.3 | 1.58x | 5451.6 ± 16.1 | 1.00x |
| convolucion | 4x4 | ARM | 120 | 17.4 ± 0.0 | 1.00x | 1788.0 ± 3.5 | 1.00x |
| convolucion | 4x4 | FPGA_v1 | 120 | 189.8 ± 0.2 | 0.09x | 1985.7 ± 16.3 | 0.90x |
| convolucion | 4x4 | FPGA_v2 | 120 | 34.6 ± 0.2 | 0.50x | 1790.9 ± 6.0 | 1.00x |
| convolucion | 4x4 | FPGA_v3 | 120 | 25.5 ± 0.1 | 0.68x | 1801.6 ± 4.0 | 0.99x |
| convolucion | 12x12 | ARM | 120 | 29.1 ± 0.0 | 1.00x | 2181.2 ± 8.6 | 1.00x |
| convolucion | 12x12 | FPGA_v1 | 120 | 340.7 ± 0.3 | 0.09x | 2472.3 ± 8.2 | 0.88x |
| convolucion | 12x12 | FPGA_v2 | 120 | 51.3 ± 0.6 | 0.57x | 2232.2 ± 42.5 | 0.98x |
| convolucion | 12x12 | FPGA_v3 | 120 | 30.6 ± 0.1 | 0.95x | 2177.0 ± 8.9 | 1.00x |
| convolucion | 32x32 | ARM | 120 | 86.8 ± 0.0 | 1.00x | 4924.6 ± 6.3 | 1.00x |
| convolucion | 32x32 | FPGA_v1 | 120 | 875.9 ± 0.2 | 0.10x | 5712.5 ± 13.4 | 0.86x |
| convolucion | 32x32 | FPGA_v2 | 120 | 121.1 ± 0.2 | 0.72x | 4982.2 ± 12.3 | 0.99x |
| convolucion | 32x32 | FPGA_v3 | 120 | 69.0 ± 0.4 | 1.26x | 4956.0 ± 11.7 | 0.99x |

## Desglose de E4 en la FPGA (media, ms)

| programa | malla | variante | reservar | empacar | H2D | kernel | D2H |
|---|---|---|---|---|---|---|---|
| reduccion | 4x4 | FPGA_v1 | 2.63 | 5.39 | 0.21 | 59.78 | 0.14 |
| reduccion | 4x4 | FPGA_v2 | 2.73 | 5.39 | 0.21 | 5.32 | 0.13 |
| reduccion | 4x4 | FPGA_v3 | 2.95 | 3.01 | 0.23 | 5.34 | 0.13 |
| reduccion | 12x12 | FPGA_v1 | 4.21 | 17.01 | 0.33 | 189.62 | 0.14 |
| reduccion | 12x12 | FPGA_v2 | 4.05 | 17.00 | 0.33 | 9.41 | 0.14 |
| reduccion | 12x12 | FPGA_v3 | 4.60 | 6.46 | 0.34 | 9.41 | 0.13 |
| reduccion | 32x32 | FPGA_v1 | 7.97 | 70.67 | 0.80 | 735.33 | 0.15 |
| reduccion | 32x32 | FPGA_v2 | 132.55 | 73.62 | 0.81 | 32.50 | 0.17 |
| reduccion | 32x32 | FPGA_v3 | 7.91 | 21.19 | 0.82 | 32.49 | 0.14 |
| matmul | 4x4 | FPGA_v1 | 6.84 | 41.32 | 0.53 | 459.54 | 0.22 |
| matmul | 4x4 | FPGA_v2 | 7.48 | 41.45 | 0.53 | 37.66 | 0.21 |
| matmul | 4x4 | FPGA_v3 | 6.45 | 12.40 | 0.53 | 37.67 | 0.20 |
| matmul | 12x12 | FPGA_v1 | 6.10 | 43.21 | 0.55 | 480.81 | 0.22 |
| matmul | 12x12 | FPGA_v2 | 6.14 | 43.17 | 0.55 | 22.92 | 0.21 |
| matmul | 12x12 | FPGA_v3 | 6.70 | 13.46 | 0.57 | 22.92 | 0.20 |
| matmul | 32x32 | FPGA_v1 | 8.29 | 84.05 | 0.89 | 859.70 | 0.23 |
| matmul | 32x32 | FPGA_v2 | 7.73 | 84.13 | 0.88 | 37.79 | 0.21 |
| matmul | 32x32 | FPGA_v3 | 8.26 | 25.28 | 0.89 | 37.80 | 0.21 |
| convolucion | 4x4 | FPGA_v1 | 4.43 | 15.22 | 0.28 | 169.11 | 0.22 |
| convolucion | 4x4 | FPGA_v2 | 4.22 | 15.23 | 0.28 | 14.18 | 0.22 |
| convolucion | 4x4 | FPGA_v3 | 4.74 | 5.58 | 0.28 | 14.19 | 0.21 |
| convolucion | 12x12 | FPGA_v1 | 5.26 | 27.56 | 0.40 | 306.57 | 0.23 |
| convolucion | 12x12 | FPGA_v2 | 7.20 | 28.00 | 0.40 | 14.86 | 0.24 |
| convolucion | 12x12 | FPGA_v3 | 4.73 | 9.76 | 0.40 | 14.84 | 0.21 |
| convolucion | 32x32 | FPGA_v1 | 8.19 | 76.21 | 0.82 | 788.98 | 0.23 |
| convolucion | 32x32 | FPGA_v2 | 7.47 | 76.30 | 0.82 | 34.77 | 0.22 |
| convolucion | 32x32 | FPGA_v3 | 8.46 | 23.27 | 0.82 | 34.78 | 0.22 |

## Porcentaje del total por etapa en la CPU ARM

| programa | malla | E2 | E3 | E4 | E5 |
|---|---|---|---|---|---|
| reduccion | 4x4 | 24.2 % | 4.1 % | 0.9 % | 70.6 % |
| reduccion | 12x12 | 27.9 % | 4.8 % | 1.3 % | 65.9 % |
| reduccion | 32x32 | 27.7 % | 4.8 % | 1.6 % | 65.8 % |
| matmul | 4x4 | 22.4 % | 4.0 % | 1.0 % | 72.6 % |
| matmul | 12x12 | 25.6 % | 4.7 % | 1.3 % | 68.4 % |
| matmul | 32x32 | 27.8 % | 5.6 % | 2.2 % | 64.5 % |
| convolucion | 4x4 | 24.2 % | 4.1 % | 1.0 % | 70.6 % |
| convolucion | 12x12 | 25.4 % | 4.8 % | 1.3 % | 68.4 % |
| convolucion | 32x32 | 27.5 % | 5.3 % | 1.8 % | 65.4 % |
