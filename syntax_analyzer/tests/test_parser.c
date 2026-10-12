#include "ssl/parser.h"

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

static bool parse_text(const char *text, SslDiagnostic *diagnostic) {
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  bool result = ssl_lexer_init_text(&lexer, text, "<test>", &names, &constants,
                                    diagnostic);
  if (result) {
    result = ssl_parser_parse(&lexer, false, NULL, diagnostic);
    ssl_lexer_destroy(&lexer);
  }
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
  return result;
}

static bool parse_text_with_dump(const char *text, FILE *dump,
                                 SslDiagnostic *diagnostic) {
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  bool result = ssl_lexer_init_text(&lexer, text, "<dump-test>", &names,
                                    &constants, diagnostic);
  if (result) {
    result = ssl_parser_parse(&lexer, true, dump, diagnostic);
    ssl_lexer_destroy(&lexer);
  }
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
  return result;
}

static void expect_parse(bool valid, const char *text) {
  SslDiagnostic diagnostic = {0};
  bool actual = parse_text(text, &diagnostic);
  CHECK(actual == valid);
  if (actual != valid)
    fprintf(stderr, "input: %s\ndiagnostic: %s\n", text, diagnostic.message);
  if (!valid)
    CHECK(diagnostic.line > 0 && diagnostic.column > 0);
}

static void test_parser_valid(void) {
  expect_parse(true, "type Alias = integer");
  expect_parse(true, "type Vector = array [10] of char");
  expect_parse(true,
               "type Pair = struct { left,right: integer; flag: boolean }");
  expect_parse(true, "type Global = integer "
                     "function compute(x: integer, flag: boolean): integer { "
                     "var y,z: integer; "
                     "y = x + 2 * 3; "
                     "if (flag) if (x < y) z = y; else z = x; "
                     "while (z > 0) z = z - 1; "
                     "do z = z + 1; while (z < 2); "
                     "y = compute(x, 1); }");
  expect_parse(true, "function paths(x: integer): integer { var y: integer; "
                     "y.part[x] = ++x + y-- / -x; }");
  expect_parse(true, "function loop(x: integer): integer { var y: integer; "
                     "while (x) if (y) break; else continue; }");
  expect_parse(true, "function operators(a: integer, b: integer): boolean { "
                     "var x: integer; "
                     "x = !false && true || 'a' < b > a <= b >= a == b != "
                     "a + b - a * b / a; x = --a + a++ + b--; }");
  expect_parse(true, "function nested(x: integer): integer { var y: integer; "
                     "{ var z: integer; z = x; } }");
}

static void test_parser_invalid(void) {
  expect_parse(false, "");
  expect_parse(false, "\200");
  /* The printed DE production contains DF and DT, but not DV. */
  expect_parse(false, "var global: integer;");
  expect_parse(false, "function f(): integer { var x: integer; x = 1; }");
  expect_parse(false, "function f(x: integer): integer { }");
  expect_parse(false, "function f(x: integer): integer { var x: integer; }");
  expect_parse(false, "type Empty = struct { }");
  expect_parse(false,
               "function f(x: integer): integer { var y: integer; y = f(); }");
  expect_parse(false, "var x: integer; else x = 1;");
  expect_parse(false, "var snake_name: integer;");
  expect_parse(false, "var x: integer; garbage");
  expect_parse(false, "var x integer;");
  expect_parse(false, "type A = array [1] integer");
  expect_parse(false, "type A = integer _");
}

static void test_token_dump(void) {
  FILE *dump = tmpfile();
  CHECK(dump != NULL);
  if (dump == NULL)
    return;
  SslDiagnostic diagnostic = {0};
  CHECK(
      parse_text_with_dump("function show(x: integer): string { var y: string; "
                           "y = \"line\n\r\t\\\001\"; y = '\"'; }",
                           dump, &diagnostic));
  rewind(dump);
  char output[8192];
  size_t length = fread(output, 1, sizeof output - 1, dump);
  output[length] = '\0';
  CHECK(strstr(output, "primary=9(FUNCTION)") != NULL);
  CHECK(strstr(output, "\\n\\r\\t\\\\\\x01") != NULL);
  CHECK(strstr(output, "primary=0(EOF)") != NULL);
  fclose(dump);
}

static void test_deep_parser_stack(void) {
  char *text = malloc(8192);
  CHECK(text != NULL);
  if (text == NULL)
    return;
  strcpy(text, "function deep(x: integer): integer { var y: integer; y = ");
  for (int i = 0; i < 200; ++i)
    strcat(text, "(");
  strcat(text, "x");
  for (int i = 0; i < 200; ++i)
    strcat(text, ")");
  strcat(text, "; }");
  expect_parse(true, text);
  free(text);
}

int main(void) {
  SslDiagnostic diagnostic = {0};
  CHECK(ssl_parser_validate_tables(&diagnostic));
  test_parser_valid();
  test_parser_invalid();
  test_token_dump();
  test_deep_parser_stack();
  if (tests_failed != 0) {
    fprintf(stderr, "%u/%u parser checks failed\n", tests_failed, tests_run);
    return EXIT_FAILURE;
  }
  printf("all %u parser checks passed\n", tests_run);
  return EXIT_SUCCESS;
}
