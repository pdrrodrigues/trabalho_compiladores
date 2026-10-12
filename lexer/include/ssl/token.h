#ifndef SSL_TOKEN_H
#define SSL_TOKEN_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  TOK_EOF = 0,
  TOK_ARRAY,
  TOK_BOOLEAN,
  TOK_BREAK,
  TOK_CHAR,
  TOK_CONTINUE,
  TOK_DO,
  TOK_ELSE,
  TOK_FALSE,
  TOK_FUNCTION,
  TOK_IF,
  TOK_INTEGER,
  TOK_OF,
  TOK_STRING,
  TOK_STRUCT,
  TOK_TRUE,
  TOK_TYPE,
  TOK_VAR,
  TOK_WHILE,
  TOK_COLON,
  TOK_SEMI_COLON,
  TOK_COMMA,
  TOK_EQUALS,
  TOK_LEFT_SQUARE,
  TOK_RIGHT_SQUARE,
  TOK_LEFT_BRACE,
  TOK_RIGHT_BRACE,
  TOK_LEFT_PAREN,
  TOK_RIGHT_PAREN,
  TOK_AND,
  TOK_OR,
  TOK_LESS_THAN,
  TOK_GREATER_THAN,
  TOK_LESS_OR_EQUAL,
  TOK_GREATER_OR_EQUAL,
  TOK_NOT_EQUAL,
  TOK_EQUAL_EQUAL,
  TOK_PLUS,
  TOK_PLUS_PLUS,
  TOK_MINUS,
  TOK_MINUS_MINUS,
  TOK_TIMES,
  TOK_DIVIDE,
  TOK_DOT,
  TOK_NOT,
  TOK_CHARACTER,
  TOK_NUMERAL,
  TOK_STRING_VALUE,
  TOK_IDENTIFIER,
  TOK_KIND_COUNT,
  TOK_INVALID = TOK_KIND_COUNT
} SslTokenKind;

enum { SSL_NO_SECONDARY_TOKEN = -1 };

typedef struct {
  SslTokenKind primary;
  int32_t secondary;
  size_t line;
  size_t column;
  size_t end_line;
  size_t end_column;
  char *lexeme;
} SslToken;

const char *ssl_token_kind_name(SslTokenKind kind);
SslTokenKind ssl_keyword_kind(const char *text);
void ssl_token_dispose(SslToken *token);

#endif
