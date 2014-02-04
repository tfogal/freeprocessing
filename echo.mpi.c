/* A simple freeprocessor that just prints out what it sees.
 * Useful for gaining a preliminary understanding of what a simulation is
 * doing. */
#include "debug.h"
#include "parallel.mpi.h"

DECLARE_CHANNEL(echo);

void
exec(const char* fn, const void* buf, size_t n)
{
  TRACE(echo, "[%zu] write %p to %s: %zu bytes", rank(), buf, fn, n);
}

void
finish(const char* fn)
{
  TRACE(echo, "[%zu] done with %s", rank(), fn);
}
