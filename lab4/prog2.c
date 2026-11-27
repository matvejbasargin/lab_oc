/* Программа №2: библиотеки загружаются во время исполнения (dlopen) */
#include <stdio.h>
#include <dlfcn.h>
#include "contracts.h"

static const char *paths[2] = {"./libimpl1.so", "./libimpl2.so"};

static void *handle = NULL;
static float (*p_sin_integral)(float, float, float);
static int (*p_gcd)(int, int);

static int load(int idx) {
    void *h = dlopen(paths[idx], RTLD_LAZY);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return -1; }
    float (*f1)(float, float, float) = (float (*)(float, float, float))dlsym(h, "sin_integral");
    int (*f2)(int, int) = (int (*)(int, int))dlsym(h, "gcd");
    if (!f1 || !f2) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        dlclose(h);
        return -1;
    }
    if (handle) dlclose(handle);
    handle = h;
    p_sin_integral = f1;
    p_gcd = f2;
    return 0;
}

int main(void) {
    int cur = 0;
    if (load(cur) != 0) return 1;
    printf("Используется реализация №%d\n", cur + 1);

    int cmd;
    while (scanf("%d", &cmd) == 1) {
        if (cmd == 0) {
            int next = 1 - cur;
            if (load(next) == 0) {
                cur = next;
                printf("Переключено на реализацию №%d\n", cur + 1);
            }
        } else if (cmd == 1) {
            float a, b, e;
            if (scanf("%f %f %f", &a, &b, &e) != 3) break;
            printf("%f\n", p_sin_integral(a, b, e));
        } else if (cmd == 2) {
            int a, b;
            if (scanf("%d %d", &a, &b) != 2) break;
            printf("%d\n", p_gcd(a, b));
        } else {
            printf("Неизвестная команда\n");
        }
    }
    if (handle) dlclose(handle);
    return 0;
}
