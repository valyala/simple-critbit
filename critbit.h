#ifndef CRITBIT_H
#define CRITBIT_H

/*
 * Simple crit-bit implementation for C99.
 * Supports only keys of type uintptr_t.
 *
 * Author: Aliaksandr Valialkin <valyala@gmail.com>
 */

/*******************************************************************************
 * Interface.
 ******************************************************************************/
#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint*_t */

struct critbit;

/* Creates a crit-bit */
static inline struct critbit *critbit_create(void);

/* Deletes the given crit-bit */
static inline void critbit_delete(struct critbit *cb);

/* Adds v to crit-bit. Returs 1 on success, 0 if v already exists
 * in the crit-bit.
 */
static inline int critbit_add(struct critbit *cb, uintptr_t v);

/* Removes v from crit-bit. Returns 1 on success, 0 if v doesn't exist
 * in the crit-bit.
 */
static inline int critbit_remove(struct critbit *cb, uintptr_t v);

/* Returns 1 if crit-bit contains v, otherwise returns 0. */
static inline int critbit_contains(const struct critbit *cb, uintptr_t v);

/* Sorts the array a of size n. Returns the size of sorted array.
 * The size of sorted array can be smaller than the initial array,
 * since crit-bit sorting removes duplicate elements.
 */
static inline size_t critbit_sort(uintptr_t *a, size_t n);

/*******************************************************************************
 * Implementation.
 ******************************************************************************/
#include <assert.h>
#include <limits.h>  /* for CHAR_BIT */
#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint*_t */
#include <stdlib.h>  /* for malloc()/free() */


struct critbit
{
  uintptr_t root;
};

struct _critbit_node
{
  uintptr_t next[2];
  uint8_t crit_bit;
};

static const uint8_t _CRITBIT_PTR_BITS = sizeof(void *) * CHAR_BIT;

static inline int _critbit_has_tag(const uintptr_t v)
{
  assert(v != 0);

  return (v & 1);
}

static inline int _critbit_is_set(const uintptr_t v, const uint8_t bit)
{
  assert(bit < _CRITBIT_PTR_BITS - 1);

  return (v & (((uintptr_t)1) << (_CRITBIT_PTR_BITS - 1 - bit)));
}

static inline uint8_t _critbit_get_crit_bit(const uintptr_t v1,
    const uintptr_t v2)
{
  assert(v1 != v2);
  assert(!_critbit_has_tag(v1));
  assert(!_critbit_has_tag(v2));

  const uintptr_t x = v1 ^ v2;
  uint8_t crit_bit = 0;
  for (;;) {
    if (_critbit_is_set(x, crit_bit)) {
      break;
    }
    ++crit_bit;
  }
  return crit_bit;
}

static inline size_t _critbit_get_index(const uintptr_t v, const uint8_t bit)
{
  assert(v != 0);

  return (_critbit_is_set(v, bit) ? 1 : 0);
}

static inline uintptr_t _critbit_add_tag(const struct _critbit_node *const node)
{
  const uintptr_t v = (uintptr_t)node;
  assert(!_critbit_has_tag(v));

  return v + 1;
}

static inline struct _critbit_node *_critbit_remove_tag(const uintptr_t v)
{
  assert(_critbit_has_tag(v));

  return (struct _critbit_node *)(v - 1);
}

static inline uintptr_t _critbit_create_node(const uintptr_t v1,
    const uintptr_t v2, const uint8_t crit_bit)
{
  assert(v1 != v2);
  assert(!_critbit_has_tag(v1));

  struct _critbit_node *const node = malloc(sizeof(*node));
  const size_t index = _critbit_get_index(v1, crit_bit);
  if (!_critbit_has_tag(v2)) {
    assert(_critbit_get_index(v2, crit_bit) == (index ^ 1));
  }
  node->next[index] = v1;
  node->next[index ^ 1] = v2;
  node->crit_bit = crit_bit;
  return _critbit_add_tag(node);
}

static inline uintptr_t _critbit_delete_node(const uintptr_t tagged_node,
    const uintptr_t v)
{
  struct _critbit_node *const node = _critbit_remove_tag(tagged_node);
  const size_t index = _critbit_get_index(v, node->crit_bit);
  uintptr_t remaining_child = node->next[index ^ 1];
  free(node);
  return remaining_child;
}

static inline uintptr_t *_critbit_get_leaf_ptr(const struct critbit *const cb,
    const uintptr_t v)
{
  const uintptr_t *next = &cb->root;
  while (_critbit_has_tag(*next)) {
    const struct _critbit_node *const node = _critbit_remove_tag(*next);
    const size_t index = _critbit_get_index(v, node->crit_bit);
    next = &node->next[index];
  }
  return (uintptr_t *) next;
}

static inline uintptr_t *_critbit_get_node_ptr(const struct critbit *const cb,
    const uintptr_t v, const uint8_t crit_bit)
{
  const uintptr_t *next = &cb->root;
  while (_critbit_has_tag(*next)) {
    const struct _critbit_node *const node = _critbit_remove_tag(*next);
    assert(node->crit_bit != crit_bit);
    if (node->crit_bit > crit_bit) {
      break;
    }
    const size_t index = _critbit_get_index(v, node->crit_bit);
    next = &node->next[index];
  }
  return (uintptr_t *) next;
}

static inline void remove_all_nodes(const uintptr_t v)
{
  if (_critbit_has_tag(v)) {
    struct _critbit_node *const node = _critbit_remove_tag(v);
    remove_all_nodes(node->next[0]);
    remove_all_nodes(node->next[1]);
    free(node);
  }
}

static inline struct critbit *critbit_create()
{
  struct critbit *const cb = malloc(sizeof(*cb));
  cb->root = 0;
  return cb;
}

static inline void critbit_delete(struct critbit *const cb)
{
  if (cb->root != 0) {
    remove_all_nodes(cb->root);
  }
  free(cb);
}

static inline int critbit_add(struct critbit *const cb, const uintptr_t v)
{
  assert(!_critbit_has_tag(v));

  if (cb->root == 0) {
    cb->root = v;
    return 1;
  }

  uintptr_t *next = _critbit_get_leaf_ptr(cb, v);
  if (*next == v) {
    return 0;
  }
  const uint8_t crit_bit = _critbit_get_crit_bit(*next, v);
  next = _critbit_get_node_ptr(cb, v, crit_bit);
  *next = _critbit_create_node(v, *next, crit_bit);
  return 1;
}

static inline int critbit_remove(struct critbit *const cb, const uintptr_t v)
{
  assert(!_critbit_has_tag(v));

  if (cb->root == 0) {
    return 0;
  }

  if (!_critbit_has_tag(cb->root)) {
    if (cb->root == v) {
      cb->root = 0;
      return 1;
    }
    return 0;
  }

  uintptr_t *prev = &cb->root;
  struct _critbit_node *node = _critbit_remove_tag(*prev);
  size_t index = _critbit_get_index(v, node->crit_bit);
  uintptr_t *next = &node->next[index];
  while (_critbit_has_tag(*next)) {
    prev = next;
    node = _critbit_remove_tag(*next);
    index = _critbit_get_index(v, node->crit_bit);
    next = &node->next[index];
  }
  if (*next != v) {
    return 0;
  }
  *prev = _critbit_delete_node(*prev, v);
  return 1;
}

static inline int critbit_contains(const struct critbit *const cb,
    const uintptr_t v)
{
  assert(!_critbit_has_tag(v));

  if (cb->root == 0) {
    return 0;
  }

  uintptr_t *next = _critbit_get_leaf_ptr(cb, v);
  return (*next == v);
}

static inline size_t _critbit_put_node(uintptr_t *const a, size_t offset,
    const uintptr_t v)
{
  if (_critbit_has_tag(v)) {
    const struct _critbit_node *const node = _critbit_remove_tag(v);
    offset = _critbit_put_node(a, offset, node->next[0]);
    offset = _critbit_put_node(a, offset, node->next[1]);
  }
  else {
    a[offset] = v;
    ++offset;
  }
  return offset;
}

static inline size_t critbit_sort(uintptr_t *const a, const size_t n)
{
  struct critbit *const cb = critbit_create();

  size_t m = n;
  for (size_t i = 0; i < n; ++i) {
    if (!critbit_add(cb, a[i])) {
      --m;
    }
  }

  size_t offset = _critbit_put_node(a, 0, cb->root);
  assert(offset == m);
  (void)offset;

  critbit_delete(cb);
  return m;
}

#endif
