#include "ssl/pools.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *text) {
  size_t length = strlen(text) + 1;
  char *copy = malloc(length);
  if (copy != NULL) { /* LCOV_EXCL_BR_LINE: allocator failure. */
    memcpy(copy, text, length);
  }
  return copy;
}

static uint64_t hash_name(const char *name) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (const unsigned char *p = (const unsigned char *)name; *p != 0; ++p) {
    hash ^= *p;
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

void ssl_name_table_init(SslNameTable *table) { *table = (SslNameTable){0}; }

void ssl_name_table_destroy(SslNameTable *table) {
  if (table == NULL)
    return;
  for (size_t i = 0; i < table->count; ++i)
    free(table->names[i]);
  free(table->names);
  free(table->slots);
  *table = (SslNameTable){0};
}

static bool resize_slots(SslNameTable *table, size_t capacity) {
  int32_t *slots = calloc(capacity, sizeof *slots);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (slots == NULL)
    return false;
  /* LCOV_EXCL_STOP */
  for (size_t id = 0; id < table->count; ++id) {
    size_t slot = (size_t)(hash_name(table->names[id]) % capacity);
    while (slots[slot] != 0)
      slot = (slot + 1) % capacity;
    slots[slot] = (int32_t)id + 1;
  }
  free(table->slots);
  table->slots = slots;
  table->slot_capacity = capacity;
  return true;
}

int32_t ssl_name_table_intern(SslNameTable *table, const char *name,
                              bool *inserted) {
  if (inserted != NULL)
    *inserted = false;
  if (table->slot_capacity == 0 && /* LCOV_EXCL_BR_LINE */
      !resize_slots(table, 16))
    return -1; /* LCOV_EXCL_LINE: allocator failure. */
  if ((table->count + 1) * 10 >=
          table->slot_capacity * 7 && /* LCOV_EXCL_BR_LINE */
      !resize_slots(table, table->slot_capacity * 2))
    return -1; /* LCOV_EXCL_LINE: allocator failure. */
  size_t slot = (size_t)(hash_name(name) % table->slot_capacity);
  while (table->slots[slot] != 0) {
    int32_t id = table->slots[slot] - 1;
    if (strcmp(table->names[id], name) == 0)
      return id;
    slot = (slot + 1) % table->slot_capacity;
  }
  if (table->count == (size_t)INT32_MAX) /* LCOV_EXCL_BR_LINE */
    return -1; /* LCOV_EXCL_LINE: impractical address-space limit. */
  if (table->count == table->capacity) {
    size_t capacity = table->capacity == 0 ? 8 : table->capacity * 2;
    char **names = realloc(table->names, capacity * sizeof *names);
    /* LCOV_EXCL_START: allocator failures require fault injection. */
    if (names == NULL)
      return -1;
    /* LCOV_EXCL_STOP */
    table->names = names;
    table->capacity = capacity;
  }
  char *copy = copy_string(name);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (copy == NULL)
    return -1;
  /* LCOV_EXCL_STOP */
  int32_t id = (int32_t)table->count;
  table->names[table->count++] = copy;
  table->slots[slot] = id + 1;
  if (inserted != NULL)
    *inserted = true;
  return id;
}

const char *ssl_name_table_get(const SslNameTable *table, int32_t id) {
  return id >= 0 && (size_t)id < table->count ? table->names[id] : NULL;
}

void ssl_constant_pool_init(SslConstantPool *pool) {
  *pool = (SslConstantPool){0};
}

void ssl_constant_pool_destroy(SslConstantPool *pool) {
  if (pool == NULL)
    return;
  for (size_t i = 0; i < pool->count; ++i) {
    if (pool->items[i].kind == SSL_CONST_STRING)
      free(pool->items[i].value.string);
  }
  free(pool->items);
  *pool = (SslConstantPool){0};
}

static SslConstant *append_constant(SslConstantPool *pool) {
  if (pool->count == (size_t)INT32_MAX) /* LCOV_EXCL_BR_LINE */
    return NULL;
  if (pool->count == pool->capacity) {
    size_t capacity = pool->capacity == 0 ? 8 : pool->capacity * 2;
    SslConstant *items = realloc(pool->items, capacity * sizeof *items);
    /* LCOV_EXCL_START: allocator failures require fault injection. */
    if (items == NULL)
      return NULL;
    /* LCOV_EXCL_STOP */
    pool->items = items;
    pool->capacity = capacity;
  }
  return &pool->items[pool->count];
}

int32_t ssl_constant_pool_add_character(SslConstantPool *pool,
                                        unsigned char value) {
  SslConstant *item = append_constant(pool);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (item == NULL)
    return -1;
  /* LCOV_EXCL_STOP */
  item->kind = SSL_CONST_CHARACTER;
  item->value.character = value;
  return (int32_t)pool->count++;
}

int32_t ssl_constant_pool_add_integer(SslConstantPool *pool, int64_t value) {
  SslConstant *item = append_constant(pool);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (item == NULL)
    return -1;
  /* LCOV_EXCL_STOP */
  item->kind = SSL_CONST_INTEGER;
  item->value.integer = value;
  return (int32_t)pool->count++;
}

int32_t ssl_constant_pool_add_string(SslConstantPool *pool, const char *value) {
  SslConstant *item = append_constant(pool);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (item == NULL)
    return -1;
  /* LCOV_EXCL_STOP */
  char *copy = copy_string(value);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (copy == NULL)
    return -1;
  /* LCOV_EXCL_STOP */
  item->kind = SSL_CONST_STRING;
  item->value.string = copy;
  return (int32_t)pool->count++;
}

const SslConstant *ssl_constant_pool_get(const SslConstantPool *pool,
                                         int32_t id) {
  return id >= 0 && (size_t)id < pool->count ? &pool->items[id] : NULL;
}
