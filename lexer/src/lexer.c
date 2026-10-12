#include "ssl/lexer.h"

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static char *copy_bytes(const unsigned char *bytes, size_t length) {
  /* LCOV_EXCL_START: callers cannot construct SIZE_MAX-sized source slices. */
  if (length == SIZE_MAX)
    return NULL;
  /* LCOV_EXCL_STOP */
  char *copy = malloc(length + 1);
  if (copy != NULL) { /* LCOV_EXCL_BR_LINE: allocator failure. */
    memcpy(copy, bytes, length);
    copy[length] = '\0';
  }
  return copy;
}

static bool initialize(SslLexer *lexer, unsigned char *input, size_t length,
                       const char *source_name, SslNameTable *names,
                       SslConstantPool *constants, SslDiagnostic *diagnostic) {
  char *name =
      copy_bytes((const unsigned char *)source_name, strlen(source_name));
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (name == NULL) {
    free(input);
    ssl_diagnostic_set(diagnostic, source_name, 1, 1,
                       "out of memory while initializing lexer");
    return false;
  }
  /* LCOV_EXCL_STOP */
  *lexer = (SslLexer){.input = input,
                      .length = length,
                      .line = 1,
                      .column = 1,
                      .source_name = name,
                      .names = names,
                      .constants = constants};
  return true;
}

bool ssl_lexer_init_text(SslLexer *lexer, const char *text,
                         const char *source_name, SslNameTable *names,
                         SslConstantPool *constants,
                         SslDiagnostic *diagnostic) {
  size_t length = strlen(text);
  unsigned char *input =
      (unsigned char *)copy_bytes((const unsigned char *)text, length);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (input == NULL) {
    ssl_diagnostic_set(diagnostic, source_name, 1, 1,
                       "out of memory while copying input");
    return false;
  }
  /* LCOV_EXCL_STOP */
  return initialize(lexer, input, length, source_name, names, constants,
                    diagnostic);
}

bool ssl_lexer_init_file(SslLexer *lexer, FILE *input, const char *source_name,
                         SslNameTable *names, SslConstantPool *constants,
                         SslDiagnostic *diagnostic) {
  size_t length = 0;
  size_t capacity = 4096;
  unsigned char *buffer = malloc(capacity);
  /* LCOV_EXCL_START: allocator and address-space failures require injection. */
  if (buffer == NULL) {
    ssl_diagnostic_set(diagnostic, source_name, 1, 1,
                       "out of memory while reading input");
    return false;
  }
  /* LCOV_EXCL_STOP */
  for (;;) {
    if (length == capacity) {
      /* LCOV_EXCL_START: SIZE_MAX input and realloc failure are defensive. */
      if (capacity > SIZE_MAX / 2) {
        free(buffer);
        ssl_diagnostic_set(diagnostic, source_name, 1, 1, "input is too large");
        return false;
      }
      /* LCOV_EXCL_STOP */
      capacity *= 2;
      unsigned char *grown = realloc(buffer, capacity);
      /* LCOV_EXCL_START: allocator failures require fault injection. */
      if (grown == NULL) {
        free(buffer);
        ssl_diagnostic_set(diagnostic, source_name, 1, 1,
                           "out of memory while reading input");
        return false;
      }
      /* LCOV_EXCL_STOP */
      buffer = grown;
    }
    size_t count = fread(buffer + length, 1, capacity - length, input);
    length += count;
    if (count == 0)
      break;
  }
  if (ferror(input)) {
    free(buffer);
    ssl_diagnostic_set(diagnostic, source_name, 1, 1, "failed to read input");
    return false;
  }
  return initialize(lexer, buffer, length, source_name, names, constants,
                    diagnostic);
}

void ssl_lexer_destroy(SslLexer *lexer) {
  if (lexer == NULL)
    return;
  free(lexer->input);
  free(lexer->source_name);
  *lexer = (SslLexer){0};
}

static bool ascii_letter(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool ascii_digit(unsigned char c) { return c >= '0' && c <= '9'; }

static bool ascii_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

static unsigned char peek(const SslLexer *lexer) {
  return lexer->offset < lexer->length ? lexer->input[lexer->offset] : 0;
}

static unsigned char advance(SslLexer *lexer) {
  unsigned char c = peek(lexer);
  /* All callers guard advance with remaining input. */
  if (lexer->offset < lexer->length) { /* LCOV_EXCL_BR_LINE */
    ++lexer->offset;
    if (c == '\n') {
      ++lexer->line;
      lexer->column = 1;
    } else {
      ++lexer->column;
    }
  }
  return c;
}

static bool finish_token(SslLexer *lexer, SslToken *token, size_t start,
                         size_t line, size_t column, SslTokenKind kind,
                         int32_t secondary, SslDiagnostic *diagnostic) {
  token->lexeme = copy_bytes(lexer->input + start, lexer->offset - start);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (token->lexeme == NULL) {
    ssl_diagnostic_set(diagnostic, lexer->source_name, line, column,
                       "out of memory while storing lexeme");
    return false;
  }
  /* LCOV_EXCL_STOP */
  token->primary = kind;
  token->secondary = secondary;
  token->line = line;
  token->column = column;
  token->end_line = lexer->line;
  token->end_column = lexer->column;
  return true;
}

static bool pool_failure(SslLexer *lexer, SslDiagnostic *diagnostic,
                         size_t line, size_t column) {
  /* LCOV_EXCL_START: reached only through injected allocation failures. */
  ssl_diagnostic_set(diagnostic, lexer->source_name, line, column,
                     "out of memory while assigning secondary token");
  return false;
  /* LCOV_EXCL_STOP */
}

static SslTokenKind scan_symbol(SslLexer *lexer) {
  static const SslTokenKind single_byte[128] = {
      [':'] = TOK_COLON,        [';'] = TOK_SEMI_COLON,
      [','] = TOK_COMMA,        ['['] = TOK_LEFT_SQUARE,
      [']'] = TOK_RIGHT_SQUARE, ['{'] = TOK_LEFT_BRACE,
      ['}'] = TOK_RIGHT_BRACE,  ['('] = TOK_LEFT_PAREN,
      [')'] = TOK_RIGHT_PAREN,  ['.'] = TOK_DOT,
      ['*'] = TOK_TIMES,        ['/'] = TOK_DIVIDE,
      ['+'] = TOK_PLUS,         ['-'] = TOK_MINUS,
      ['&'] = TOK_INVALID,      ['|'] = TOK_INVALID,
      ['<'] = TOK_LESS_THAN,    ['>'] = TOK_GREATER_THAN,
      ['!'] = TOK_NOT,          ['='] = TOK_EQUALS,
  };
  static const unsigned char second_byte[128] = {
      ['+'] = '+', ['-'] = '-', ['&'] = '&', ['|'] = '|',
      ['<'] = '=', ['>'] = '=', ['!'] = '=', ['='] = '=',
  };
  static const SslTokenKind double_byte[128] = {
      ['+'] = TOK_PLUS_PLUS,     ['-'] = TOK_MINUS_MINUS,
      ['&'] = TOK_AND,           ['|'] = TOK_OR,
      ['<'] = TOK_LESS_OR_EQUAL, ['>'] = TOK_GREATER_OR_EQUAL,
      ['!'] = TOK_NOT_EQUAL,     ['='] = TOK_EQUAL_EQUAL,
  };

  unsigned char first = advance(lexer);
  if (first >= sizeof single_byte / sizeof *single_byte)
    return TOK_INVALID;
  if (second_byte[first] != 0 && peek(lexer) == second_byte[first]) {
    advance(lexer);
    return double_byte[first];
  }
  return single_byte[first] == TOK_EOF ? TOK_INVALID : single_byte[first];
}

bool ssl_lexer_next(SslLexer *lexer, SslToken *token,
                    SslDiagnostic *diagnostic) {
  *token = (SslToken){.secondary = SSL_NO_SECONDARY_TOKEN};
  while (lexer->offset < lexer->length && ascii_space(peek(lexer)))
    advance(lexer);
  size_t start = lexer->offset;
  size_t line = lexer->line;
  size_t column = lexer->column;
  if (start == lexer->length) {
    return finish_token(lexer, token, start, line, column, TOK_EOF,
                        SSL_NO_SECONDARY_TOKEN, diagnostic);
  }

  unsigned char c = peek(lexer);
  if (ascii_letter(c)) {
    do {
      advance(lexer);
    } while (lexer->offset < lexer->length &&
             (ascii_letter(peek(lexer)) || ascii_digit(peek(lexer))));
    char *text = copy_bytes(lexer->input + start, lexer->offset - start);
    if (text == NULL) /* LCOV_EXCL_BR_LINE: allocator failure. */
      return pool_failure(lexer, diagnostic, line, column);
    SslTokenKind kind = ssl_keyword_kind(text);
    int32_t secondary = SSL_NO_SECONDARY_TOKEN;
    if (kind == TOK_IDENTIFIER) {
      secondary = ssl_name_table_intern(lexer->names, text, NULL);
      if (secondary < 0) { /* LCOV_EXCL_BR_LINE: allocator failure. */
        free(text);
        return pool_failure(lexer, diagnostic, line, column);
      }
    }
    free(text);
    return finish_token(lexer, token, start, line, column, kind, secondary,
                        diagnostic);
  }

  if (ascii_digit(c)) {
    int64_t value = 0;
    while (lexer->offset < lexer->length && ascii_digit(peek(lexer))) {
      int digit = advance(lexer) - '0';
      if (value > (INT64_MAX - digit) / 10) {
        while (lexer->offset < lexer->length && ascii_digit(peek(lexer)))
          advance(lexer);
        ssl_diagnostic_set(diagnostic, lexer->source_name, line, column,
                           "integer literal exceeds INT64_MAX");
        return false;
      }
      value = value * 10 + digit;
    }
    int32_t secondary = ssl_constant_pool_add_integer(lexer->constants, value);
    if (secondary < 0) /* LCOV_EXCL_BR_LINE: allocator failure. */
      return pool_failure(lexer, diagnostic, line, column);
    return finish_token(lexer, token, start, line, column, TOK_NUMERAL,
                        secondary, diagnostic);
  }

  if (c == '\'') {
    advance(lexer);
    if (lexer->offset == lexer->length) {
      ssl_diagnostic_set(diagnostic, lexer->source_name, line, column,
                         "unterminated character literal");
      return false;
    }
    unsigned char value = advance(lexer);
    if (lexer->offset == lexer->length || peek(lexer) != '\'') {
      ssl_diagnostic_set(diagnostic, lexer->source_name, line, column,
                         "character literal must contain exactly one byte");
      return false;
    }
    advance(lexer);
    int32_t secondary =
        ssl_constant_pool_add_character(lexer->constants, value);
    if (secondary < 0) /* LCOV_EXCL_BR_LINE: allocator failure. */
      return pool_failure(lexer, diagnostic, line, column);
    return finish_token(lexer, token, start, line, column, TOK_CHARACTER,
                        secondary, diagnostic);
  }

  if (c == '"') {
    advance(lexer);
    size_t content_start = lexer->offset;
    while (lexer->offset < lexer->length && peek(lexer) != '"')
      advance(lexer);
    if (lexer->offset == lexer->length) {
      ssl_diagnostic_set(diagnostic, lexer->source_name, line, column,
                         "unterminated string literal");
      return false;
    }
    char *value =
        copy_bytes(lexer->input + content_start, lexer->offset - content_start);
    if (value == NULL) /* LCOV_EXCL_BR_LINE: allocator failure. */
      return pool_failure(lexer, diagnostic, line, column);
    advance(lexer);
    int32_t secondary = ssl_constant_pool_add_string(lexer->constants, value);
    free(value);
    if (secondary < 0) /* LCOV_EXCL_BR_LINE: allocator failure. */
      return pool_failure(lexer, diagnostic, line, column);
    return finish_token(lexer, token, start, line, column, TOK_STRING_VALUE,
                        secondary, diagnostic);
  }

  SslTokenKind kind = scan_symbol(lexer);
  if (kind == TOK_INVALID) {
    ssl_diagnostic_set(diagnostic, lexer->source_name, line, column,
                       "invalid character or incomplete operator 0x%02X", c);
    return false;
  }
  return finish_token(lexer, token, start, line, column, kind,
                      SSL_NO_SECONDARY_TOKEN, diagnostic);
}
