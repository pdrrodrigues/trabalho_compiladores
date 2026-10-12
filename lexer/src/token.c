#include "ssl/token.h"

#include <stdlib.h>
#include <string.h>

static const char *const token_names[TOK_KIND_COUNT] = {"EOF",
                                                        "ARRAY",
                                                        "BOOLEAN",
                                                        "BREAK",
                                                        "CHAR",
                                                        "CONTINUE",
                                                        "DO",
                                                        "ELSE",
                                                        "FALSE",
                                                        "FUNCTION",
                                                        "IF",
                                                        "INTEGER",
                                                        "OF",
                                                        "STRING",
                                                        "STRUCT",
                                                        "TRUE",
                                                        "TYPE",
                                                        "VAR",
                                                        "WHILE",
                                                        "COLON",
                                                        "SEMI_COLON",
                                                        "COMMA",
                                                        "EQUALS",
                                                        "LEFT_SQUARE",
                                                        "RIGHT_SQUARE",
                                                        "LEFT_BRACE",
                                                        "RIGHT_BRACE",
                                                        "LEFT_PAREN",
                                                        "RIGHT_PAREN",
                                                        "AND",
                                                        "OR",
                                                        "LESS_THAN",
                                                        "GREATER_THAN",
                                                        "LESS_OR_EQUAL",
                                                        "GREATER_OR_EQUAL",
                                                        "NOT_EQUAL",
                                                        "EQUAL_EQUAL",
                                                        "PLUS",
                                                        "PLUS_PLUS",
                                                        "MINUS",
                                                        "MINUS_MINUS",
                                                        "TIMES",
                                                        "DIVIDE",
                                                        "DOT",
                                                        "NOT",
                                                        "CHARACTER",
                                                        "NUMERAL",
                                                        "STRING_VALUE",
                                                        "IDENTIFIER"};

typedef struct {
  const char *word;
  SslTokenKind kind;
} Keyword;
static const Keyword keywords[] = {
    {"array", TOK_ARRAY},       {"boolean", TOK_BOOLEAN},
    {"break", TOK_BREAK},       {"char", TOK_CHAR},
    {"continue", TOK_CONTINUE}, {"do", TOK_DO},
    {"else", TOK_ELSE},         {"false", TOK_FALSE},
    {"function", TOK_FUNCTION}, {"if", TOK_IF},
    {"integer", TOK_INTEGER},   {"of", TOK_OF},
    {"string", TOK_STRING},     {"struct", TOK_STRUCT},
    {"true", TOK_TRUE},         {"type", TOK_TYPE},
    {"var", TOK_VAR},           {"while", TOK_WHILE}};

const char *ssl_token_kind_name(SslTokenKind kind) {
  if ((unsigned)kind < TOK_KIND_COUNT) {
    return token_names[kind];
  }
  return "INVALID";
}

SslTokenKind ssl_keyword_kind(const char *text) {
  size_t low = 0;
  size_t high = sizeof keywords / sizeof keywords[0];
  while (low < high) {
    size_t middle = low + (high - low) / 2;
    int comparison = strcmp(text, keywords[middle].word);
    if (comparison == 0) {
      return keywords[middle].kind;
    }
    if (comparison < 0) {
      high = middle;
    } else {
      low = middle + 1;
    }
  }
  return TOK_IDENTIFIER;
}

void ssl_token_dispose(SslToken *token) {
  if (token != NULL) {
    free(token->lexeme);
    token->lexeme = NULL;
  }
}
