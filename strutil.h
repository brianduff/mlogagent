#ifndef STRUTIL_H_SEEN
#define STRUTIL_H_SEEN

char *buffer_get();
void buffer_append(const char *format, ...);
void buffer_reset();

#endif