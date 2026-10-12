#ifndef SSL_DIAGNOSTIC_H
#define SSL_DIAGNOSTIC_H

#include <stddef.h>

typedef struct {
  const char *source;
  size_t line;
  size_t column;
  char message[768];
} SslDiagnostic;

void ssl_diagnostic_set(SslDiagnostic *diagnostic, const char *source,
                        size_t line, size_t column, const char *format, ...);

#endif
