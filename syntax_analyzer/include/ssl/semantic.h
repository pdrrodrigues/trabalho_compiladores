#ifndef SSL_SEMANTIC_H
#define SSL_SEMANTIC_H

#include "ssl/parser.h"

typedef struct SslSemanticContext SslSemanticContext;

/* Creates an analysis context.  The caller keeps both it and tree alive. */
SslSemanticContext *ssl_semantic_analyze(SslNode *tree,
                                         const SslNameTable *names,
                                         const SslConstantPool *constants,
                                         const char *source,
                                         SslDiagnostic *diagnostic);
void ssl_semantic_context_destroy(SslSemanticContext *context);

/* Layout information associated with a declaration or use node. */
int64_t ssl_semantic_node_size(const SslSemanticContext *context,
                               const SslNode *node);
int64_t ssl_semantic_lvalue_size(const SslSemanticContext *context,
                                 SslNode *node);
bool ssl_semantic_lvalue_is_string(const SslSemanticContext *context,
                                   SslNode *node);
int64_t ssl_semantic_node_offset(const SslSemanticContext *context,
                                 const SslNode *node);
int64_t ssl_semantic_function_index(const SslSemanticContext *context,
                                    const SslNode *node);
int64_t ssl_semantic_function_parameters(const SslSemanticContext *context,
                                         const SslNode *node);
int64_t ssl_semantic_function_locals(const SslSemanticContext *context,
                                     const SslNode *node);

/* The tree and name table must remain alive for the duration of this call. */
bool ssl_semantic_check(SslNode *tree, const SslNameTable *names,
                        const SslConstantPool *constants, const char *source,
                        SslDiagnostic *diagnostic);

#endif
