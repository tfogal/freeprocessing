/* Calculate the min/max of integer data that flows through it.
 * WARNING: this has a subtle bug, do not use directly! */
#include <limits.h>
#include <stdio.h>

void
exec(const char* fn, const void* buf, size_t n) {
  (void)fn;
  const int* data = (const int*)buf;
  const size_t elems = n / sizeof(int);
  int minmax[2] = { INT_MAX, INT_MIN };
  for(size_t i=0; i < elems; ++i) {
    minmax[0] = data[i] < minmax[0] ? data[i] : minmax[0];
    minmax[1] = data[i] > minmax[1] ? data[i] : minmax[1];
  }
  printf("The data range is: [%d:%d]\n", minmax[0], minmax[1]);
}
