#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include "../compiler.h"
#include "../debug.h"
#include "mcubes.h"

DECLARE_CHANNEL(iso);

/* the interface to the code that runs our MC. */
static struct processor* mc = NULL;
/* buffer to hold a slice of the input data. */
static uint16_t* data = NULL;
/* size of the data.. for a real processor, we'd have to read this from a file,
 * of course. */
static const size_t dims[3] = { 2025, 1600, 400 };
/* how much we've buffered into 'data'.  in bytes. */
static size_t filled = 0;

static void
initialize()
{
  assert(mc == NULL);
  assert(data == NULL);
  mc = mcubes();
  mc->skip = false;
  TRACE(iso, "allocating %zu-byte buffer.", dims[0]*dims[1]*sizeof(uint16_t));
  const int err = posix_memalign((void**)&data, sizeof(void*),
                                 dims[0]*dims[1]*sizeof(uint16_t)*2);
  if(err != 0) {
    ERR(iso, "error (%d) allocating buffer for a slice.", err);
  }
  memset(data, 0, dims[0]*dims[1]*sizeof(uint16_t)*2);
  mc->init(mc, false, sizeof(uint16_t), dims);
}

static size_t minzu(size_t a, size_t b) { return a < b ? a : b; }

void
exec(const char* fn, const void* buf, size_t n)
{
  if(!n) { return; }
  if(mc == NULL) {
    initialize();
  }
  /* Our MC code currently works on a slice at time.  So we buffer up to a
   * slice, and then flush it out.  Note, however, that we read into the second
   * half of the buffer; the first half is for the previous slice. */
  const size_t sliceelems = dims[0]*dims[1];
  if(filled+n < sliceelems) {
    /* filled is in bytes, but data is uint16_t. */
    memcpy(data+sliceelems+filled/sizeof(uint16_t), buf, n);
    filled += n;
  } else {
    /* fill up the remaining bytes of the buffer. */
    const size_t mn = minzu(n, (sliceelems*sizeof(uint16_t))-filled);
    assert(mn > 0);
    TRACE(iso, "%zu more bytes will exceed bufsize of %zu; copying %zu bytes "
          "and running.", n, sliceelems*sizeof(uint16_t), mn);
    memcpy(data+sliceelems+filled/sizeof(uint16_t), buf, mn);
    const void* dptr[2] = { (void*)data, (void*)data+sliceelems };
    mc->run(mc, dptr, dims[0]*dims[1]); /* run MC */

    /* now get set up for the next slice.  copy the 'second half' to the 'first
     * half', and reset the number of bytes we read to be 0.  but, first:
     * calculate if there are any bytes we missed, above.  If so, we'll have to
     * recurse. */
    memcpy(data, data+sliceelems, sliceelems*sizeof(uint16_t));
    const size_t skipped = minzu(n, (sliceelems*sizeof(uint16_t))-filled);
    filled = 0;
    if(skipped) { exec(fn, buf+skipped, n-skipped); }
  }
}

void
finish(const char* fn)
{
  TRACE(iso, "%s done.", fn);
  free(data); data = NULL;
  mc->fini(mc);
  free(mc); mc = NULL;
  filled = 0;
  /* null out lower half.  the top half will get read into for the next file
   * anyway. */
  memset(data, 0, dims[0]*dims[1]*sizeof(uint16_t));
}
