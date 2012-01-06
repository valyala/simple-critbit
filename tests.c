#include "critbit.h"

#include <assert.h>
#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint*_t */
#include <stdio.h>
#include <stdlib.h>  /* for malloc()/free() */


static void *alloc_critbit_node(void *const ctx)
{
  assert(ctx == NULL);
  return malloc(critbit_node_size());
}

static void free_critbit_node(void *const ctx, void *const node)
{
  assert(ctx == NULL);
  free(node);
}

struct check_order_data
{
  uintptr_t prev_v;
};

static void check_order_callback(void *const ctx, const uintptr_t v)
{
  struct check_order_data *const data = (struct check_order_data *)ctx;
  assert(data->prev_v < v);
  data->prev_v = v;
}

static void test_critbit(const size_t n)
{
  printf("test_critbit(n=%zu) ", n);

  const struct critbit_node_allocator node_allocator = {
    .alloc_node = &alloc_critbit_node,
    .free_node = &free_critbit_node,
    .ctx = NULL,
  };
  struct critbit *const cb = critbit_create(&node_allocator);
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

  struct check_order_data data = {
    .prev_v = 0,
  };
  const struct critbit_visitor check_order_visitor = {
    .callback = &check_order_callback,
    .ctx = &data,
  };
  critbit_foreach(cb, &check_order_visitor);

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

  printf("OK\n");
}

int main(void)
{
  static const size_t N = 128 * 1024;

  test_critbit(N);

  return 0;
}


