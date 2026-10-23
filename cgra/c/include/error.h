/*
 * Mensaje del último error.
 *
 * Las funciones que fallan anotan aquí el mensaje y devuelven -1; main.c
 * decide cómo mostrarlo, igual que run_cgra.py convierte las excepciones
 * en "No se pudo cargar el programa: ...".
 */

#ifndef ERROR_H
#define ERROR_H

// Error que Python no atrapa (una excepción sin capturar, como la división
// entre cero): termina el simulador, también en medio de 'todas'.
#define ERROR_EJECUCION -2

// Que el compilador revise los argumentos como los de printf.
#define FORMATO_PRINTF(formato, primero) __attribute__((format(printf, formato, primero)))

void anotar_error(const char *formato, ...) FORMATO_PRINTF(1, 2);
// Agrega un prefijo al mensaje actual: "prefijo" + mensaje.
void anteponer_error(const char *formato, ...) FORMATO_PRINTF(1, 2);
const char *ultimo_error(void);

#endif
