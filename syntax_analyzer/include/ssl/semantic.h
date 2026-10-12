#ifndef SSL_SEMANTIC_H
#define SSL_SEMANTIC_H

#include "ssl/parser.h"

/* The tree and name table must remain alive for the duration of this call. */
bool ssl_semantic_check(SslNode *tree, const SslNameTable *names,
                        const SslConstantPool *constants, const char *source,
                        SslDiagnostic *diagnostic);

#endif
