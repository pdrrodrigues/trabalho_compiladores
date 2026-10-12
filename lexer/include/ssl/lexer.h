#ifndef SSL_LEXER_H
#define SSL_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "ssl/diagnostic.h"
#include "ssl/pools.h"
#include "ssl/token.h"

typedef struct {
  unsigned char *input;
  size_t length;
  size_t offset;
  size_t line;
  size_t column;
  char *source_name;
  SslNameTable *names;
  SslConstantPool *constants;
} SslLexer;

bool ssl_lexer_init_file(SslLexer *lexer, FILE *input, const char *source_name,
                         SslNameTable *names, SslConstantPool *constants,
                         SslDiagnostic *diagnostic);
bool ssl_lexer_init_text(SslLexer *lexer, const char *text,
                         const char *source_name, SslNameTable *names,
                         SslConstantPool *constants, SslDiagnostic *diagnostic);
void ssl_lexer_destroy(SslLexer *lexer);
bool ssl_lexer_next(SslLexer *lexer, SslToken *token,
                    SslDiagnostic *diagnostic);

#endif
