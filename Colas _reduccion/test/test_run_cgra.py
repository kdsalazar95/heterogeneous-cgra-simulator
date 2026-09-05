"""Pruebas del ejecutor de programas de la CGRA desde archivos PE*.txt."""

import copy
import os
import sys
import unittest

_raiz_proyecto = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(_raiz_proyecto, "src"))

from reduccion_cgra import cargar_programas_pe, ejecutar_cgra, validar_programas


class TestEjecucionCGRA(unittest.TestCase):
    """Verifica carga, validación y ejecución del calendario generado por LLVM."""

    def setUp(self):
        self.directorio_instrucciones = os.path.join(
            _raiz_proyecto, "src", "graphs", "pe_instructions"
        )
        self.memoria = {
            "a": [1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4],
            "b": [1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4],
            "c": [0] * 12,
            "result": [0],
        }

    def test_1_carga_los_cuatro_programas_con_ciclos_alineados(self):
        """Los cuatro archivos generados deben cargar y cubrir los mismos 24 ciclos."""
        programas = cargar_programas_pe(self.directorio_instrucciones)

        self.assertEqual(set(programas), {"PE00", "PE01", "PE10", "PE11"})
        for programa in programas.values():
            self.assertEqual(set(programa), set(range(24)))

    def test_2_ejecuta_mapa_y_reduccion_hasta_resultado_final(self):
        """La CGRA debe calcular c[i] = a[i] + b[i] y reducir los 12 valores a 60."""
        programas = cargar_programas_pe(self.directorio_instrucciones)
        malla = ejecutar_cgra(self.memoria, programas)

        self.assertEqual(self.memoria["c"], [2, 4, 6, 8, 2, 4, 6, 8, 2, 4, 6, 8])
        self.assertEqual(self.memoria["result"], [60])
        self.assertEqual(malla[0][0]._register[4], 60)  # acc de PE00

    def test_3_rechaza_un_recv_sin_send_correspondiente(self):
        """La validación debe detectar si un RECV no tiene su SEND vecino en el mismo ciclo."""
        programas = cargar_programas_pe(self.directorio_instrucciones)
        programas_invalidos = copy.deepcopy(programas)
        programas_invalidos["PE01"][19] = {"op": "nop"}

        with self.assertRaisesRegex(ValueError, "RECV east"):
            validar_programas(programas_invalidos)


if __name__ == "__main__":
    unittest.main()
