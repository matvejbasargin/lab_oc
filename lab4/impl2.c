#include <math.h>
#include "contracts.h"

/* Реализация №2: метод трапеций, наивный НОД */
float sin_integral(float a, float b, float e) {
    if (e <= 0.0f || b <= a) return 0.0f;
    double sum = 0.0, x = a;
    while (x < b) {
        double h = (x + e > b) ? (b - x) : e;
        sum += (sin(x) + sin(x + h)) * h / 2.0;
        x += h;
    }
    return (float)sum;
}

int gcd(int a, int b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    if (a == 0) return b;
    if (b == 0) return a;
    int m = a < b ? a : b;
    for (int d = m; d > 1; d--)
        if (a % d == 0 && b % d == 0) return d;
    return 1;
}
