#include <stdio.h>

void
exec(unsigned dtype, const size_t dims[3], const void* buf, size_t n)
{
  printf("%s(%u, [%zu %zu %zu], %p, %zu)\n", __FILE__, dtype,
         dims[0],dims[1],dims[2], buf, n);
}
