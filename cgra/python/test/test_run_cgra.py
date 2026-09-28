"""Pruebas del ejecutor de programas de la CGRA desde archivos PE*.txt."""

import copy
import os
import sys
import tempfile
import unittest

_raiz_proyecto = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _raiz_proyecto)

from programas_pe import cargar_programas_pe, ejecutar_cgra, validar_programas
from reportes_ciclos import contar_ciclos, formatear_estadisticas_ciclos, guardar_reporte_ciclos


class TestEjecucionCGRA(unittest.TestCase):
    """Verifica carga, validación y ejecución del calendario generado por LLVM."""

    def setUp(self):
        self.directorio_instrucciones = os.path.join(
            _raiz_proyecto, "test", "datos", "reduccion_2x2"
        )
        self.memoria = {
            "a": [1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4],
            "b": [1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4],
            "c": [0] * 12,
            "result": [0],
        }

    def test_1_carga_los_cuatro_programas_con_ciclos_alineados(self):
        """Los cuatro archivos generados deben cargar y cubrir los mismos 24 ciclos."""
        programas = cargar_programas_pe(self.directorio_instrucciones, 2, 2)

        self.assertEqual(set(programas), {"PE00", "PE01", "PE10", "PE11"})
        for programa in programas.values():
            self.assertEqual(set(programa), set(range(24)))

    def test_2_ejecuta_mapa_y_reduccion_hasta_resultado_final(self):
        """La CGRA debe calcular c[i] = a[i] + b[i] y reducir los 12 valores a 60."""
        programas = cargar_programas_pe(self.directorio_instrucciones, 2, 2)
        malla = ejecutar_cgra(self.memoria, programas, 2, 2)

        self.assertEqual(self.memoria["c"], [2, 4, 6, 8, 2, 4, 6, 8, 2, 4, 6, 8])
        self.assertEqual(self.memoria["result"], [60])
        self.assertEqual(malla[0][0]._register[4], 60)  # acc de PE00

    def test_3_rechaza_un_recv_sin_send_correspondiente(self):
        """La validación debe detectar si un RECV no tiene su SEND vecino en el mismo ciclo."""
        programas = cargar_programas_pe(self.directorio_instrucciones, 2, 2)
        programas_invalidos = copy.deepcopy(programas)
        programas_invalidos["PE01"][19] = {"op": "nop"}

        with self.assertRaisesRegex(ValueError, "RECV east"):
            validar_programas(programas_invalidos, 2, 2)


class TestEstadisticasCiclos(unittest.TestCase):
    """Pruebas del contador de ciclos de cómputo vs comunicación."""

    def setUp(self):
        directorio_instrucciones = os.path.join(_raiz_proyecto, "test", "datos", "reduccion_2x2")
        self.programas = cargar_programas_pe(directorio_instrucciones, 2, 2)

    def test_1_clasifica_los_24_ciclos_del_ejemplo(self):
        """De los 24 ciclos del ejemplo, solo los 2 con SEND/RECV deben
        contar como comunicación; el resto es cómputo y ninguno queda
        inactivo (no hay NOP en el programa de ejemplo)."""
        estadisticas = contar_ciclos(self.programas)

        self.assertEqual(estadisticas, {"total": 24, "comunicacion": 2, "computo": 22, "inactivo": 0})

    def test_2_un_ciclo_totalmente_nop_cuenta_como_inactivo(self):
        """Si todos los PEs hacen NOP en un ciclo, ese ciclo debe
        clasificarse como inactivo y no como cómputo."""
        programas = copy.deepcopy(self.programas)
        for programa in programas.values():
            programa[0] = {"op": "nop"}

        estadisticas = contar_ciclos(programas)

        self.assertEqual(estadisticas["inactivo"], 1)
        self.assertEqual(estadisticas["computo"], 21)

    def test_3_guardar_reporte_ciclos_escribe_los_totales_en_un_archivo(self):
        """guardar_reporte_ciclos debe crear un archivo de texto con el
        total de ciclos y el desglose de cómputo/comunicación."""
        with tempfile.TemporaryDirectory() as directorio_temporal:
            ruta = os.path.join(directorio_temporal, "reporte.txt")
            guardar_reporte_ciclos(self.programas, 2, 2, ruta)

            with open(ruta, encoding="utf-8") as archivo:
                contenido = archivo.read()

        self.assertIn("Total de ciclos:        24", contenido)
        self.assertIn("Ciclos de comunicación: 2 (8.3%)", contenido)

    def test_4_lista_los_pasos_de_comunicacion_con_direccion_y_receptor(self):
        """El reporte debe listar cada SEND del ejemplo con su ciclo,
        emisor, receptor y dirección (según la ruta de reducción del
        skill 06: PE01->PE00 y PE11->PE10 por el oeste, luego
        PE10->PE00 por el norte)."""
        pasos = formatear_estadisticas_ciclos(self.programas, 2, 2)
        texto = "\n".join(pasos)

        self.assertIn("Ciclo  19: PE01 -> PE00 (oeste)", texto)
        self.assertIn("Ciclo  19: PE11 -> PE10 (oeste)", texto)
        self.assertIn("Ciclo  21: PE10 -> PE00 (norte)", texto)

    def test_5_sin_comunicacion_lo_dice_explicitamente(self):
        """Si un programa no manda datos entre PEs (como el mapeo
        output-stationary de matmul), el reporte debe decirlo en vez de
        mostrar una lista vacía sin explicación."""
        programas = copy.deepcopy(self.programas)
        for programa in programas.values():
            for ciclo, instruccion in programa.items():
                if instruccion["op"] in {"send", "recv"}:
                    programa[ciclo] = {"op": "nop"}

        texto = "\n".join(formatear_estadisticas_ciclos(programas, 2, 2))

        self.assertIn("no manda datos entre PEs", texto)


if __name__ == "__main__":
    unittest.main()
