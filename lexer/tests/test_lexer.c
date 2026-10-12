#include "ssl/lexer.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned tests_run;
static unsigned tests_failed;

#define CHECK(condition)                                                       \
  do {                                                                         \
    ++tests_run;                                                               \
    if (!(condition)) {                                                        \
      ++tests_failed;                                                          \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,         \
              #condition);                                                     \
    }                                                                          \
  } while (0)

static void test_token_metadata(void) {
  CHECK(strcmp(ssl_token_kind_name(TOK_FUNCTION), "FUNCTION") == 0);
  CHECK(strcmp(ssl_token_kind_name(TOK_INVALID), "INVALID") == 0);
  CHECK(ssl_keyword_kind("array") == TOK_ARRAY);
  CHECK(ssl_keyword_kind("while") == TOK_WHILE);
  CHECK(ssl_keyword_kind("While") == TOK_IDENTIFIER);
  CHECK(ssl_keyword_kind("widget") == TOK_IDENTIFIER);
  ssl_token_dispose(NULL);
  ssl_diagnostic_set(NULL, NULL, 0, 0, "ignored");
  SslDiagnostic diagnostic = {0};
  ssl_diagnostic_set(&diagnostic, NULL, 4, 2, "fallback source");
  CHECK(strcmp(diagnostic.source, "<input>") == 0);
}

static void test_name_table(void) {
  SslNameTable table;
  ssl_name_table_init(&table);
  bool inserted = false;
  CHECK(ssl_name_table_intern(&table, "alpha", &inserted) == 0 && inserted);
  CHECK(ssl_name_table_intern(&table, "alpha", &inserted) == 0 && !inserted);
  for (int i = 0; i < 100; ++i) {
    char name[32];
    snprintf(name, sizeof name, "name%d", i);
    CHECK(ssl_name_table_intern(&table, name, NULL) == i + 1);
  }
  CHECK(strcmp(ssl_name_table_get(&table, 100), "name99") == 0);
  CHECK(ssl_name_table_get(&table, -1) == NULL);
  CHECK(ssl_name_table_get(&table, 101) == NULL);
  ssl_name_table_destroy(&table);
  ssl_name_table_destroy(NULL);
}

static void test_constant_pool(void) {
  SslConstantPool pool;
  ssl_constant_pool_init(&pool);
  CHECK(ssl_constant_pool_add_character(&pool, 'x') == 0);
  CHECK(ssl_constant_pool_add_integer(&pool, 42) == 1);
  CHECK(ssl_constant_pool_add_string(&pool, "hello") == 2);
  CHECK(ssl_constant_pool_get(&pool, 0)->value.character == 'x');
  CHECK(ssl_constant_pool_get(&pool, 1)->value.integer == 42);
  CHECK(strcmp(ssl_constant_pool_get(&pool, 2)->value.string, "hello") == 0);
  CHECK(ssl_constant_pool_get(&pool, 3) == NULL);
  ssl_constant_pool_destroy(&pool);
  ssl_constant_pool_destroy(NULL);
  ssl_constant_pool_init(&pool);
  pool.count = (size_t)INT32_MAX;
  CHECK(ssl_constant_pool_add_integer(&pool, 1) == -1);
  pool.count = 0;
  ssl_constant_pool_destroy(&pool);
}

static void test_lexer(void) {
  const char *text =
      "var x: integer; x x 12 12 'q' \"hi\" ++ -- && || <= >= != ==";
  const SslTokenKind expected[] = {TOK_VAR,
                                   TOK_IDENTIFIER,
                                   TOK_COLON,
                                   TOK_INTEGER,
                                   TOK_SEMI_COLON,
                                   TOK_IDENTIFIER,
                                   TOK_IDENTIFIER,
                                   TOK_NUMERAL,
                                   TOK_NUMERAL,
                                   TOK_CHARACTER,
                                   TOK_STRING_VALUE,
                                   TOK_PLUS_PLUS,
                                   TOK_MINUS_MINUS,
                                   TOK_AND,
                                   TOK_OR,
                                   TOK_LESS_OR_EQUAL,
                                   TOK_GREATER_OR_EQUAL,
                                   TOK_NOT_EQUAL,
                                   TOK_EQUAL_EQUAL,
                                   TOK_EOF};
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  SslDiagnostic diagnostic = {0};
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  CHECK(ssl_lexer_init_text(&lexer, text, "lex", &names, &constants,
                            &diagnostic));
  int32_t x_id = -2;
  for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
    SslToken token;
    CHECK(ssl_lexer_next(&lexer, &token, &diagnostic));
    CHECK(token.primary == expected[i]);
    if (token.primary == TOK_IDENTIFIER) {
      if (x_id == -2)
        x_id = token.secondary;
      CHECK(token.secondary == x_id);
    }
    ssl_token_dispose(&token);
  }
  CHECK(names.count == 1);
  CHECK(constants.count == 4);
  CHECK(ssl_constant_pool_get(&constants, 0)->value.integer == 12);
  CHECK(ssl_constant_pool_get(&constants, 1)->value.integer == 12);
  CHECK(ssl_constant_pool_get(&constants, 2)->value.character == 'q');
  CHECK(strcmp(ssl_constant_pool_get(&constants, 3)->value.string, "hi") == 0);
  ssl_lexer_destroy(&lexer);
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
}

static void expect_lex_error(const char *text, const char *fragment) {
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  SslDiagnostic diagnostic = {0};
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  CHECK(ssl_lexer_init_text(&lexer, text, "bad", &names, &constants,
                            &diagnostic));
  SslToken token;
  bool ok;
  do {
    ok = ssl_lexer_next(&lexer, &token, &diagnostic);
    if (ok)
      ssl_token_dispose(&token);
  } while (ok && token.primary != TOK_EOF);
  CHECK(!ok);
  CHECK(strstr(diagnostic.message, fragment) != NULL);
  ssl_lexer_destroy(&lexer);
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
}

static void test_lexer_errors(void) {
  expect_lex_error("bad_name", "invalid character");
  expect_lex_error("&", "incomplete operator");
  expect_lex_error("'ab'", "exactly one byte");
  expect_lex_error("\"open", "unterminated string");
  expect_lex_error("9223372036854775808", "INT64_MAX");
  expect_lex_error("922337203685477580800x", "INT64_MAX");
  expect_lex_error("|", "incomplete operator");
  expect_lex_error("@", "invalid character");
  expect_lex_error("\200", "invalid character");
  expect_lex_error("'", "unterminated character");
  expect_lex_error("'a", "exactly one byte");
}

static void test_lexer_file_and_positions(void) {
  FILE *file = tmpfile();
  CHECK(file != NULL);
  if (file == NULL)
    return;
  const char text[] =
      " \t\r\f\v\nabc : ; , [ ] { } ( ) . * / + - < > ! = \"\" '''";
  for (size_t i = 0; i < 5000; ++i)
    CHECK(fputc(' ', file) == ' ');
  CHECK(fwrite(text, 1, sizeof text - 1, file) == sizeof text - 1);
  rewind(file);
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  SslDiagnostic diagnostic = {0};
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  CHECK(ssl_lexer_init_file(&lexer, file, "positions", &names, &constants,
                            &diagnostic));
  fclose(file);
  const SslTokenKind expected[] = {
      TOK_IDENTIFIER,   TOK_COLON,        TOK_SEMI_COLON, TOK_COMMA,
      TOK_LEFT_SQUARE,  TOK_RIGHT_SQUARE, TOK_LEFT_BRACE, TOK_RIGHT_BRACE,
      TOK_LEFT_PAREN,   TOK_RIGHT_PAREN,  TOK_DOT,        TOK_TIMES,
      TOK_DIVIDE,       TOK_PLUS,         TOK_MINUS,      TOK_LESS_THAN,
      TOK_GREATER_THAN, TOK_NOT,          TOK_EQUALS,     TOK_STRING_VALUE,
      TOK_CHARACTER,    TOK_EOF};
  for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
    SslToken token;
    CHECK(ssl_lexer_next(&lexer, &token, &diagnostic));
    CHECK(token.primary == expected[i]);
    if (i == 0) {
      CHECK(token.line == 2);
      CHECK(token.column == 1);
    }
    ssl_token_dispose(&token);
  }
  CHECK(constants.count == 2);
  CHECK(strcmp(ssl_constant_pool_get(&constants, 0)->value.string, "") == 0);
  CHECK(ssl_constant_pool_get(&constants, 1)->value.character == '\'');
  ssl_lexer_destroy(&lexer);
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
  ssl_lexer_destroy(NULL);

  file = fopen("/dev/null", "wb");
  CHECK(file != NULL);
  if (file != NULL) {
    ssl_name_table_init(&names);
    ssl_constant_pool_init(&constants);
    CHECK(!ssl_lexer_init_file(&lexer, file, "write-only", &names, &constants,
                               &diagnostic));
    CHECK(strstr(diagnostic.message, "failed to read") != NULL);
    fclose(file);
    ssl_name_table_destroy(&names);
    ssl_constant_pool_destroy(&constants);
  }
}

int main(void) {
  test_token_metadata();
  test_name_table();
  test_constant_pool();
  test_lexer();
  test_lexer_errors();
  test_lexer_file_and_positions();
  if (tests_failed != 0) {
    fprintf(stderr, "%u/%u lexer checks failed\n", tests_failed, tests_run);
    return EXIT_FAILURE;
  }
  printf("all %u lexer checks passed\n", tests_run);
  return EXIT_SUCCESS;
}
