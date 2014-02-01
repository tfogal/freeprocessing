#include "debug.h"

DECLARE_CHANNEL(enzo);

void
exec(const char* fn, const void* buf, size_t n)
{
  TRACE(enzo, "write %p to %s: %zu bytes", buf, fn, n);
  const double* d = (const double*)buf;
  TRACE(enzo, "%s first few values: %5.3g %5.3g %5.3g %5.3g %5.3g", fn,
        d[0], d[1], d[2], d[3], d[4]);
}

void
finish(const char* fn)
{
  TRACE(enzo, "done with %s", fn);
}
