#include <math.h>
#include "contracts.h"

/* Реализация №1: метод прямоугольников (левые), Евклид */
float sin_integral(float a, float b, float e) {
    if (e <= 0.0f || b <= a) return 0.0f;
    double sum = 0.0, x = a;
    while (x < b) {
        double h = (x + e > b) ? (b - x) : e;
        sum += sin(x) * h;
        x += h;
    }
    return (float)sum;
}

int gcd(int a, int b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}
