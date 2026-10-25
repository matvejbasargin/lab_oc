#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define CUT 1024

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int free_slots;
static int run = 1;
static int peak = 1;
static bool demo = false;
static bool show = false;
static _Thread_local bool slept = false;

static bool take_slot(void)
{
	pthread_mutex_lock(&m);
	bool ok = free_slots > 0;
	if (ok) {
		free_slots--;
		run++;
		if (run > peak) peak = run;
	}
	pthread_mutex_unlock(&m);
	return ok;
}

static void give_slot(void)
{
	pthread_mutex_lock(&m);
	free_slots++;
	run--;
	pthread_mutex_unlock(&m);
}

static void merge(int *a, int *t, size_t n, size_t mid)
{
	size_t i = 0, j = mid, k = 0;
	while (i < mid && j < n)
		t[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
	while (i < mid) t[k++] = a[i++];
	while (j < n)   t[k++] = a[j++];
	memcpy(a, t, n * sizeof(int));
}

static void sort_seq(int *a, int *t, size_t n)
{
	if (n < 2) return;
	size_t mid = n / 2;
	sort_seq(a, t, mid);
	sort_seq(a + mid, t + mid, n - mid);
	merge(a, t, n, mid);
}

typedef struct { int *a; int *t; size_t n; } Task;

static void sort_par(int *a, int *t, size_t n);

static void *sort_thread(void *p)
{
	Task *k = (Task *)p;
	sort_par(k->a, k->t, k->n);
	return NULL;
}

static void sort_par(int *a, int *t, size_t n)
{
	if (n < 2) return;
	size_t mid = n / 2;

	if (n >= CUT && take_slot()) {
		Task k = {a, t, mid};
		pthread_t th;
		if (pthread_create(&th, NULL, sort_thread, &k) == 0) {
			sort_par(a + mid, t + mid, n - mid);
			pthread_join(th, NULL);
			give_slot();
		} else {
			give_slot();
			sort_seq(a, t, n);
			return;
		}
		merge(a, t, n, mid);
	} else {
		if (demo && !slept) {
			slept = true;
			sleep(5);
		}
		sort_seq(a, t, n);
	}
}

static double now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void print_arr(const char *title, const int *a, long n)
{
	long k = n < 10 ? n : 10;
	printf("%s\n  начало:", title);
	for (long i = 0; i < k; i++) printf(" %d", a[i]);
	printf("\n  конец:  ");
	for (long i = n - k; i < n; i++) printf(" %d", a[i]);
	printf("\n");
}

int main(int argc, char **argv)
{
	long n = 10000000;
	int th = 1;

	int o;
	while ((o = getopt(argc, argv, "n:t:dp")) != -1) {
		if (o == 'n') n = atol(optarg);
		else if (o == 't') th = atoi(optarg);
		else if (o == 'd') demo = true;
		else if (o == 'p') show = true;
		else return 1;
	}
	if (th < 1) th = 1;
	free_slots = th - 1;

	int *a = malloc(n * sizeof(int));
	int *b = malloc(n * sizeof(int));
	int *t = malloc(n * sizeof(int));
	if (!a || !b || !t) {
		fprintf(stderr, "нет памяти\n");
		return 1;
	}
	srand(1);
	for (long i = 0; i < n; i++)
		a[i] = b[i] = rand();

	printf("PID=%d, логических ядер=%ld, n=%ld, потоков (лимит)=%d\n",
	       getpid(), sysconf(_SC_NPROCESSORS_ONLN), n, th);

	if (show) print_arr("Массив до сортировки:", a, n);

	double t0 = now_ms();
	sort_seq(b, t, n);
	double ts = now_ms() - t0;

	t0 = now_ms();
	sort_par(a, t, n);
	double tp = now_ms() - t0;

	if (show) print_arr("Массив после параллельной сортировки:", a, n);

	bool ok = memcmp(a, b, n * sizeof(int)) == 0;
	for (long i = 1; i < n && ok; i++)
		if (a[i - 1] > a[i]) ok = false;

	double sp = ts / tp;
	double ef = sp / th;

	printf("Проверка: %s, пик потоков: %d\n", ok ? "OK" : "ОШИБКА", peak);
	printf("Число потоков | Время исполнения (мс) | Ускорение | Эффективность\n");
	printf("%13d | %21.2f | %9.3f | %13.3f   (Ts = %.2f мс)\n", th, tp, sp, ef, ts);

	free(a); free(b); free(t);
	return 0;
}
