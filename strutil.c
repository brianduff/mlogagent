// A probably horrifying thing to append strings safely to a thread local buffer without buffer
// overrun.

#include "strutil.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_LINE_LENGTH 16384

__thread char line_buffer[MAX_LINE_LENGTH];
__thread char *bufferPtr = NULL;

char *buffer_get() {
  return line_buffer;
}

void buffer_reset() {
  bufferPtr = NULL;
}

void buffer_append(const char *format, ...) {
  if (bufferPtr == NULL) {
    bufferPtr = line_buffer;
  }

  va_list args;
  va_start(args, format);

  int remaining_chars = MAX_LINE_LENGTH - (bufferPtr - line_buffer);
  if (remaining_chars > 0) {
    int written = vsnprintf(bufferPtr, remaining_chars, format, args);
    bufferPtr += written;
  }

  va_end(args);
}