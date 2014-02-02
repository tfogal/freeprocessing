#include <assert.h>
#include <stdio.h>
#include "debug.h"
#include "parallel.mpi.h"

DECLARE_CHANNEL(enzo);

void
exec(const char* fn, const void* buf, size_t n)
{
  TRACE(enzo, "write %p to %s: %zu bytes", buf, fn, n);
  char fname[256];
  FILE* fp;
  for(size_t i=0; i < 64; ++i) {
    snprintf(fname, 256, "%zu-xvel.%zu", i, rank());
    /* try to create the file, but with O_EXCL: i.e. it *must* be created.
     * if it actually creates, it we're done.  if it fails, keep spinning (up
     * to 64times) as per the loop. */
    fp = fopen(fname, "wex");
    if(fp) { break; }
  }
  if(fp == NULL) { return; } /* fuck it, dude.  ... Let's go bowling. */
  assert(fp);
  if(fwrite(buf, 1, n, fp) != n) {
    WARN(enzo, "short write writing %zu-byte field %s!", n, fn);
  }
  if(fclose(fp) != 0) {
    ERR(enzo, "error closing '%s'!", fn);
  }
}

void
finish(const char* fn)
{
  TRACE(enzo, "done with %s", fn);
}
