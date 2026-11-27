/* Программа №1: библиотека подключается на этапе компоновки */
#include <stdio.h>
#include "contracts.h"

int main(void) {
    int cmd;
    while (scanf("%d", &cmd) == 1) {
        if (cmd == 1) {
            float a, b, e;
            if (scanf("%f %f %f", &a, &b, &e) != 3) break;
            printf("%f\n", sin_integral(a, b, e));
        } else if (cmd == 2) {
            int a, b;
            if (scanf("%d %d", &a, &b) != 2) break;
            printf("%d\n", gcd(a, b));
        } else {
            printf("Неизвестная команда\n");
        }
    }
    return 0;
}
