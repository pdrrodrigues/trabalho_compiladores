#include "ssl/parser.h"
#include "ssl/codegen.h"
#include "ssl/semantic.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program) {
  fprintf(stderr, "usage: %s [--syntax-only] [--tokens] FILE|-\n", program);
}

int main(int argc, char **argv) {
  bool dump_tokens = false;
  bool syntax_only = false;
  const char *path = NULL;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--tokens") == 0)
      dump_tokens = true;
    else if (strcmp(argv[i], "--syntax-only") == 0)
      syntax_only = true;
    else if (path == NULL && (argv[i][0] != '-' || strcmp(argv[i], "-") == 0))
      path = argv[i];
    else {
      usage(argv[0]);
      return 2;
    }
  }
  if (path == NULL) {
    usage(argv[0]);
    return 2;
  }
  FILE *input = stdin;
  if (strcmp(path, "-") != 0) {
    input = fopen(path, "rb");
    if (input == NULL) {
      fprintf(stderr, "%s: cannot open input\n", path);
      return 2;
    }
  }
  SslNameTable names;
  SslConstantPool constants;
  SslLexer lexer;
  SslDiagnostic diagnostic = {0};
  ssl_name_table_init(&names);
  ssl_constant_pool_init(&constants);
  bool initialized =
      ssl_lexer_init_file(&lexer, input, path, &names, &constants, &diagnostic);
  if (input != stdin)
    fclose(input);
  if (!initialized) {
    fprintf(stderr, "%s:%zu:%zu: %s\n", diagnostic.source, diagnostic.line,
            diagnostic.column, diagnostic.message);
    ssl_name_table_destroy(&names);
    ssl_constant_pool_destroy(&constants);
    return 2;
  }
  bool valid_tables = ssl_parser_validate_tables(&diagnostic);
  SslNode *tree = NULL;
  bool valid =
      valid_tables &&
      (syntax_only ? ssl_parser_parse(&lexer, dump_tokens, stdout, &diagnostic)
                   : ssl_parser_parse_tree(&lexer, dump_tokens, stdout, &tree,
                                           &diagnostic));
  SslSemanticContext *semantic = NULL;
  if (valid && !syntax_only && !dump_tokens) {
    semantic = ssl_semantic_analyze(tree, &names, &constants, path, &diagnostic);
    valid = semantic != NULL;
    if (valid)
      valid = ssl_codegen_emit(stdout, tree, semantic, &constants, path, &diagnostic);
  } else if (valid && !syntax_only) {
    valid = ssl_semantic_check(tree, &names, &constants, path, &diagnostic);
  }
  ssl_semantic_context_destroy(semantic);
  ssl_node_destroy(tree);
  if (!valid) {
    fprintf(stderr, "%s:%zu:%zu: %s\n", diagnostic.source, diagnostic.line,
            diagnostic.column, diagnostic.message);
  }
  ssl_lexer_destroy(&lexer);
  ssl_name_table_destroy(&names);
  ssl_constant_pool_destroy(&constants);
  return valid ? 0
               : (!valid_tables ||
                          strstr(diagnostic.message, "out of memory") != NULL
                      ? 2
                      : 1);
}
