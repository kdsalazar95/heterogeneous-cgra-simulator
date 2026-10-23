# Validación del port a C

Referencia: Python 3.14.4 y los `.py` de `cgra/src/` del árbol de trabajo
basado en `56cd2e2`, con sus cambios locales, sin modificarlos para el port.
Compiladores: GCC 15.2.0 y clang 18.1.8.

Los programas de `cgra/programas/` están compilados para una malla 2×3
(adaptación activada: `M C M / M C M`, ancho vectorial 4, 4096 palabras). El
`cgra/src/cgra.json` del árbol de trabajo dice `filas: 3`, así que las
operaciones se compararon con una copia de ese JSON con `filas: 2`, el
hardware para el que se compilaron. Con el JSON de 3×3, Python y C fallan
igual (`No existe .../PE20.txt`) y `todas` da la misma salida en ambos.

## Comprobaciones ejecutadas

- Compilación C11 con `-Wall -Wextra`, sin advertencias, con GCC y clang-18.
- AddressSanitizer, UndefinedBehaviorSanitizer (sin recuperación) y
  LeakSanitizer: la suite completa pasa sin diagnósticos.
- Suite diferencial `test/equivalence.py`: 19 pruebas, con GCC, clang-18 y el
  build con sanitizers. Compara salida en consola, traza, reporte completo
  byte a byte y memoria final escrita con `--write-back`.
- Las tres operaciones con salida normal, `--traza`, `--ciclos` y ambas.
- Las 20 instrucciones; reducción con cancelación (suma compensada de
  CPython), valores fraccionarios, ceros con signo, infinitos y NaN.
- Copias de mensajes vectoriales, comunicación en ambos sentidos y orden de
  escrituras y lecturas de distintos PEs en el mismo ciclo.
- Mapas: 16 geometrías (de 1×1 a 6×10, incluida 11×1, con IDs de dos
  dígitos) con adaptación activada y desactivada, y un programa en la
  rotación E/V/C de 3×3; distribuciones planas, por paridad y en ciclo;
  precedencia de `malla`; hardware genérico sin `tipos_pe`.
- Errores con el mensaje completo idéntico al de Python: 24 configuraciones
  inválidas, 11 JSON mal formados (línea, columna y carácter), JSON
  inexistente, 10 programas inválidos, 7 archivos de PE mal escritos, un PE
  sin archivo, la lista de más de 10 instrucciones no permitidas y 3
  imágenes de memoria dañadas.
- Comentarios, `\r\n`, `\r` y `\f` como fines de línea, mayúsculas y
  minúsculas, comas sin espacios y constantes como `1_000.5e-1`.
- `todas`: sigue después del fallo de una operación, muestra el resumen y
  rechaza `--memoria`, `--instrucciones`, `--reporte-ciclos` y `--perfil`.
- Perfilado: `--perfil` imprime una línea de 15 campos y E4a + E4b no supera
  E4. `perfilar.sh` con 120 muestras deja un CSV de 121 líneas y 16 columnas,
  y `analizar_perfil.py` informa 120 muestras por etapa y escribe
  `resumen.csv` y los dos gráficos. Todo en una copia fuera del proyecto:
  `cgra/c/perfilado/` quedó vacío.

## Diferencias y límites conocidos

- Un registro que no existe (`MOV rX, 1`): Python lo descubre al ejecutar esa
  instrucción y termina con un `ValueError` sin capturar; C lo detecta al
  final de la carga, antes del primer ciclo, en el mismo orden de ejecución
  y con el mismo texto (`Registro desconocido: rX`). Si un error de ejecución
  anterior (una división entre cero) ocurriera antes en Python, C informaría
  primero el registro.
- Los errores que Python no atrapa (división entre cero, valor que no cabe
  en float32 al guardar) se informan con una línea y el contexto
  (`Ciclo 1, PE00: division by zero`) en vez de un traceback.
- La ayuda y los errores de la línea de comandos no copian el texto de
  `argparse`; sí el código de salida (2).
- Algunos JSON con tipos inesperados (una lista donde va un texto, un tipo
  de PE que no es un objeto) dan un mensaje propio, donde Python falla con
  una excepción sin capturar.
- Las letras de los tipos de PE deben ser ASCII; Python también acepta otras
  letras Unicode.
- La suma vectorial reproduce `sum()` de CPython 3.12 o posterior; con otra
  versión hay que repetir la suite.
