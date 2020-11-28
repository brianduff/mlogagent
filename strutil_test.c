#include "strutil.h"
#include <stdio.h>
#include <time.h>

// A little test program to verify that config file loading works.
int main(int argc, char **argv) {
  buffer_reset();

  buffer_append("Hello ");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World %s", "Foo Long very long long long");
  buffer_append("World %s", "Foo Long very long long long");
  buffer_append("World %s", "Foo Long very long long long");
  buffer_append("World %s", "Foo Long very long long long");
  buffer_append("World %s", "Foo Long very long long long");
  buffer_append("World %s", "Foo Long very long long long");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");
  buffer_append("World");

  for (int i = 0; i < 250000; i++) {
    printf("%s\n", buffer_get());
  }

  // /a.out  0.13s user 0.43s system 2% cpu 24.798 total

}