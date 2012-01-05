#include "critbit.h"

#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint*_t */
#include <stdio.h>
#include <stdlib.h>  /* for malloc()/free() */
#include <time.h>    /* for clock() */

static double get_time(void)
{
  return (double)clock() / CLOCKS_PER_SEC;
}

static void print_performance(const double t, const size_t m)
{
  printf(": %.0lf Kops/s\n", m / t / 1000);
}

static void init_array(uintptr_t *const a, const size_t n)
{
  for (size_t i = 0; i < n; ++i) {
    do {
      a[i] = rand() * 2;
    } while (a[i] == 0);
  }
}

static void test_sort(const size_t n, const size_t m)
{
  printf("test_sort(n=%zu, m=%zu)", n, m);

  uintptr_t *const a = malloc(sizeof(a[0]) * n);

  double total_time = 0;
  srand(0);
  for (size_t i = 0; i < m / n; ++i) {
    init_array(a, n);
    double start = get_time();
    critbit_sort(a, n);
    double end = get_time();
    total_time += end - start;
  }
  print_performance(total_time, m);

  free(a);
}

int main(void)
{
  static const size_t MAX_N = 4 * 1024 * 1024;

  for (size_t i = 0; i < 20; ++i) {
    const size_t n = MAX_N >> i;
    test_sort(n, MAX_N);
  }

  return 0;
}


