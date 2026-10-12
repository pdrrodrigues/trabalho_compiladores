#include "ssl/parser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { PARSER_ACCEPT = INT16_MAX };
#include "parser_tables.inc"

_Static_assert(TOK_KIND_COUNT == 49, "token enum and parser columns differ");
_Static_assert(PARSER_STATE_COUNT < INT16_MAX, "parser states exceed int16_t");
_Static_assert(PARSER_PRODUCTION_COUNT < INT16_MAX,
               "productions exceed int16_t");

typedef struct {
  uint16_t state;
  SslNode *node;
} ParserEntry;

typedef struct {
  ParserEntry *entries;
  size_t count;
  size_t capacity;
} ParserStack;

void ssl_node_destroy(SslNode *node) {
  if (node == NULL)
    return;
  for (size_t i = 0; i < node->count; ++i)
    ssl_node_destroy(node->children[i]);
  free(node);
}

static bool stack_init(ParserStack *stack, SslLexer *lexer,
                       SslDiagnostic *diagnostic) {
  *stack = (ParserStack){.capacity = 64};
  stack->entries = calloc(stack->capacity, sizeof *stack->entries);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (stack->entries == NULL) {
    ssl_diagnostic_set(diagnostic, lexer->source_name, 1, 1,
                       "out of memory while creating parser stack");
    return false;
  }
  /* LCOV_EXCL_STOP */
  stack->entries[0] = (ParserEntry){.state = 0};
  stack->count = 1;
  return true;
}

static void stack_destroy(ParserStack *stack) {
  for (size_t i = 0; i < stack->count; ++i)
    ssl_node_destroy(stack->entries[i].node);
  free(stack->entries);
  *stack = (ParserStack){0};
}

static bool stack_push(ParserStack *stack, uint16_t state, SslNode *node,
                       SslLexer *lexer, const SslToken *lookahead,
                       SslDiagnostic *diagnostic) {
  if (stack->count == stack->capacity) {
    /* LCOV_EXCL_START: allocation exhaustion requires fault injection. */
    if (stack->capacity > SIZE_MAX / 2 / sizeof *stack->entries) {
      ssl_diagnostic_set(diagnostic, lexer->source_name, lookahead->line,
                         lookahead->column,
                         "out of memory while growing parser stack");
      return false;
    }
    /* LCOV_EXCL_STOP */
    size_t new_capacity = stack->capacity * 2;
    ParserEntry *grown = calloc(new_capacity, sizeof *grown);
    /* LCOV_EXCL_START: allocator failures require fault injection. */
    if (grown == NULL) {
      ssl_diagnostic_set(diagnostic, lexer->source_name, lookahead->line,
                         lookahead->column,
                         "out of memory while growing parser stack");
      return false;
    }
    /* LCOV_EXCL_STOP */
    memcpy(grown, stack->entries, stack->count * sizeof *grown);
    free(stack->entries);
    stack->entries = grown;
    stack->capacity = new_capacity;
  }
  stack->entries[stack->count++] = (ParserEntry){.state = state, .node = node};
  return true;
}

bool ssl_parser_validate_tables(SslDiagnostic *diagnostic) {
  for (size_t state = 0; state < PARSER_STATE_COUNT; ++state) {
    for (size_t symbol = 0; symbol < PARSER_SYMBOL_COUNT; ++symbol) {
      int action = parser_action[state][symbol];
      if (action == PARSER_ACCEPT)
        continue;
      /* LCOV_EXCL_START: the checked-in const table cannot take these paths. */
      if (action > 0 && action >= PARSER_STATE_COUNT) {
        ssl_diagnostic_set(diagnostic, "<parser-table>", state, symbol,
                           "transition targets invalid state %d", action);
        return false;
      }
      /* LCOV_EXCL_STOP */
      if (action < 0 && (size_t)(-action) >= PARSER_PRODUCTION_COUNT) {
        ssl_diagnostic_set(diagnostic, "<parser-table>", state, symbol,
                           "reduction targets invalid production %d", -action);
        return false;
      }
    }
  }
  for (size_t production = 1; production < PARSER_PRODUCTION_COUNT;
       ++production) {
    if (parser_production[production][1] < TOK_KIND_COUNT ||
        parser_production[production][1] >= PARSER_SYMBOL_COUNT) {
      /* LCOV_EXCL_START: the checked-in const table cannot take this path. */
      ssl_diagnostic_set(diagnostic, "<parser-table>", production, 0,
                         "production has invalid left-hand symbol");
      return false;
      /* LCOV_EXCL_STOP */
    }
  }
  return true;
}

static void dump_escaped(FILE *stream, const char *text) {
  fputc('"', stream);
  for (const unsigned char *p = (const unsigned char *)text; *p != 0; ++p) {
    switch (*p) {
    case '\n':
      fputs("\\n", stream);
      break;
    case '\r':
      fputs("\\r", stream);
      break;
    case '\t':
      fputs("\\t", stream);
      break;
    case '\\':
      fputs("\\\\", stream);
      break;
    case '"':
      fputs("\\\"", stream);
      break;
    default:
      if (*p >= 32 && *p <= 126)
        fputc(*p, stream);
      else
        fprintf(stream, "\\x%02X", *p);
    }
  }
  fputc('"', stream);
}

static void dump_token(FILE *stream, const SslToken *token) {
  fprintf(stream, "%zu:%zu\tprimary=%d(%s)\tsecondary=%d\tlexeme=", token->line,
          token->column, token->primary, ssl_token_kind_name(token->primary),
          token->secondary);
  dump_escaped(stream, token->lexeme);
  fputc('\n', stream);
}

static bool next_token(SslLexer *lexer, SslToken *token, bool dump_tokens,
                       FILE *dump_stream, SslDiagnostic *diagnostic) {
  if (!ssl_lexer_next(lexer, token, diagnostic))
    return false;
  if (dump_tokens)
    dump_token(dump_stream, token);
  return true;
}

static void syntax_error(SslLexer *lexer, size_t state, const SslToken *token,
                         SslDiagnostic *diagnostic) {
  char expected[512] = "";
  size_t used = 0;
  for (size_t terminal = 0; terminal < TOK_KIND_COUNT; ++terminal) {
    if (parser_action[state][terminal] == 0)
      continue;
    const char *name = ssl_token_kind_name((SslTokenKind)terminal);
    int written = snprintf(expected + used, sizeof expected - used, "%s%s",
                           used == 0 ? "" : ", ", name);
    /* LCOV_EXCL_START: no generated state has a set large enough to truncate.
     */
    if (written < 0 || (size_t)written >= sizeof expected - used) {
      strcpy(expected + sizeof expected - 5, "...");
      break;
    }
    /* LCOV_EXCL_STOP */
    used += (size_t)written;
  }
  ssl_diagnostic_set(diagnostic, lexer->source_name, token->line, token->column,
                     "syntax error: found %s `%s`; expected %s",
                     ssl_token_kind_name(token->primary), token->lexeme,
                     expected[0] != '\0' ? expected
                                         : "no terminal (invalid table state)");
}

static SslNode *new_node(unsigned production, const SslToken *token,
                         SslDiagnostic *diagnostic, const char *source) {
  SslNode *node = calloc(1, sizeof *node);
  if (node == NULL) {
    ssl_diagnostic_set(diagnostic, source, token->line, token->column,
                       "out of memory while building parse tree");
    return NULL;
  }
  node->production = production;
  node->token = production == 0 ? token->primary : TOK_INVALID;
  node->secondary = production == 0 ? token->secondary : SSL_NO_SECONDARY_TOKEN;
  node->line = token->line;
  node->column = token->column;
  return node;
}

static bool parse_internal(SslLexer *lexer, bool dump_tokens, FILE *dump_stream,
                           SslNode **tree, SslDiagnostic *diagnostic) {
  if (tree != NULL)
    *tree = NULL;
  ParserStack stack;
  if (!stack_init(&stack, lexer, diagnostic)) /* LCOV_EXCL_BR_LINE */
    return false;

  SslToken lookahead = {0};
  if (!next_token(lexer, &lookahead, dump_tokens, dump_stream, diagnostic)) {
    stack_destroy(&stack);
    return false;
  }
  for (;;) {
    size_t state = stack.entries[stack.count - 1].state;
    int action = parser_action[state][lookahead.primary];
    if (action == PARSER_ACCEPT) {
      if (tree != NULL) {
        *tree = stack.entries[stack.count - 1].node;
        stack.entries[stack.count - 1].node = NULL;
      }
      ssl_token_dispose(&lookahead);
      stack_destroy(&stack);
      return true;
    }
    if (action > 0) {
      SslNode *leaf = NULL;
      if (tree != NULL) {
        leaf = new_node(0, &lookahead, diagnostic, lexer->source_name);
        if (leaf == NULL) {
          ssl_token_dispose(&lookahead);
          stack_destroy(&stack);
          return false;
        }
      }
      if (!stack_push(&stack, (uint16_t)action, leaf, lexer,
                      &lookahead, /* LCOV_EXCL_BR_LINE */
                      diagnostic)) {
        ssl_node_destroy(leaf);
        ssl_token_dispose(&lookahead);
        stack_destroy(&stack);
        return false;
      }
      ssl_token_dispose(&lookahead);
      if (!next_token(lexer, &lookahead, dump_tokens, dump_stream,
                      diagnostic)) {
        stack_destroy(&stack);
        return false;
      }
      continue;
    }
    if (action < 0) {
      size_t production = (size_t)(-action);
      size_t length = parser_production[production][0];
      size_t left = parser_production[production][1];
      /* LCOV_EXCL_START: validated tables cannot underflow or miss a GOTO. */
      if (length == 0 || length > 9 || length >= stack.count) {
        ssl_diagnostic_set(diagnostic, "<parser-table>", state, 0,
                           "reduction %zu underflows parser stack", production);
        ssl_token_dispose(&lookahead);
        stack_destroy(&stack);
        return false;
      }
      /* LCOV_EXCL_STOP */
      SslNode *branch = NULL;
      if (tree != NULL) {
        branch = new_node((unsigned)production, &lookahead, diagnostic,
                          lexer->source_name);
        if (branch == NULL) {
          ssl_token_dispose(&lookahead);
          stack_destroy(&stack);
          return false;
        }
        branch->count = length;
        for (size_t i = 0; i < length; ++i) {
          ParserEntry *entry = &stack.entries[stack.count - length + i];
          /* LCOV_EXCL_START: a tree parse always pushes an owned node. */
          if (entry->node == NULL) {
            ssl_diagnostic_set(diagnostic, "<parser-table>", state, 0,
                               "reduction %zu has missing tree node",
                               production);
            ssl_node_destroy(branch);
            ssl_token_dispose(&lookahead);
            stack_destroy(&stack);
            return false;
          }
          /* LCOV_EXCL_STOP */
          branch->children[i] = entry->node;
          entry->node = NULL;
        }
        branch->line = branch->children[0]->line;
        branch->column = branch->children[0]->column;
      }
      stack.count -= length;
      size_t previous = stack.entries[stack.count - 1].state;
      /* LCOV_EXCL_START: validated tables cannot produce invalid symbols. */
      if (previous >= PARSER_STATE_COUNT || left >= PARSER_SYMBOL_COUNT) {
        ssl_diagnostic_set(diagnostic, "<parser-table>", state, left,
                           "reduction %zu has invalid symbol", production);
        ssl_node_destroy(branch);
        ssl_token_dispose(&lookahead);
        stack_destroy(&stack);
        return false;
      }
      /* LCOV_EXCL_STOP */
      int target = parser_action[previous][left];
      /* LCOV_EXCL_START: validated tables cannot underflow or miss a GOTO. */
      if (target <= 0 || target == PARSER_ACCEPT) {
        ssl_diagnostic_set(diagnostic, "<parser-table>", state, left,
                           "reduction %zu has invalid GOTO", production);
        ssl_node_destroy(branch);
        ssl_token_dispose(&lookahead);
        stack_destroy(&stack);
        return false;
      }
      /* LCOV_EXCL_STOP */
      /* Every reduction pops at least one state, so the GOTO always fits. */
      stack.entries[stack.count++] =
          (ParserEntry){.state = (uint16_t)target, .node = branch};
      continue;
    }
    syntax_error(lexer, state, &lookahead, diagnostic);
    ssl_token_dispose(&lookahead);
    stack_destroy(&stack);
    return false;
  }
}
bool ssl_parser_parse(SslLexer *lexer, bool dump_tokens, FILE *dump_stream,
                      SslDiagnostic *diagnostic) {
  return parse_internal(lexer, dump_tokens, dump_stream, NULL, diagnostic);
}

bool ssl_parser_parse_tree(SslLexer *lexer, bool dump_tokens, FILE *dump_stream,
                           SslNode **tree, SslDiagnostic *diagnostic) {
  return parse_internal(lexer, dump_tokens, dump_stream, tree, diagnostic);
}
