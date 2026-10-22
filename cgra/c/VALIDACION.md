# Validación del port a C

Referencia: Python 3.14.4, árbol de trabajo basado en `47451c0`.
Se usaron los `.py` actuales, incluidas sus modificaciones locales, sin
cambiarlos para acomodar el port. Hardware actual: 2×3, adaptación activada,
mapa efectivo `M C M / M C M`, ancho vectorial 4 y 4096 palabras.

## Comprobaciones ejecutadas

- GCC y clang-18: compilación C11 con `-Wall -Wextra`, sin advertencias.
- AddressSanitizer, UndefinedBehaviorSanitizer y LeakSanitizer: las 13
  pruebas pasan sin diagnósticos. LeakSanitizer se ejecutó fuera del sandbox,
  donde ptrace impide que funcione.
- Suite diferencial: 13 pruebas con ambos compiladores. Incluye subcasos
  para las tres operaciones y sus cuatro combinaciones de salida/traza.
- Reportes completos idénticos byte a byte, imágenes finales idénticas y
  salida de consola idéntica tras normalizar rutas temporales de comparación.
- Las 20 instrucciones; operaciones vectoriales y escalares, reducción con
  cancelación, valores fraccionarios, ceros con signo, infinitos y NaN.
- Snapshot de mensajes vectoriales, comunicación en ambos sentidos y
  visibilidad de escrituras/lecturas de distintos PEs en el mismo ciclo.
- 16 geometrías con adaptación activada y desactivada: 1×1, 1×2, 2×1, 1×3,
  3×1, 2×2, 3×2, 1×4, 2×3, 2×4, 1×10, 2×10, 3×4, 4×3, 6×10 y 11×1.
  Estas pruebas de configuración usan programas NOP para aislar el mapa;
  no equivalen a compilar las tres operaciones para cada geometría.
- Distribuciones planas, por paridad y por ciclo; precedencia de mapa
  explícito; hardware genérico sin `tipos_pe`.
- Errores de registros, bancos, permisos, división entre cero, transferencias,
  límites vectoriales, capacidad insuficiente e imagen truncada.
- `todas`: continúa después del fallo de una operación, conserva el resumen
  y rechaza opciones de entrada/salida que solo corresponden a una operación.
- Perfilado: una línea con 15 campos; E4a + E4b no supera E4. Recolección de
  120 muestras de prueba con cinco calentamientos y generación de resumen y
  dos gráficos, exclusivamente en `/tmp`.

Los builds de verificación y los resultados de prueba están fuera del
repositorio. El reporte opcional de rendimiento del skill 09 requiere
mediciones reales y no forma parte de esta comprobación.

## Diferencias y límites conocidos

- La suite Python existente pasa 66/67 pruebas. El fallo previo es
  `test_11_cada_tipo_de_src_cgra_json_ejecuta_solo_lo_suyo`: exige E y V en el
  mapa activo, aunque la adaptación actual lo convierte en dos filas M C M.
  No se modificó esa prueba ni la configuración.
- Los errores se comprueban por causa/contexto. La ayuda/uso del CLI C y
  algunos diagnósticos de JSON malformado no reproducen literalmente el
  texto de `argparse`/`json.JSONDecodeError`; no se afirma identidad de todos
  los mensajes de error posibles. Los registros inválidos se detectan al
  parsear, antes que en el ejecutor Python.
- La suma vectorial de referencia es la suma compensada de CPython moderno;
  cambiar a un intérprete con otro algoritmo requiere repetir las pruebas.
- Los identificadores de tipos de PE admitidos por el lector C son letras
  ASCII. Los tipos actuales G/M/E/V/C están cubiertos. No se afirma soporte
  de identificadores de tipo Unicode ni equivalencia universal para todas
  las entradas JSON no usadas por el simulador del proyecto.
