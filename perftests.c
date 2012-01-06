#include "critbit.h"

#include <assert.h>
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

struct sort_data
{
  uintptr_t *const a;
  size_t offset;
};

static void sort_callback(void *const ctx, const uintptr_t v)
{
  struct sort_data *const data = (struct sort_data *)ctx;
  data->a[data->offset++] = v;
}

struct free_list
{
  struct free_list *next;
};

struct node_storage
{
  char *node_arena;
  size_t next_index;
};

static void *alloc_critbit_node(void *const ctx)
{
  struct node_storage *const s = (struct node_storage *)ctx;
  void *const node = s->node_arena + (s->next_index * critbit_node_size());
  ++(s->next_index);
  return node;
}

static void free_critbit_node(void *const ctx, void *const node)
{
  (void)ctx;
  (void)node;
  /* do nothing */
}

static struct node_storage *create_node_storage(const size_t n)
{
  const size_t node_size = critbit_node_size();
  assert(node_size <= SIZE_MAX / (n - 1));
  void *const node_arena = malloc(node_size * (n - 1));

  struct node_storage *const s = malloc(sizeof(*s));
  s->node_arena = node_arena;
  s->next_index = 0;
  return s;
}

static void delete_node_storage(struct node_storage *const s)
{
  free(s->node_arena);
  free(s);
}

static size_t critbit_sort(uintptr_t *const a, const size_t n)
{
  struct node_storage *const s = create_node_storage(n);

  const struct critbit_node_allocator node_allocator = {
    .alloc_node = &alloc_critbit_node,
    .free_node = &free_critbit_node,
    .ctx = s,
  };
  struct critbit *const cb = critbit_create(&node_allocator);

  size_t m = n;
  for (size_t i = 0; i < n; ++i) {
    if (!critbit_add(cb, a[i])) {
      --m;
    }
  }

  struct sort_data data = {
    .a = a,
    .offset = 0,
  };
  const struct critbit_visitor sort_visitor = {
    .callback = &sort_callback,
    .ctx = &data,
  };
  critbit_foreach(cb, &sort_visitor);
  assert(data.offset == m);

  critbit_delete(cb);
  delete_node_storage(s);
  return data.offset;
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


