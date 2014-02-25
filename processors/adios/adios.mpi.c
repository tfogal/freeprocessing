#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include "adios.h"
#include "debug.h"
#include "parallel.mpi.h"

DECLARE_CHANNEL(adios);

#define INVALID_FD -1
/* adios' file descriptor for operations. */
static int64_t fdes = INVALID_FD;

static void
initialize()
{
  static int have_initialized = 0;
  if(!have_initialized) {
    have_initialized = 1;
    /* init has a return value, but no code I can find seems to check it.  I
     * can't find any documentation on what it's *supposed* to return. */
    adios_init("config.xml", MPI_COMM_WORLD);
  }
}

__attribute__((destructor(500))) static void
close_file()
{
  adios_finalize(rank());
}

/* Just like basename, except doesn't modify it's damn argument.  Allocates
 * memory; user must free it. */
MALLOC static char*
basename_r(const char* s)
{
  char* t = strdup(s);
  char* rv = strdup(basename(t));
  free(t);
  return rv;
}

void
exec(const char* fn, const void* buf, const size_t n)
{
  /* TRACE(adios, "stream %s(%p, %zu)", fn, buf, n); */
  initialize();
  if(fdes == INVALID_FD) {
    char fname[256];
    char* bname = basename_r(fn);
    TRACE(adios, "bn(%s): %s", fn, bname);
    snprintf(fname, 256, "%s.adios", bname);
    free(bname);
    adios_open(&fdes, "freeprocessing", fname, "w", MPI_COMM_WORLD);
    if(fdes == 0) {
      ERR(adios, "adios_open did not update file descriptor!");
    }
  }
  uint64_t totsize;
  adios_group_size(fdes, n, &totsize);
  /* TRACE(adios, "total size for group write of %zu: %" PRIu64, n, totsize);*/
  adios_write(fdes, "fpvar", (void*)buf);
}

void
finish(const char* fn)
{
  TRACE(adios, "closing file %s", fn);
  adios_close(fdes);
  fdes = INVALID_FD;
}
