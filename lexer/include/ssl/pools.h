#ifndef SSL_POOLS_H
#define SSL_POOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  char **names;
  size_t count;
  size_t capacity;
  int32_t *slots;
  size_t slot_capacity;
} SslNameTable;

typedef enum {
  SSL_CONST_CHARACTER,
  SSL_CONST_INTEGER,
  SSL_CONST_STRING
} SslConstantKind;

typedef struct {
  SslConstantKind kind;
  union {
    unsigned char character;
    int64_t integer;
    char *string;
  } value;
} SslConstant;

typedef struct {
  SslConstant *items;
  size_t count;
  size_t capacity;
} SslConstantPool;

void ssl_name_table_init(SslNameTable *table);
void ssl_name_table_destroy(SslNameTable *table);
int32_t ssl_name_table_intern(SslNameTable *table, const char *name,
                              bool *inserted);
const char *ssl_name_table_get(const SslNameTable *table, int32_t id);

void ssl_constant_pool_init(SslConstantPool *pool);
void ssl_constant_pool_destroy(SslConstantPool *pool);
int32_t ssl_constant_pool_add_character(SslConstantPool *pool,
                                        unsigned char value);
int32_t ssl_constant_pool_add_integer(SslConstantPool *pool, int64_t value);
int32_t ssl_constant_pool_add_string(SslConstantPool *pool, const char *value);
const SslConstant *ssl_constant_pool_get(const SslConstantPool *pool,
                                         int32_t id);

#endif
