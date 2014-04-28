#include <stdio.h>
void
exec(const char* fn, const void* buf, size_t n) {
  (void)fn; (void)buf; (void)n;
  printf("Hello, world!\n");
}
