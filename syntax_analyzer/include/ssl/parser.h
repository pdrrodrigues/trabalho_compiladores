#ifndef SSL_PARSER_H
#define SSL_PARSER_H

#include <stdbool.h>
#include <stdio.h>

#include "ssl/diagnostic.h"
#include "ssl/lexer.h"

typedef struct SslNode {
  unsigned production; /* Zero for a terminal. */
  SslTokenKind token;
  int32_t secondary;
  size_t line;
  size_t column;
  size_t count;
  struct SslNode *children[9];
  void *binding; /* Set by scope analysis; owned by the analysis context. */
} SslNode;

bool ssl_parser_validate_tables(SslDiagnostic *diagnostic);
bool ssl_parser_parse(SslLexer *lexer, bool dump_tokens, FILE *dump_stream,
                      SslDiagnostic *diagnostic);
bool ssl_parser_parse_tree(SslLexer *lexer, bool dump_tokens, FILE *dump_stream,
                           SslNode **tree, SslDiagnostic *diagnostic);
void ssl_node_destroy(SslNode *node);

#endif
