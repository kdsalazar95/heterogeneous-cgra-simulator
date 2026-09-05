int main() {
    float a[12] = {1.0, 2.0, 3.0, 4.0, 1.0, 2.0, 3.0, 4.0, 1.0, 2.0, 3.0, 4.0};
    float b[12] = {1.0, 2.0, 3.0, 4.0, 1.0, 2.0, 3.0, 4.0, 1.0, 2.0, 3.0, 4.0};
    float c[12] = {0.0};

    for (int i = 0; i < 12; ++i) {
        c[i] = a[i] + b[i];
    }

    float result = 0.f;
    for (int i = 0; i < 12; ++i) {
        result += c[i];
    }

    return 0;
}
