# E4 de la CGRA en la FPGA (KV260): segundo acelerador

La etapa E4 (ejecución de la malla) corre como un IP de HLS en la Kria KV260.
A diferencia de la GPU, donde se lanzan 3 kernels por ciclo, el IP ejecuta
**todos los ciclos de la malla en una sola llamada**, con todo el estado
(registros, vectores, memoria local y colas) en la URAM del chip. E1–E3 y E5
quedan en la CPU ARM de la placa.

## Archivos

| Archivo | Qué es |
|---|---|
| `cgra_hls.h` / `cgra_hls.cpp` | El kernel `cgra_e4` (variantes 1 y 2) |
| `cgra_tb.cpp` + `tb_datos/` | Testbench de C-sim y cosim: compara la memoria final con la de la CPU (`reduccion 4x4`) |
| `malla_fpga.c` | Reemplaza a `malla.c` en `cgra_fpga`: empaca las instrucciones, maneja los buffers (XRT, como `pynq.allocate`) y el IP (`/dev/mem`) |
| `compilar_fpga.sh` | `MODO=hw` → `cgra/c/build/cgra_fpga` (Kria); `MODO=sw` → `cgra_fpga_sw_v<k>` (emulación en cualquier PC) |
| `preparar_entradas.sh` | Arma `cgra/c/perfilado/entradas/` (18 configuraciones) |
| `verificar_fpga.sh` | CPU contra FPGA: salida, reporte y memoria final idénticos |
| `run_hls.tcl` / `build_bitstream.tcl` / `correr_cgra.bat` | Vitis HLS, Vivado y el `.bat` de Windows |
| `cargar_overlay.py` | Carga el bitstream con PYNQ y escribe `overlay/regs.txt` |
| `perfilar_fpga.sh` | 120 muestras por configuración: CPU ARM y FPGA, con desglose de E4 |

## Variantes del kernel

| Variante | Qué hace |
|---|---|
| 1 | Port directo de `malla.c`: dos pasadas por ciclo (SEND, luego el resto en orden de PE); cada instrucción se lee de la DDR en cada pasada |
| 2 | La fila de instrucciones del ciclo se lee **una vez en ráfaga** al chip; los LD/ST van en una pasada aparte y se le indica a Vitis que los PEs son independientes (`DEPENDENCE ... inter false`) para encauzar la pasada de cómputo |

Las instrucciones se empacan como `[ciclo × pes + pe]` (4 palabras de 64 bits cada una) para que un ciclo completo sea contiguo en la DDR.

**Límites del hardware** (salen de los programas reales): hasta 1024 PEs (32×32), `ancho_vector` 4, 16 384 palabras de memoria y `local[0..63]` (el índice más alto usado es 60).

## Reproducir

### 1. Emulación y verificación (cualquier PC con gcc/g++)

```bash
cd cgra/fpga
./preparar_entradas.sh
make -C ../c                                  # cgra (CPU)
MODO=sw VARIANTE=2 ./compilar_fpga.sh          # cgra_fpga_sw_v2
./verificar_fpga.sh ../c/build/cgra_fpga_sw_v2 # esperado: iguales: 18, distintos: 0
```

### 2. Síntesis y bitstream (Windows, Vitis/Vivado 2026.1)

Copiar la carpeta `fpga` a una ruta corta (por ejemplo `C:\cgra\fpga`) y dar doble clic a
`correr_cgra.bat`. Corre C-sim, síntesis y cosim de las dos variantes, genera los bitstreams y deja
`cgra_fpga_para_ronald.zip` con `reportes/`, `overlay/` y los logs.

Equivalente por consola:

```bat
vitis-run --mode hls --tcl run_hls.tcl
vivado -mode batch -source build_bitstream.tcl -tclargs 2
```

### 3. En la Kria KV260

```bash
cd cgra/fpga
./preparar_entradas.sh
MODO=hw ./compilar_fpga.sh                     # cgra/c/build/cgra_fpga (enlaza libxrt_core)
sudo bash -c 'source /etc/profile.d/pynq_venv.sh && python3 cargar_overlay.py overlay/cgra_v2.bit'
sudo ./verificar_fpga.sh                       # esperado: iguales: 18, distintos: 0
sudo -E ./perfilar_fpga.sh 120 FPGA_v2         # ~2 h: CPU ARM + FPGA, 9 configuraciones
```

Para medir la variante 1 sin repetir la CPU: cargar `overlay/cgra_v1.bit` y correr
`sudo -E SOLO_FPGA=1 ./perfilar_fpga.sh 120 FPGA_v1`.

Los CSV quedan en `cgra/c/perfilado/` con el mismo formato que los de la Jetson
(`<programa>_<malla>_<variante>.csv`) y el desglose de E4 en `..._desglose.csv`
(`preparar`, `h2d`, `kernel`, `d2h`, en ns).

## Resultados esperados

- Verificación: **18/18 iguales** a la CPU (ya comprobado en emulación en x86 y en el ARM de la Kria).
- Testbench de Vitis: `*** PASA: memoria final bit a bit igual a la CPU ***` en C-sim y cosim.
- En la CPU de la Kria, E4 es 6–116 ms de un total de 0.6–5.3 s (1–2 %): la FPGA acelera E4,
  pero por la ley de Amdahl el total cambia poco. El cuello de botella sigue siendo E5
  (`seccion_ciclos`) y E2, que se proponen como siguiente paso.
