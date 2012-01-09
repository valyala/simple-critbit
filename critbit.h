#ifndef CRITBIT_H
#define CRITBIT_H

/*
 * Simple crit-bit implementation for C99.
 * Supports only non-zero even keys of type uintptr_t.
 *
 * Author: Aliaksandr Valialkin <valyala@gmail.com>
 */

/*******************************************************************************
 * Interface.
 ******************************************************************************/
#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint*_t */

/* Opaque crit-bit structure. */
struct critbit;

/* Memory allocator for critbit nodes. */
struct critbit_node_allocator
{
  /*
   * Must return a pointer to memory suitable for storing a crit-bit node
   * of size critbit_node_size().
   */
  void *(*alloc_node)(void *ctx);

  /* Must release memory, which was allocated with alloc_node. */
  void (*free_node)(void *ctx, void *node);

  /* Arbitrary context, which is passed to alloc_node() and free_node(). */
  void *ctx;
};

/* Item visitor for critbit_foreach(). */
struct critbit_visitor
{
  /* The callback is called for each crit-bit item. */
  void (*callback)(void *ctx, uintptr_t v);

  /* Arbitrary context, which is passed to the callback. */
  void *ctx;
};

/*
 * Returns a size of a crit-bit node. The result of this function must be used
 * by critbit_node_allocator.
 */
static inline size_t critbit_node_size(void);

/*
 * Creates a crit-bit. Uses node_allocator for dynamic memory allocation
 * for crit-bit nodes.
 */
static inline struct critbit *critbit_create(
    const struct critbit_node_allocator *node_allocator);

/*
 * Deletes the given crit-bit
 */
static inline void critbit_delete(struct critbit *cb);

/*
 * Adds v to crit-bit. Returs 1 on success, 0 if v already exists
 * in the crit-bit.
 * v must be non-zero even integer.
 */
static inline int critbit_add(struct critbit *cb, uintptr_t v);

/*
 * Removes v from crit-bit. Returns 1 on success, 0 if v doesn't exist
 * in the crit-bit.
 * v must be non-zero even integer.
 */
static inline int critbit_remove(struct critbit *cb, uintptr_t v);

/*
 * Returns 1 if crit-bit contains v, otherwise returns 0.
 * v must be non-zero even integer.
 */
static inline int critbit_contains(const struct critbit *cb, uintptr_t v);

/*
 * Calls visitor for each crit-bit item in ascending order.
 * Do not modify crit-bit in visitor!
 */
static inline void critbit_foreach(const struct critbit *const cb,
    const struct critbit_visitor *visitor);


/*******************************************************************************
 * Implementation.
 ******************************************************************************/
#include <assert.h>
#include <limits.h>  /* for CHAR_BIT */
#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint*_t */
#include <stdlib.h>  /* for malloc/free */

struct critbit
{
  uintptr_t root;
  const struct critbit_node_allocator *node_allocator;
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

static inline uintptr_t _critbit_create_node(const struct critbit *const cb,
    const uintptr_t v1, const uintptr_t v2, const uint8_t crit_bit)
{
  assert(v1 != v2);
  assert(!_critbit_has_tag(v1));

  struct _critbit_node *const node = cb->node_allocator->alloc_node(
      cb->node_allocator->ctx);
  const size_t index = _critbit_get_index(v1, crit_bit);
  if (!_critbit_has_tag(v2)) {
    assert(_critbit_get_index(v2, crit_bit) == (index ^ 1));
  }
  node->next[index] = v1;
  node->next[index ^ 1] = v2;
  node->crit_bit = crit_bit;
  return _critbit_add_tag(node);
}

static inline uintptr_t _critbit_delete_node(const struct critbit *const cb,
    const uintptr_t tagged_node, const uintptr_t v)
{
  struct _critbit_node *const node = _critbit_remove_tag(tagged_node);
  const size_t index = _critbit_get_index(v, node->crit_bit);
  uintptr_t remaining_child = node->next[index ^ 1];
  cb->node_allocator->free_node(cb->node_allocator->ctx, node);
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

static inline void _critbit_remove_all_nodes(const struct critbit *const cb,
    const uintptr_t v)
{
  if (_critbit_has_tag(v)) {
    struct _critbit_node *const node = _critbit_remove_tag(v);
    _critbit_remove_all_nodes(cb, node->next[0]);
    _critbit_remove_all_nodes(cb, node->next[1]);
    cb->node_allocator->free_node(cb->node_allocator->ctx, node);
  }
}

static inline size_t critbit_node_size(void)
{
  return sizeof(struct _critbit_node);
}

static inline struct critbit *critbit_create(
    const struct critbit_node_allocator *const node_allocator)
{
  struct critbit *const cb = malloc(sizeof(*cb));
  cb->root = 0;
  cb->node_allocator = node_allocator;
  return cb;
}

static inline void critbit_delete(struct critbit *const cb)
{
  if (cb->root != 0) {
    _critbit_remove_all_nodes(cb, cb->root);
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
  *next = _critbit_create_node(cb, v, *next, crit_bit);
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
  *prev = _critbit_delete_node(cb, *prev, v);
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

static inline void _critbit_visit(
    const struct critbit_visitor *const visitor, const uintptr_t v)
{
  if (_critbit_has_tag(v)) {
    const struct _critbit_node *const node = _critbit_remove_tag(v);
    const uintptr_t left = node->next[0];
    const uintptr_t right = node->next[1];
    _critbit_visit(visitor, left);
    _critbit_visit(visitor, right);
  }
  else {
    visitor->callback(visitor->ctx, v);
  }
}

static inline void critbit_foreach(const struct critbit *const cb,
    const struct critbit_visitor *const visitor)
{
  _critbit_visit(visitor, cb->root);
}

#endif
