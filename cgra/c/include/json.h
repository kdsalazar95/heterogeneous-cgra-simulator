/*
 * Lector de JSON para cgra.json, con las mismas reglas y los mismos
 * mensajes de error que json.loads() de Python.
 */

#ifndef JSON_H
#define JSON_H

#include <stddef.h>

typedef enum { JSON_NULO, JSON_BOOLEANO, JSON_NUMERO, JSON_TEXTO, JSON_LISTA, JSON_OBJETO } TipoJson;

typedef struct Json {
    TipoJson tipo;
    char *texto;               // TEXTO: el contenido; NUMERO y BOOLEANO: tal como está escrito
    struct Json *elementos;    // LISTA y OBJETO
    char **claves;             // OBJETO: la clave de cada elemento
    size_t cantidad;
} Json;

// Lee el documento entero. Con una clave repetida queda el último valor en
// la posición de la primera, como en un dict de Python. Si falla, el error
// es el de JSONDecodeError: "Expecting value: line 1 column 1 (char 0)".
int  parsear_json(const char *texto, size_t largo, Json *raiz);   // 0 ok, -1 error
void liberar_json(Json *valor);

const Json *json_clave(const Json *objeto, const char *clave);    // NULL si no está
// Un int de Python: un número sin parte decimal ni exponente.
int  json_es_entero(const Json *valor);
// El valor como lo muestra str() en Python ("True", "None", "5"...).
const char *json_como_texto(const Json *valor);

#endif
