"""Lógica para cargar, validar y ejecutar una reducción sobre una CGRA 2x2."""

import json
import re
from pathlib import Path

from queue_pe import PE, crear_malla, conectar_malla


PE_IDS = ("PE00", "PE01", "PE10", "PE11")
DIRECCIONES_OPUESTAS = {"north": "south", "south": "north", "east": "west", "west": "east"}
DESPLAZAMIENTOS = {"north": (-1, 0), "south": (1, 0), "west": (0, -1), "east": (0, 1)}
DIRECCIONES_EN_ESPANOL = {"north": "norte", "south": "sur", "west": "oeste", "east": "este"}

PATRON_CICLO = re.compile(r"^(\d+):\s*(.*)$")
# Valida instrucciones con el formato "ciclo: instrucción".
# (\d+) captura el número de ciclo y (.*) captura la instrucción.
# ciclo: instruccion
# Ejemplo: 0: ADD acc, acc, r0

PATRON_MEMORIA = re.compile(r"^([A-Za-z_]\w*)\[(\d+)\]$")
# Valida operandos de memoria con el formato "banco[índice]".
# ([A-Za-z_]\w*) captura el nombre del banco y (\d+) captura el índice.
#
# Ejemplo: a[0], b[5]

#===================================================================================
Funciones de utilidad para dar formato a números, imprimir títulos y cajas de texto


# ============================= Inicio =============================================

def fmt(numero):
    """Da formato compacto a enteros y flotantes, como en ``reduccion.py``."""
    return f"{round(numero, 6):g}" if isinstance(numero, (int, float)) else str(numero)


def imprimir_titulo(texto):
    """Imprime un título con el mismo estilo visual que ``reduccion.py``."""
    ancho = len(texto) + 2
    print()
    print("┌" + "─" * ancho + "┐")
    print(f"│ {texto} │")
    print("└" + "─" * ancho + "┘")


def caja(lineas):
    """Imprime un resultado en una caja de texto."""
    ancho = max(len(linea) for linea in lineas) + 2
    print("┌" + "─" * ancho + "┐")
    for linea in lineas:
        print(f"│ {linea.ljust(ancho - 1)}│")
    print("└" + "─" * ancho + "┘")


def formatear_lista(valores):
    return "[" + ", ".join(fmt(valor) for valor in valores) + "]"


def formato_pe(pe_id):
    """Convierte ``PE01`` al formato visual ``PE(0,1)``."""
    return f"PE({pe_id[2]},{pe_id[3]})"

#============================= Fin ==========================================

#================== Lectura de instrucciones .txt ===========================
def numero(texto):
    """Lee una constante entera o flotante de MOV."""
    valor = float(texto)
    return int(valor) if valor.is_integer() else valor

# Esta funcion convierte los numeros que vienen como texto a numeros de Python

# Ejemplo: mov r0, 5
# numero("5") -> 5 (devuelve 5)
# lo mismo sucede si fuera un valor flotante: mov r0, 5.5

def operando_memoria(texto, ruta, ciclo):
    coincidencia = PATRON_MEMORIA.fullmatch(texto) # comprobacion del formato correcto
    if not coincidencia:
        raise ValueError(f"{ruta}, ciclo {ciclo}: memoria inválida '{texto}'")
    return coincidencia.group(1), int(coincidencia.group(2)) # separa a[3] en ("a", 3)

# La funcion interpreta una direccion de memoria (identifica que banco de memoria y que posicion 
#se estan utilizando)

# a[0]                 a: banco,  3: indice
# b[5]
# result[0]

# Instrucciones como LD acc, a[3]
# se puede convertir en un diccionario para que el PE lo pueda ejecutar
# por ejemplo:
# {"op": "ld", "dst": "acc", "bank": "a", "index": 3}

def parsear_instruccion(texto, ruta, ciclo):
    """Convierte la sintaxis del skill 08 a un diccionario para ``PE.execute``."""
    partes = texto.replace(",", " ").split()
    if not partes:
        raise ValueError(f"{ruta}, ciclo {ciclo}: instrucción vacía")
    op = partes[0].upper()
    argumentos = partes[1:]

    if op == "NOP" and not argumentos:
        return {"op": "nop"}
    if op == "MOV" and len(argumentos) == 2:
        return {"op": "mov", "dst": argumentos[0], "imm": numero(argumentos[1])}
    if op == "LD" and len(argumentos) == 2:
        banco, indice = operando_memoria(argumentos[1], ruta, ciclo)
        return {"op": "ld", "dst": argumentos[0], "bank": banco, "index": indice}
    if op == "ST" and len(argumentos) == 2:
        banco, indice = operando_memoria(argumentos[0], ruta, ciclo)
        return {"op": "st", "bank": banco, "index": indice, "src": argumentos[1]}
    if op in {"ADD", "SUB", "MUL", "DIV"} and len(argumentos) == 3:
        return {"op": op.lower(), "dst": argumentos[0], "src1": argumentos[1], "src2": argumentos[2]}
    if op == "SEND" and len(argumentos) == 2:
        return {"op": "send", "dir": argumentos[0].lower(), "src": argumentos[1]}
    if op == "RECV" and len(argumentos) == 2:
        return {"op": "recv", "dir": argumentos[0].lower(), "dst": argumentos[1]}
    raise ValueError(f"{ruta}, ciclo {ciclo}: sintaxis no válida: '{texto}'")


# Toma una instruccion escrita en un .txt y traduce al formato que entienda el PE.execute()
# por ejemplo, la instruccion ADD acc, acc, r0 se traduce a:
# {"op": "add", "dst": "acc", "src1": "acc", "src2": "r0"}

def cargar_programa_pe(ruta):
    """Carga un archivo del skill 08 y devuelve ``{ciclo: instrucción}``."""
    programa = {}
    for numero_linea, linea in enumerate(Path(ruta).read_text(encoding="utf-8").splitlines(), start=1): # Abre el archivo y lo recorre linea por linea
        texto = linea.split("//", 1)[0].strip()
        if not texto:
            continue
        coincidencia = PATRON_CICLO.fullmatch(texto)
        if not coincidencia:
            raise ValueError(f"{ruta}, línea {numero_linea}: se esperaba 'NN: INSTRUCCIÓN'")
        ciclo = int(coincidencia.group(1))
        if ciclo in programa:
            raise ValueError(f"{ruta}, ciclo {ciclo}: ciclo duplicado")
        programa[ciclo] = parsear_instruccion(coincidencia.group(2), ruta, ciclo)
    if not programa:
        raise ValueError(f"{ruta}: programa vacío")
    return programa

# Carga y convierte el contenido de un archivo PExx.txt en un programa que la cgra puede ejecutar

def cargar_programas_pe(directorio):
    """Carga y valida PE00.txt, PE01.txt, PE10.txt y PE11.txt."""
    directorio = Path(directorio)
    programas = {}
    for pe_id in PE_IDS: 
        ruta = directorio / f"{pe_id}.txt"
        if not ruta.is_file():
            raise FileNotFoundError(f"No existe {ruta}")
        programas[pe_id] = cargar_programa_pe(ruta)
    validar_programas(programas)
    return programas

# Esta funcion trabaja con lo 4 PEs
# carga los cuatro archivos .txt que representan los programas de los PEs y los valida antes de ejecutar la CGRA.

def vecino(pe_id, direccion):
    """Obtiene el PE vecino de un PE dado según la dirección indicada."""
    
    # Obtiene la fila y columna del PE
    fila, columna = int(pe_id[2]), int(pe_id[3])

    # Obtiene el desplazamiento correspondiente a la dirección
    delta_fila, delta_columna = DESPLAZAMIENTOS.get(direccion, (None, None))

    # Verifica que la dirección sea válida
    if delta_fila is None:
        raise ValueError(f"Dirección inválida: {direccion}")

    # Calcula la posición del PE vecino
    destino = fila + delta_fila, columna + delta_columna

    # Verifica que el vecino esté dentro de la malla 2x2
    if not all(0 <= valor < 2 for valor in destino):
        raise ValueError(f"{pe_id}: no tiene vecino hacia {direccion}")

    # Devuelve el identificador del PE vecino
    return f"PE{destino[0]}{destino[1]}"


def validar_programas(programas):
    """Aplica las comprobaciones estructurales relevantes del skill 09."""
    ciclos = set(next(iter(programas.values())))
    if ciclos != set(range(max(ciclos) + 1)):
        raise ValueError("Los ciclos deben comenzar en 0 y ser consecutivos")
    for pe_id, programa in programas.items():
        if set(programa) != ciclos:
            raise ValueError(f"{pe_id}: rango de ciclos distinto al resto")

    for ciclo in sorted(ciclos):
        for emisor, programa in programas.items():
            instruccion = programa[ciclo]
            if instruccion["op"] == "send":
                receptor = vecino(emisor, instruccion["dir"])
                esperada = DIRECCIONES_OPUESTAS[instruccion["dir"]]
                recibida = programas[receptor][ciclo]
                if recibida["op"] != "recv" or recibida["dir"] != esperada:
                    raise ValueError(f"Ciclo {ciclo}: {emisor} SEND {instruccion['dir']} no coincide con {receptor} RECV {esperada}")
            elif instruccion["op"] == "recv":
                receptor = emisor
                emisor = vecino(receptor, instruccion["dir"])
                esperada = DIRECCIONES_OPUESTAS[instruccion["dir"]]
                enviada = programas[emisor][ciclo]
                if enviada["op"] != "send" or enviada["dir"] != esperada:
                    raise ValueError(f"Ciclo {ciclo}: {receptor} RECV {instruccion['dir']} no coincide con {emisor} SEND {esperada}")
# verifica que:
# 1. Los ciclos comienzan en 0 y son consecutivos
#               ↓
# 2. Todos los PEs tienen los mismos ciclos
#               ↓
# 3. Los SEND y RECV coinciden correctamente
#               ↓
#         Programa válido
 
def ejecutar_cgra(memoria, programas, mostrar_pasos=False):
    """Ejecuta una instrucción por PE y por ciclo; SEND se procesa primero.

    Si ``mostrar_pasos`` es verdadero, imprime cada comunicación de la
    reducción cuando su ADD receptor ya se ejecutó en el ciclo siguiente.
    """
    malla = crear_malla(2, 2)
    conectar_malla(malla)
    pes = {"PE00": malla[0][0], "PE01": malla[0][1], "PE10": malla[1][0], "PE11": malla[1][1]}

    pasos_pendientes = []
    numero_paso = 1
    for ciclo in sorted(next(iter(programas.values()))):
        for pe_id in PE_IDS:
            instruccion = programas[pe_id][ciclo]
            if instruccion["op"] == "send":
                if mostrar_pasos and instruccion["src"] == "acc":
                    receptor = vecino(pe_id, instruccion["dir"])
                    pasos_pendientes.append({
                        "ciclo_resultado": ciclo + 1,
                        "emisor": pe_id,
                        "receptor": receptor,
                        "direccion": instruccion["dir"],
                        "antes": pes[receptor]._register[PE.REGISTROS["acc"]],
                        "enviado": pes[pe_id]._register[PE.REGISTROS["acc"]],
                    })
                pes[pe_id].execute(instruccion, memoria)
        for pe_id in PE_IDS:
            instruccion = programas[pe_id][ciclo]
            if instruccion["op"] != "send":
                pes[pe_id].execute(instruccion, memoria)

        for paso in [paso for paso in pasos_pendientes if paso["ciclo_resultado"] == ciclo]:
            resultado = pes[paso["receptor"]]._register[PE.REGISTROS["acc"]]
            imprimir_titulo(f"PASO {numero_paso}: reduciendo en la malla")
            print(
                f"  {formato_pe(paso['emisor'])} -> {formato_pe(paso['receptor'])} "
                f"({DIRECCIONES_EN_ESPANOL[paso['direccion']]})   "
                f"{fmt(paso['antes'])} + {fmt(paso['enviado'])} = {fmt(resultado)}"
            )
            numero_paso += 1
    return malla

# Flujo de la funcion:
# Crear malla 2×2
#        ↓
# Conectar PEs
#        ↓
# Ciclo 0
#  ├─ ejecutar SEND
#  └─ ejecutar demás instrucciones
#        ↓
# Ciclo 1
#  ├─ ejecutar SEND
#  └─ ejecutar demás instrucciones
#        ↓
# Ciclo 2
#  ├─ ejecutar SEND
#  └─ ejecutar demás instrucciones
#       ↓
#      ...
#       ↓
# Devolver CGRA ejecutada

def cargar_memoria(ruta):
    """Carga y valida la memoria desde un archivo JSON."""
    memoria = json.loads(Path(ruta).read_text(encoding="utf-8"))
    if not isinstance(memoria, dict) or not all(isinstance(valores, list) for valores in memoria.values()):
        raise ValueError("La memoria debe ser un objeto JSON de bancos representados por listas")
    return memoria

#============================= Resultados de la CGRA =========================================

# Esta funcion toma la informacion de memoria y la presenta de forma ordenada en la pantalla

def mostrar_resultados(memoria, ciclos):
    """Muestra la salida de la simulación con el estilo de ``reduccion.py``."""
    imprimir_titulo(f"CGRA 2x2: ejecutando {ciclos} ciclos")
    print("  Programas por PE cargados y validados correctamente.")

    lineas = []
    if "c" in memoria:
        lineas.append(f"c = {formatear_lista(memoria['c'])}")
    banco_resultado = "result" if "result" in memoria else "resultado" if "resultado" in memoria else None
    if banco_resultado and memoria[banco_resultado]:
        lineas.append(f"RESULTADO FINAL: {fmt(memoria[banco_resultado][0])}")
    if not lineas:
        lineas.append("Ejecución terminada.")

    imprimir_titulo("RESULTADO DE LA CGRA")
    caja(lineas)
