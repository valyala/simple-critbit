#include "critbit.h"

#include <assert.h>
#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint*_t */
#include <stdio.h>
#include <stdlib.h>  /* for malloc()/free() */

static void test_critbit(const size_t n)
{
  printf("test_critbit(n=%zu)\n", n);

  struct critbit *const cb = critbit_create();
  uintptr_t v;
  int rv;

  srand(0);
  for (size_t i = 0; i < n; ++i) {
    do {
      v = rand() * 2;
    } while (v == 0);
    rv = critbit_contains(cb, v);
    if (rv) {
      continue;
    }
    rv = critbit_add(cb, v);
    assert(rv);
    rv = critbit_contains(cb, v);
    assert(rv);
  }

  srand(0);
  for (size_t i = 0; i < n; ++i) {
    do {
      v = rand() * 2;
    } while (v == 0);
    rv = critbit_contains(cb, v);
    assert(rv);
    rv = critbit_add(cb, v);
    assert(!rv);
  }

  srand(0);
  for (size_t i = 0; i < n; ++i) {
    do {
      v = rand() * 2;
    } while (v == 0);
    rv = critbit_contains(cb, v);
    if (!rv) {
      continue;
    }
    rv = critbit_remove(cb, v);
    assert(rv);
    rv = critbit_contains(cb, v);
    assert(!rv);
  }

  srand(0);
  for (size_t i = 0; i < n; ++i) {
    do {
      v = rand() * 2;
    } while (v == 0);
    rv = critbit_contains(cb, v);
    assert(!rv);
    rv = critbit_remove(cb, v);
    assert(!rv);
  }

  critbit_delete(cb);
}

static void check_sort(uintptr_t *const a, const size_t n)
{
  for (size_t i = 1; i < n; ++i) {
    assert(a[i - 1] < a[i]);
  }
}

static void test_sort(const size_t n)
{
  printf("test_sort(n=%zu)\n", n);

  uintptr_t *const a = malloc(sizeof(a[0]) * n);

  for (size_t i = 0; i < n; ++i) {
    do {
      a[i] = rand() * 2;
    } while (a[i] == 0);
  }

  size_t m = critbit_sort(a, n);
  check_sort(a, m);

  free(a);
}

int main(void)
{
  static const size_t N = 128 * 1024;

  test_critbit(N);
  test_sort(N);

  return 0;
}


