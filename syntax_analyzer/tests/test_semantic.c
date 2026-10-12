#include "ssl/semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *source;
  const char *error;
} Case;

static unsigned checks;
static unsigned failures;

static void check_case(const Case *test) {
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  SslDiagnostic diagnostic = {0};
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  bool valid = ssl_lexer_init_text(&lexer, test->source, "<semantic-test>",
                                   &names, &constants, &diagnostic);
  SslNode *tree = NULL;
  if (valid) {
    valid = ssl_parser_parse_tree(&lexer, false, NULL, &tree, &diagnostic);
    if (!valid && tree != NULL) {
      ++failures;
      fprintf(stderr, "failed parse returned a tree: %s\n", test->source);
    }
    if (valid)
      valid = ssl_semantic_check(tree, &names, &constants, lexer.source_name,
                                 &diagnostic);
    ssl_node_destroy(tree);
    ssl_lexer_destroy(&lexer);
  }
  ++checks;
  if ((test->error == NULL && !valid) ||
      (test->error != NULL &&
       (valid || strstr(diagnostic.message, test->error) == NULL)) ||
      (!valid && (diagnostic.line == 0 || diagnostic.column == 0))) {
    ++failures;
    fprintf(stderr, "case failed: %s\nexpected: %s\nactual: %s\n", test->source,
            test->error == NULL ? "valid" : test->error,
            valid ? "valid" : diagnostic.message);
  }
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
}

static void check_tree_shape(void) {
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  SslDiagnostic diagnostic = {0};
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  if (!ssl_lexer_init_text(&lexer, "type Item = integer", "<tree-test>", &names,
                           &constants, &diagnostic)) {
    ++failures;
  } else {
    SslNode *tree = NULL;
    if (!ssl_parser_parse_tree(&lexer, false, NULL, &tree, &diagnostic)) {
      ++failures;
    } else {
      SslNode *declaration = tree->children[0]->children[0]->children[0];
      SslNode *id = declaration->children[1];
      ++checks;
      if (tree->production != 1 || declaration->production != 13 ||
          id->token != TOK_IDENTIFIER || id->secondary != 0 || id->line != 1 ||
          id->column != 6 || id->count != 0) {
        ++failures;
        fprintf(stderr, "parse tree lost production or token data\n");
      }
    }
    ssl_node_destroy(tree);
    ssl_lexer_destroy(&lexer);
  }
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
}

#define FN(body) "function f(x: integer): integer { var y: integer; " body " }"

int main(void) {
  check_tree_shape();
  const Case cases[] = {
      {"type A = integer type A = char", "duplicate declaration"},
      {"type A = integer function A(x: integer): integer { var y: integer; y = "
       "x; }",
       "duplicate declaration"},
      {FN("y = x; var z: integer;"), "syntax error"},
      {"function f(x: integer): integer { var x: integer; x = 1; }",
       "duplicate declaration"},
      {"function f(x: integer): integer { var y,y: integer; y = x; }",
       "duplicate declaration"},
      {"function f(x: integer): integer { var y,y,z: integer; y = x; }",
       "duplicate declaration"},
      {"type S = struct { x: integer; x: char }", "duplicate declaration"},
      {"type S = struct { x,x: integer; y: char }", "duplicate declaration"},
      {"type S = struct { x: integer; y: Missing }", "undeclared identifier"},
      {"type S = struct { x: Missing; y: integer }", "undeclared identifier"},
      {"type A = array [3] of Missing", "undeclared identifier"},
      {"type A = A", "undeclared identifier"},
      {"type A = B type B = integer", "undeclared identifier"},
      {FN("y = missing;"), "undeclared identifier"},
      {"function f(x: integer): integer { var y: integer; "
       "{ var z: integer; z = missing; } }",
       "undeclared identifier"},
      {"function f(x: integer): integer { var y: integer; "
       "y = missing; y = x; }",
       "undeclared identifier"},
      {"function f(x: integer): integer { var y: integer; "
       "var z: Missing; y = x; }",
       "undeclared identifier"},
      {"function f(x: integer): integer { var y: Missing; "
       "var z: integer; z = x; }",
       "undeclared identifier"},
      {"function f(x: integer, y: Missing): integer { var z: integer; "
       "z = x; }",
       "undeclared identifier"},
      {"function f(x: integer, x: integer, y: char): integer { "
       "var z: integer; z = x; }",
       "duplicate declaration"},
      {"function f(x: integer): integer { var y: integer; y = later(x); } "
       "function later(x: integer): integer { var y: integer; y = x; }",
       "undeclared identifier"},
      {"type A = integer type S = struct { x: A; y: A } "
       "function f(x: integer): integer { var y: integer; y = x; }",
       NULL},
      {"type A = integer type B = struct { x: integer } "
       "type C = struct { x: char }",
       NULL},
      {"type S = struct { S: integer; A: char } type A = integer "
       "function f(x: S): integer { var y: integer; y = x.S; }",
       NULL},
      {"type S = struct { x,y: integer } "
       "function f(x: S): integer { var y: integer; y = x.y; }",
       NULL},
      {"function f(x: integer): integer { var y: integer; "
       "{ var x: integer; x = 1; } y = x; }",
       NULL},
      {"function f(x: integer): integer { var y: integer; "
       "{ var y: integer; y = x; } y = x; }",
       NULL},
      {"function f(x: integer): integer { var y: integer; y = f(x); }", NULL},
      {"type A = array [4] of integer type S = struct { item: A } "
       "function f(x: A): integer { var y: A; y = x; }",
       NULL},
      {"type A = integer type B = A function f(x: A): B { var y: B; y = x; }",
       "assignment type mismatch"},
      {"type A = integer function f(x: A): A { var y: A; y = x; }", NULL},
      {"type A = integer type B = integer "
       "function f(x: A): integer { var y: B; y = x; }",
       "assignment type mismatch"},
      {"function f(x: integer): integer { var y: char; y = x; }",
       "assignment type mismatch"},
      {"function f(x: integer): integer { var y: x; y = x; }", "type expected"},
      {"function f(x: integer): x { var y: integer; y = x; }", "type expected"},
      {"function g(x: integer): integer { var y: integer; y = x; } "
       "type A = g",
       "type expected"},
      {"function g(x: integer): integer { var y: integer; y = x; } "
       "type A = array [2] of g",
       "type expected"},
      {"function g(x: integer): integer { var y: integer; y = x; } "
       "type A = struct { value: g }",
       "type expected"},
      {"function g(x: integer): integer { var y: integer; y = x; } "
       "function f(x: g): integer { var y: integer; y = 1; }",
       "type expected"},
      {"function g(x: integer): integer { var y: integer; y = x; } "
       "function f(x: integer): integer { var y: g; y = x; }",
       "type expected"},
      {"type T = integer function f(x: T): integer { var y: integer; "
       "y = T(x); }",
       "function expected"},
      {"type T = integer function f(x: integer): integer { var y: integer; "
       "y = T; }",
       "variable expected"},
      {"function f(x: integer): integer { var y: integer; if (x) y = 1; }",
       "condition must be boolean"},
      {"function f(x: boolean): integer { var y: integer; "
       "while (x) y = 1; do y = 2; while (x); if (x) y = 3; else y = 4; }",
       NULL},
      {"function f(x: boolean): integer { var y: integer; "
       "while (1) y = 1; }",
       "condition must be boolean"},
      {"function f(x: boolean): integer { var y: integer; "
       "do y = 1; while (1); }",
       "condition must be boolean"},
      {"function f(x: integer): integer { var y: integer; "
       "if (x + true) y = 1; }",
       "type mismatch"},
      {"function f(x: boolean): integer { var y: integer; "
       "if (x) y = true; }",
       "assignment type mismatch"},
      {"function f(x: boolean): integer { var y: integer; "
       "if (x) y = 1; else y = true; }",
       "assignment type mismatch"},
      {"function f(x: boolean): integer { var y: integer; "
       "while (x) y = true; }",
       "assignment type mismatch"},
      {"function f(x: boolean): integer { var y: integer; "
       "do y = true; while (x); }",
       "assignment type mismatch"},
      {"function f(x: boolean): integer { var y: integer; "
       "if (x) y = 1; else { var z: integer; z = 2; } }",
       NULL},
      {"function f(x: boolean): integer { var y: integer; "
       "while (x) { var z: integer; if (x) break; else continue; } }",
       NULL},
      {"function f(x: boolean): integer { var y: integer; y = x && 1; }",
       "boolean operands required"},
      {"function f(x: boolean): integer { var y: boolean; "
       "y = true && false || !x; }",
       NULL},
      {"function f(x: integer): integer { var y: integer; "
       "y = x < 2; }",
       "assignment type mismatch"},
      {"function f(x: integer): boolean { var y: boolean; "
       "y = x < true; }",
       "type mismatch"},
      {FN("y = x + true;"), "type mismatch"},
      {FN("y = x * x + x / x - -x;"), NULL},
      {"function f(x: string): string { var y: string; y = x + \"a\"; }", NULL},
      {"function f(x: string): string { var y: string; y = x - x; }",
       "invalid operand type"},
      {"function f(x: char): char { var y: char; y = x + x; }",
       "invalid operand type"},
      {"function f(x: char): char { var y: char; y = x / x; }",
       "invalid operand type"},
      {"function f(x: boolean): boolean { var y: boolean; y = x * x; }",
       "invalid operand type"},
      {"function f(x: boolean): boolean { var y: boolean; y = -x; }",
       "integer operand required"},
      {"function f(x: integer): boolean { var y: boolean; y = !x; }",
       "boolean operand required"},
      {"function f(x: integer): boolean { var y: boolean; "
       "y = x < 2 && x >= 1 || x == 0 && x != 3; }",
       NULL},
      {"type A = array [3] of integer "
       "function f(x: A): integer { var y: integer; y = x[1]; x[2] = y; }",
       NULL},
      {"type S = struct { value: integer; label: string } "
       "function f(x: S): integer { var y: integer; y = x.value; }",
       NULL},
      {"function f(x: string): char { var y: char; y = x[0]; }", NULL},
      {"type A = string function f(x: A): char { var y: char; y = x[0]; }",
       "array or string value required"},
      {"function f(x: integer): integer { var y: integer; y = x[0]; }",
       "array or string value required"},
      {"function f(x: integer): integer { var y: integer; y = x[missing]; }",
       "undeclared identifier"},
      {"type A = array [3] of integer "
       "function f(x: A): integer { var y: integer; y = x[true]; }",
       "array index must be integer"},
      {"type S = struct { value: integer } "
       "function f(x: S): integer { var y: integer; y = x.other; }",
       "unknown field"},
      {"type S = struct { value: integer } "
       "function f(x: S): integer { var y: integer; y = x.value.other; }",
       "struct value required"},
      {"type S = struct { value: integer } "
       "function f(x: S): integer { var y: integer; x.value = y; }",
       NULL},
      {"function f(x: integer): integer { var y: integer; y = x.value; }",
       "struct value required"},
      {"type S = struct { value: integer } "
       "function f(x: S): integer { var y: integer; y = x++; }",
       "integer operand required"},
      {FN("y = ++x + y--;"), NULL},
      {FN("y = --x + x++ - y;"), NULL},
      {"function f(x: char): char { var y: char; y = ++x; }",
       "integer operand required"},
      {"function f(x: integer): integer { var y: integer; "
       "y = (x); }",
       NULL},
      {FN("y = f(x, x);"), "too many function arguments"},
      {"function f(x: integer, z: char): integer { var y: integer; "
       "y = f(x); }",
       "too few function arguments"},
      {"function f(x: integer): integer { var y: integer; "
       "y = f(true); }",
       "function argument type mismatch"},
      {"function f(x: integer): integer { var y: integer; "
       "y = f(true + 1); }",
       "type mismatch"},
      {"function f(x: integer, z: char): integer { var y: integer; "
       "y = f(x, 'a'); }",
       NULL},
      {"function f(x: integer): integer { var y: integer; "
       "y = f(x) + f(x); }",
       NULL},
      {FN("y = x(1);"), "function expected"},
      {"type A = integer function f(x: A): integer { var y: integer; y = A; }",
       "variable expected"},
      {"function f(x: integer): integer { var y: integer; y = x + ; z = 1; }",
       "syntax error"},
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
    check_case(&cases[i]);
  if (failures != 0) {
    fprintf(stderr, "%u/%u semantic cases failed\n", failures, checks);
    return EXIT_FAILURE;
  }
  printf("all %u semantic cases passed\n", checks);
  return EXIT_SUCCESS;
}
