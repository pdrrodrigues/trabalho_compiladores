#include "ssl/semantic.h"

#include <stdlib.h>

typedef enum {
  TY_ERROR,
  TY_INTEGER,
  TY_CHAR,
  TY_BOOLEAN,
  TY_STRING,
  TY_ARRAY,
  TY_STRUCT,
  TY_ALIAS
} TypeKind;
typedef enum { SY_TYPE, SY_VAR, SY_PARAM, SY_FIELD, SY_FUNCTION } SymbolKind;

typedef struct Type Type;
typedef struct Symbol Symbol;
typedef struct Scope Scope;

struct Type {
  TypeKind kind;
  Type *element;
  Symbol *fields;
  int64_t length;
  int64_t size;
};
struct Symbol {
  int32_t name;
  SymbolKind kind;
  Type definition;
  Type *type;
  Symbol *members;
  Symbol *member_next;
  Symbol *scope_next;
  Symbol *owned_next;
  int64_t offset;
  int64_t index;
  int64_t parameters;
  int64_t locals;
};
struct Scope {
  Scope *parent;
  Scope *owned_next;
  Symbol *symbols;
};
struct SslSemanticContext {
  const SslNameTable *names;
  const SslConstantPool *constants;
  const char *source;
  SslDiagnostic *diagnostic;
  Symbol *symbols;
  Scope *scopes;
  Type primitives[5];
};
typedef struct SslSemanticContext Analysis;

static SslNode *child(SslNode *node, size_t index) {
  return node->children[index];
}

static Symbol *binding(SslNode *node) { return (Symbol *)node->binding; }

static const char *name_of(Analysis *a, SslNode *id) {
  const char *name = ssl_name_table_get(a->names, id->secondary);
  return name != NULL ? name : "<identifier>";
}

static bool error_at(Analysis *a, SslNode *node, const char *message) {
  ssl_diagnostic_set(a->diagnostic, a->source, node->line, node->column, "%s",
                     message);
  return false;
}

static bool named_error(Analysis *a, SslNode *id, const char *message) {
  ssl_diagnostic_set(a->diagnostic, a->source, id->line, id->column, "%s `%s`",
                     message, name_of(a, id));
  return false;
}

static Scope *new_scope(Analysis *a, Scope *parent, SslNode *at) {
  Scope *scope = calloc(1, sizeof *scope);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (scope == NULL) {
    error_at(a, at, "out of memory while creating scope");
    return NULL;
  }
  /* LCOV_EXCL_STOP */
  scope->parent = parent;
  scope->owned_next = a->scopes;
  a->scopes = scope;
  return scope;
}

static Symbol *find_here(Scope *scope, int32_t name) {
  for (Symbol *s = scope->symbols; s != NULL; s = s->scope_next)
    if (s->name == name)
      return s;
  return NULL;
}

static Symbol *find_visible(Scope *scope, int32_t name) {
  for (; scope != NULL; scope = scope->parent) {
    Symbol *found = find_here(scope, name);
    if (found != NULL)
      return found;
  }
  return NULL;
}

static bool declare(Analysis *a, Scope *scope, SslNode *id, SymbolKind kind) {
  if (find_here(scope, id->secondary) != NULL)
    return named_error(a, id, "duplicate declaration of");
  Symbol *symbol = calloc(1, sizeof *symbol);
  /* LCOV_EXCL_START: allocator failures require fault injection. */
  if (symbol == NULL)
    return error_at(a, id, "out of memory while declaring identifier");
  /* LCOV_EXCL_STOP */
  symbol->name = id->secondary;
  symbol->kind = kind;
  symbol->definition.kind = TY_ERROR;
  symbol->scope_next = scope->symbols;
  scope->symbols = symbol;
  symbol->owned_next = a->symbols;
  a->symbols = symbol;
  id->binding = symbol;
  return true;
}

static bool use(Analysis *a, Scope *scope, SslNode *id) {
  Symbol *symbol = find_visible(scope, id->secondary);
  if (symbol == NULL)
    return named_error(a, id, "undeclared identifier");
  id->binding = symbol;
  return true;
}

static bool scope_type(Analysis *a, Scope *scope, SslNode *type) {
  return type->production != 10 || use(a, scope, child(type, 0));
}

static bool scope_expression(Analysis *a, Scope *scope, SslNode *node) {
  if (node->production == 69)
    return use(a, scope, child(node, 0));
  if (node->production == 67)
    return scope_expression(a, scope, child(node, 0));
  if (node->production == 57 && !use(a, scope, child(node, 0)))
    return false;
  for (size_t i = 0; i < node->count; ++i) {
    SslNode *part = child(node, i);
    if (part->production != 0 && !scope_expression(a, scope, part))
      return false;
  }
  return true;
}

static bool scope_ids(Analysis *a, Scope *scope, SslNode *list,
                      SymbolKind kind) {
  if (list->production == 23 && !scope_ids(a, scope, child(list, 0), kind))
    return false;
  return declare(a, scope, child(list, list->count - 1), kind);
}

static bool scope_fields(Analysis *a, Scope *outer, Scope *fields,
                         SslNode *node) {
  if (node->production == 14 && !scope_fields(a, outer, fields, child(node, 0)))
    return false;
  return scope_type(a, outer, child(node, node->count - 1)) &&
         scope_ids(a, fields, child(node, node->count - 3), SY_FIELD);
}

static bool scope_params(Analysis *a, Scope *scope, SslNode *node) {
  if (node->production == 17 && !scope_params(a, scope, child(node, 0)))
    return false;
  size_t id_index = node->production == 17 ? 2 : 0;
  return scope_type(a, scope, child(node, id_index + 2)) &&
         declare(a, scope, child(node, id_index), SY_PARAM);
}

static bool scope_block(Analysis *a, Scope *scope, SslNode *node);

static bool scope_statement(Analysis *a, Scope *scope, SslNode *node) {
  if (node->production == 31) {
    Scope *inner = new_scope(a, scope, node);
    return inner != NULL && scope_block(a, inner, child(node, 0));
  }
  for (size_t i = 0; i < node->count; ++i) {
    SslNode *part = child(node, i);
    if (part->production == 0)
      continue;
    if (part->production >= 27 && part->production <= 34) {
      if (!scope_statement(a, scope, part))
        return false;
    } else if (!scope_expression(a, scope, part)) {
      return false;
    }
  }
  return true;
}

static bool scope_statements(Analysis *a, Scope *scope, SslNode *node) {
  if (node->production == 25 && !scope_statements(a, scope, child(node, 0)))
    return false;
  return scope_statement(a, scope, child(node, node->count - 1));
}

static bool scope_variables(Analysis *a, Scope *scope, SslNode *node) {
  if (node->production == 20 && !scope_variables(a, scope, child(node, 0)))
    return false;
  SslNode *declaration = child(node, node->count - 1);
  return scope_type(a, scope, child(declaration, 3)) &&
         scope_ids(a, scope, child(declaration, 1), SY_VAR);
}

static bool scope_block(Analysis *a, Scope *scope, SslNode *node) {
  return scope_variables(a, scope, child(node, 1)) &&
         scope_statements(a, scope, child(node, 2));
}

static bool scope_external(Analysis *a, Scope *global, SslNode *node) {
  SslNode *declaration = child(node, 0);
  if (declaration->production == 16) {
    SslNode *id = child(declaration, 1);
    if (!declare(a, global, id, SY_FUNCTION))
      return false;
    Scope *scope = new_scope(a, global, declaration);
    return scope != NULL && scope_params(a, scope, child(declaration, 3)) &&
           scope_type(a, scope, child(declaration, 6)) &&
           scope_block(a, scope, child(declaration, 7));
  }
  SslNode *id = child(declaration, 1);
  if (declaration->production == 11 &&
      !scope_type(a, global, child(declaration, 8)))
    return false;
  if (declaration->production == 13 &&
      !scope_type(a, global, child(declaration, 3)))
    return false;
  if (declaration->production == 12) {
    Scope *fields = new_scope(a, global, declaration);
    if (fields == NULL ||
        !scope_fields(a, global, fields, child(declaration, 5)))
      return false;
  }
  return declare(a, global, id, SY_TYPE);
}

static bool scope_externals(Analysis *a, Scope *global, SslNode *node) {
  if (node->production == 2 && !scope_externals(a, global, child(node, 0)))
    return false;
  return scope_external(a, global, child(node, node->count - 1));
}

static Type *primitive(Analysis *a, TypeKind kind) {
  return &a->primitives[kind];
}

static bool is_type(Symbol *symbol) { return symbol->kind == SY_TYPE; }

static Type *type_reference(Analysis *a, SslNode *node) {
  switch (node->production) {
  case 6:
    return primitive(a, TY_INTEGER);
  case 7:
    return primitive(a, TY_CHAR);
  case 8:
    return primitive(a, TY_BOOLEAN);
  case 9:
    return primitive(a, TY_STRING);
  default:
    if (!is_type(binding(child(node, 0)))) {
      named_error(a, child(node, 0), "type expected at");
      return NULL;
    }
    return &binding(child(node, 0))->definition;
  }
}

static void append_member(Symbol *owner, Symbol *member) {
  Symbol **tail = &owner->members;
  while (*tail != NULL)
    tail = &(*tail)->member_next;
  *tail = member;
}

static bool type_fields(Analysis *a, Symbol *owner, SslNode *node);

static bool assign_list(Analysis *a, Symbol *owner, SslNode *list, Type *type) {
  if (list->production == 23 && !assign_list(a, owner, child(list, 0), type))
    return false;
  Symbol *symbol = binding(child(list, list->count - 1));
  symbol->type = type;
  if (owner != NULL)
    append_member(owner, symbol);
  return true;
}

static bool type_fields(Analysis *a, Symbol *owner, SslNode *node) {
  if (node->production == 14 && !type_fields(a, owner, child(node, 0)))
    return false;
  Type *type = type_reference(a, child(node, node->count - 1));
  return type != NULL &&
         assign_list(a, owner, child(node, node->count - 3), type);
}

static bool type_params(Analysis *a, Symbol *function, SslNode *node) {
  if (node->production == 17 && !type_params(a, function, child(node, 0)))
    return false;
  size_t index = node->production == 17 ? 2 : 0;
  Type *type = type_reference(a, child(node, index + 2));
  if (type == NULL)
    return false;
  Symbol *parameter = binding(child(node, index));
  parameter->type = type;
  append_member(function, parameter);
  return true;
}

static bool type_declaration(Analysis *a, SslNode *node) {
  Symbol *symbol = binding(child(node, 1));
  if (node->production == 11) {
    Type *element = type_reference(a, child(node, 8));
    if (element == NULL)
      return false;
    symbol->definition.kind = TY_ARRAY;
    symbol->definition.element = element;
    const SslConstant *length =
        ssl_constant_pool_get(a->constants, child(node, 5)->secondary);
    if (length != NULL)
      symbol->definition.length = length->value.integer;
    return true;
  }
  if (node->production == 12) {
    if (!type_fields(a, symbol, child(node, 5)))
      return false;
    symbol->definition.kind = TY_STRUCT;
    symbol->definition.fields = symbol->members;
    return true;
  }
  Type *base = type_reference(a, child(node, 3));
  if (base == NULL)
    return false;
  symbol->definition.kind = TY_ALIAS;
  symbol->definition.element = base;
  return true;
}

static bool compatible(Type *left, Type *right) { return left == right; }

static Type *type_expression(Analysis *a, SslNode *node);

static Type *type_lvalue(Analysis *a, SslNode *node) {
  if (node->production == 69) {
    SslNode *id = child(node, 0);
    Symbol *symbol = binding(id);
    if (symbol->kind != SY_VAR && symbol->kind != SY_PARAM) {
      named_error(a, id, "variable expected at");
      return NULL;
    }
    return symbol->type;
  }
  Type *base = type_lvalue(a, child(node, 0));
  if (base == NULL)
    return NULL;
  if (node->production == 67) {
    if (base->kind != TY_STRUCT) {
      error_at(a, child(node, 1), "struct value required for field access");
      return NULL;
    }
    SslNode *id = child(node, 2);
    for (Symbol *field = base->fields; field != NULL;
         field = field->member_next)
      if (field->name == id->secondary)
        return (id->binding = field), field->type;
    named_error(a, id, "unknown field");
    return NULL;
  }
  if (base != primitive(a, TY_STRING) && base->kind != TY_ARRAY) {
    error_at(a, child(node, 1), "array or string value required for indexing");
    return NULL;
  }
  Type *index = type_expression(a, child(node, 2));
  if (index == NULL)
    return NULL;
  if (!compatible(index, primitive(a, TY_INTEGER))) {
    error_at(a, child(node, 2), "array index must be integer");
    return NULL;
  }
  if (base == primitive(a, TY_STRING))
    return primitive(a, TY_CHAR);
  return base->element;
}

static bool type_arguments(Analysis *a, SslNode *node, Symbol **parameter,
                           size_t *number) {
  if (node->production == 65 &&
      !type_arguments(a, child(node, 0), parameter, number))
    return false;
  SslNode *expression = child(node, node->count - 1);
  Type *argument = type_expression(a, expression);
  if (argument == NULL)
    return false;
  ++*number;
  if (*parameter == NULL)
    return error_at(a, expression, "too many function arguments");
  if (!compatible((*parameter)->type, argument))
    return error_at(a, expression, "function argument type mismatch");
  *parameter = (*parameter)->member_next;
  return true;
}

static Type *type_expression(Analysis *a, SslNode *node) {
  unsigned p = node->production;
  if (p == 67 || p == 68 || p == 69)
    return type_lvalue(a, node);
  if (p == 51)
    return type_lvalue(a, child(node, 0));
  if (p == 57) {
    SslNode *id = child(node, 0);
    Symbol *function = binding(id);
    if (function->kind != SY_FUNCTION) {
      named_error(a, id, "function expected at");
      return NULL;
    }
    Symbol *parameter = function->members;
    size_t number = 0;
    if (!type_arguments(a, child(node, 2), &parameter, &number))
      return NULL;
    if (parameter != NULL) {
      error_at(a, id, "too few function arguments");
      return NULL;
    }
    return function->type;
  }
  if (p == 56)
    return type_expression(a, child(node, 1));
  if (p >= 60 && p <= 64) {
    TypeKind kind = p == 60 || p == 61 ? TY_BOOLEAN
                    : p == 62          ? TY_CHAR
                    : p == 63          ? TY_STRING
                                       : TY_INTEGER;
    return primitive(a, kind);
  }
  if (p == 52 || p == 53 || p == 54 || p == 55 || p == 58 || p == 59) {
    SslNode *operand = child(node, p == 54 || p == 55 ? 0 : 1);
    Type *type = p >= 52 && p <= 55 ? type_lvalue(a, operand)
                                    : type_expression(a, operand);
    if (type == NULL)
      return NULL;
    Type *expected = primitive(a, p == 59 ? TY_BOOLEAN : TY_INTEGER);
    if (!compatible(type, expected)) {
      error_at(a, node,
               p == 59 ? "boolean operand required"
                       : "integer operand required");
      return NULL;
    }
    return expected;
  }
  if (p == 37 || p == 44 || p == 47 || p == 50)
    return type_expression(a, child(node, 0));
  Type *left = type_expression(a, child(node, 0));
  if (left == NULL)
    return NULL;
  Type *right = type_expression(a, child(node, 2));
  if (right == NULL)
    return NULL;
  if (p == 35 || p == 36) {
    if (left != primitive(a, TY_BOOLEAN) || right != primitive(a, TY_BOOLEAN)) {
      error_at(a, child(node, 1), "boolean operands required");
      return NULL;
    }
    return primitive(a, TY_BOOLEAN);
  }
  if (!compatible(left, right)) {
    error_at(a, child(node, 1), "type mismatch between operands");
    return NULL;
  }
  if (p >= 38 && p <= 43)
    return primitive(a, TY_BOOLEAN);
  if (p == 45 &&
      (left == primitive(a, TY_INTEGER) || left == primitive(a, TY_STRING)))
    return left;
  if ((p == 46 || p == 48 || p == 49) && left == primitive(a, TY_INTEGER))
    return left;
  error_at(a, child(node, 1), "invalid operand type for operator");
  return NULL;
}

static bool type_statement(Analysis *a, SslNode *node);

static bool type_statements(Analysis *a, SslNode *node) {
  if (node->production == 25 && !type_statements(a, child(node, 0)))
    return false;
  return type_statement(a, child(node, node->count - 1));
}

static bool type_variables(Analysis *a, SslNode *node) {
  if (node->production == 20 && !type_variables(a, child(node, 0)))
    return false;
  SslNode *declaration = child(node, node->count - 1);
  Type *type = type_reference(a, child(declaration, 3));
  return type != NULL && assign_list(a, NULL, child(declaration, 1), type);
}

static bool type_block(Analysis *a, SslNode *node) {
  return type_variables(a, child(node, 1)) &&
         type_statements(a, child(node, 2));
}

static bool type_condition(Analysis *a, SslNode *node) {
  Type *type = type_expression(a, node);
  return type != NULL && (type == primitive(a, TY_BOOLEAN) ||
                          error_at(a, node, "condition must be boolean"));
}

static bool type_statement(Analysis *a, SslNode *node) {
  switch (node->production) {
  case 27:
  case 28:
    return type_condition(a, child(node, 2)) &&
           type_statement(a, child(node, 4)) &&
           (node->production == 27 || type_statement(a, child(node, 6)));
  case 29:
    return type_condition(a, child(node, 2)) &&
           type_statement(a, child(node, 4));
  case 30:
    return type_statement(a, child(node, 1)) &&
           type_condition(a, child(node, 4));
  case 31:
    return type_block(a, child(node, 0));
  case 32: {
    Type *left = type_lvalue(a, child(node, 0));
    if (left == NULL)
      return false;
    Type *right = type_expression(a, child(node, 2));
    return right != NULL &&
           (compatible(left, right) ||
            error_at(a, child(node, 1), "assignment type mismatch"));
  }
  default:
    return true; /* break and continue carry no type attributes. */
  }
}

static bool type_external(Analysis *a, SslNode *node) {
  SslNode *declaration = child(node, 0);
  if (declaration->production != 16)
    return type_declaration(a, declaration);
  Symbol *function = binding(child(declaration, 1));
  if (!type_params(a, function, child(declaration, 3)))
    return false;
  function->type = type_reference(a, child(declaration, 6));
  return function->type != NULL && type_block(a, child(declaration, 7));
}

static bool type_externals(Analysis *a, SslNode *node) {
  if (node->production == 2 && !type_externals(a, child(node, 0)))
    return false;
  return type_external(a, child(node, node->count - 1));
}

static int64_t type_size(Type *type) {
  if (type->size != 0)
    return type->size;
  if (type->kind == TY_ALIAS)
    type->size = type_size(type->element);
  else if (type->kind == TY_ARRAY)
    type->size = type->length * type_size(type->element);
  else if (type->kind == TY_STRUCT) {
    int64_t size = 0;
    for (Symbol *field = type->fields; field != NULL; field = field->member_next) {
      field->offset = size;
      size += type_size(field->type);
    }
    type->size = size;
  } else
    type->size = 1;
  return type->size;
}

static void layout_ids(SslNode *list, int64_t *offset) {
  if (list->production == 23)
    layout_ids(child(list, 0), offset);
  Symbol *symbol = binding(child(list, list->count - 1));
  symbol->offset = *offset;
  *offset += type_size(symbol->type);
}

static void layout_variables(SslNode *node, int64_t *offset) {
  if (node->production == 20)
    layout_variables(child(node, 0), offset);
  SslNode *declaration = child(node, node->count - 1);
  layout_ids(child(declaration, 1), offset);
}

static void layout_statement(SslNode *node, int64_t *offset);

static void layout_statements(SslNode *node, int64_t *offset) {
  if (node->production == 25)
    layout_statements(child(node, 0), offset);
  layout_statement(child(node, node->count - 1), offset);
}

static void layout_block(SslNode *node, int64_t *offset) {
  layout_variables(child(node, 1), offset);
  layout_statements(child(node, 2), offset);
}

static void layout_statement(SslNode *node, int64_t *offset) {
  if (node->production == 31)
    layout_block(child(node, 0), offset);
  else if (node->production == 27 || node->production == 28) {
    layout_statement(child(node, 4), offset);
    if (node->production == 28)
      layout_statement(child(node, 6), offset);
  } else if (node->production == 29)
    layout_statement(child(node, 4), offset);
  else if (node->production == 30)
    layout_statement(child(node, 1), offset);
}

static void layout_external(SslNode *node, int64_t *function_index) {
  SslNode *declaration = child(node, 0);
  if (declaration->production != 16)
    return;
  Symbol *function = binding(child(declaration, 1));
  function->index = (*function_index)++;
  int64_t offset = 0;
  for (Symbol *parameter = function->members; parameter != NULL;
       parameter = parameter->member_next) {
    parameter->offset = offset;
    offset += type_size(parameter->type);
  }
  function->parameters = offset;
  layout_block(child(declaration, 7), &offset);
  function->locals = offset - function->parameters;
}

static void layout_externals(SslNode *node, int64_t *function_index) {
  if (node->production == 2)
    layout_externals(child(node, 0), function_index);
  layout_external(child(node, node->count - 1), function_index);
}

static void clear_bindings(SslNode *node) {
  node->binding = NULL;
  for (size_t i = 0; i < node->count; ++i)
    clear_bindings(child(node, i));
}

SslSemanticContext *ssl_semantic_analyze(SslNode *tree, const SslNameTable *names,
                                         const SslConstantPool *constants,
                                         const char *source,
                                         SslDiagnostic *diagnostic) {
  Analysis *a = calloc(1, sizeof *a);
  if (a == NULL) {
    ssl_diagnostic_set(diagnostic, source, 0, 0, "out of memory while creating analysis");
    return NULL;
  }
  *a = (Analysis){.names = names,
                .constants = constants,
                .source = source,
                .diagnostic = diagnostic};
  for (int i = TY_ERROR; i <= TY_STRING; ++i)
    a->primitives[i].kind = (TypeKind)i;
  Scope *global = new_scope(a, NULL, tree);
  bool valid = global != NULL && scope_externals(a, global, child(tree, 0)) &&
               type_externals(a, child(tree, 0));
  if (valid) {
    int64_t function_index = 0;
    layout_externals(child(tree, 0), &function_index);
  }
  if (!valid) {
    ssl_semantic_context_destroy(a);
    return NULL;
  }
  return a;
}

void ssl_semantic_context_destroy(SslSemanticContext *a) {
  if (a == NULL)
    return;
  while (a->symbols != NULL) {
    Symbol *next = a->symbols->owned_next;
    free(a->symbols);
    a->symbols = next;
  }
  while (a->scopes != NULL) {
    Scope *next = a->scopes->owned_next;
    free(a->scopes);
    a->scopes = next;
  }
  free(a);
}

int64_t ssl_semantic_node_size(const SslSemanticContext *context,
                               const SslNode *node) {
  (void)context;
  Symbol *symbol = node == NULL ? NULL : (Symbol *)node->binding;
  return symbol != NULL && symbol->type != NULL ? type_size(symbol->type) : 1;
}
int64_t ssl_semantic_lvalue_size(const SslSemanticContext *context,
                                 SslNode *node) {
  Type *type = type_lvalue((Analysis *)context, node);
  return type == NULL ? 1 : type_size(type);
}
bool ssl_semantic_lvalue_is_string(const SslSemanticContext *context,
                                   SslNode *node) {
  Type *type = type_lvalue((Analysis *)context, node);
  return type == primitive((Analysis *)context, TY_STRING);
}
int64_t ssl_semantic_node_offset(const SslSemanticContext *context,
                                 const SslNode *node) {
  (void)context;
  Symbol *symbol = node == NULL ? NULL : (Symbol *)node->binding;
  return symbol == NULL ? 0 : symbol->offset;
}
int64_t ssl_semantic_function_index(const SslSemanticContext *context,
                                    const SslNode *node) {
  (void)context;
  Symbol *symbol = node == NULL ? NULL : (Symbol *)node->binding;
  return symbol == NULL ? 0 : symbol->index;
}
int64_t ssl_semantic_function_parameters(const SslSemanticContext *context,
                                         const SslNode *node) {
  (void)context;
  Symbol *symbol = node == NULL ? NULL : (Symbol *)node->binding;
  return symbol == NULL ? 0 : symbol->parameters;
}
int64_t ssl_semantic_function_locals(const SslSemanticContext *context,
                                     const SslNode *node) {
  (void)context;
  Symbol *symbol = node == NULL ? NULL : (Symbol *)node->binding;
  return symbol == NULL ? 0 : symbol->locals;
}

bool ssl_semantic_check(SslNode *tree, const SslNameTable *names,
                        const SslConstantPool *constants, const char *source,
                        SslDiagnostic *diagnostic) {
  SslSemanticContext *context =
      ssl_semantic_analyze(tree, names, constants, source, diagnostic);
  if (context == NULL)
    return false;
  clear_bindings(tree);
  ssl_semantic_context_destroy(context);
  return true;
}
