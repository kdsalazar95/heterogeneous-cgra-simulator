#!/usr/bin/env python3
"""cargar_overlay.py - Carga el bitstream del IP cgra_e4 en la KV260 y escribe overlay/regs.txt.

El bitstream queda cargado en la PL después de salir; el simulador (cgra_fpga) lee
regs.txt para saber dónde están los registros AXI-Lite del IP.

Uso en la Kria (PYNQ necesita root):
  sudo bash -c 'source /etc/profile.d/pynq_venv.sh && python3 cargar_overlay.py overlay/cgra_v2.bit'
"""
import os
import sys

from pynq import Overlay

bit = sys.argv[1] if len(sys.argv) > 1 else "overlay/cgra_v2.bit"
ol = Overlay(bit)
nombre = [k for k in ol.ip_dict if "cgra_e4" in k][0]
ip = ol.ip_dict[nombre]
regs = ip["registers"]


def offset(arg):
    # Un puntero de 64 bits aparece como <arg>_1 (bits bajos) y <arg>_2, o solo <arg>.
    for clave in (arg + "_1", arg, arg.upper()):
        if clave in regs:
            return regs[clave]["address_offset"]
    raise SystemExit(f"No está el registro '{arg}' en el .hwh: {sorted(regs)}")


salida = os.path.join(os.path.dirname(os.path.abspath(bit)), "regs.txt")
with open(salida, "w") as f:
    f.write(f"base {ip['phys_addr']:x}\n")
    for arg in ("inst", "memoria", "estado", "pes", "columnas", "ciclos", "ancho", "palabras"):
        f.write(f"{arg} {offset(arg):x}\n")
os.chmod(salida, 0o644)
print(f"Cargado {bit} ({nombre} en 0x{ip['phys_addr']:x}); PL a {ol.clock_dict.get('clock0', {}).get('frequency', '?')} MHz")
print(open(salida).read())
