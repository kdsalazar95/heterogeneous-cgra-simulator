#include "../memoria_cgra.h"

#define TAMANO_VECTOR 100

// Crea e inicializa los arreglos y guarda la memoria inicial de la CGRA en
// memoria.bin. Solo prepara los datos: los skills no la calendarizan.
static void inicializar_memoria(float a[TAMANO_VECTOR], float b[TAMANO_VECTOR], float c[TAMANO_VECTOR],
                                float *result) {
    for (int i = 0; i < TAMANO_VECTOR; ++i) {
        a[i] = (float)(i % 4 + 1);
        b[i] = (float)(i % 4 + 1);
        c[i] = 0.0f;
    }
    *result = 0.0f;

    const RegionMemoria regiones[] = {
        {"a", a, 1, TAMANO_VECTOR},
        {"b", b, 1, TAMANO_VECTOR},
        {"c", c, 1, TAMANO_VECTOR},
        {"result", result, 1, 1},
    };
    guardar_memoria(ARCHIVO_MEMORIA, regiones, sizeof regiones / sizeof regiones[0]);
}

int main(void) {
    float a[TAMANO_VECTOR];
    float b[TAMANO_VECTOR];
    float c[TAMANO_VECTOR];
    float result;

    inicializar_memoria(a, b, c, &result);

    for (int i = 0; i < TAMANO_VECTOR; ++i) {
        c[i] = a[i] + b[i];
    }

    for (int i = 0; i < TAMANO_VECTOR; ++i) {
        result += c[i];
    }

    (void)result;
    return 0;
}
