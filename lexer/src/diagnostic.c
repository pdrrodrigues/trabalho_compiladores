#include "ssl/diagnostic.h"

#include <stdarg.h>
#include <stdio.h>

void ssl_diagnostic_set(SslDiagnostic *diagnostic, const char *source,
                        size_t line, size_t column, const char *format, ...) {
  if (diagnostic == NULL) {
    return;
  }
  diagnostic->source = source != NULL ? source : "<input>";
  diagnostic->line = line;
  diagnostic->column = column;
  va_list arguments;
  va_start(arguments, format);
  (void)vsnprintf(diagnostic->message, sizeof diagnostic->message, format,
                  arguments);
  va_end(arguments);
}
