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
/* buffers to hold 2 slices of the input data. */
static uint16_t* data[2] = { NULL, NULL };
/* how much we've buffered into 'data'.  in bytes. */
static size_t filled = 0;
/* which buffer is active... always 0 or 1. */
static size_t active = 1;
/* size of the data.. for a real processor, we'd have to read this from a file,
 * of course. */
static const size_t dims[3] = { 2025, 1600, 400 };

static void
initialize()
{
  assert(mc == NULL);
  assert(data[0] == NULL);
  assert(data[1] == NULL);
  mc = mcubes();
  mc->skip = false;
  TRACE(iso, "allocating %zu-byte buffer.", dims[0]*dims[1]*sizeof(uint16_t));
  const int err = posix_memalign((void**)&data[0], sizeof(void*),
                                 dims[0]*dims[1]*sizeof(uint16_t));
  if(err != 0) {
    ERR(iso, "error (%d) allocating buffer for slice 1", err);
  }
  const int err2 = posix_memalign((void**)&data[1], sizeof(void*),
                                  dims[0]*dims[1]*sizeof(uint16_t));
  if(err2 != 0) {
    ERR(iso, "error (%d) allocating buffer for slice 1", err2);
  }
  memset(data[0], 0, dims[0]*dims[1]*sizeof(uint16_t));
  memset(data[1], 0, dims[0]*dims[1]*sizeof(uint16_t));
  mc->init(mc, false, sizeof(uint16_t), dims);
}

static size_t minzu(size_t a, size_t b) { return a < b ? a : b; }

void
exec(const char* fn, const void* buf, size_t n)
{
  if(!n) { TRACE(iso, "ignored 0 byte stream."); return; }
  if(mc == NULL) {
    initialize();
  }
  /* Our MC code works on a slice at a time.  Of course, for MC that means it
   * needs the corners of our boxes, so from our point of view here that's
   * *two* slices at a time.  The current run's "front" slice will end up being
   * the next run's "back" slice.  Instead of copying things around, we use two
   * buffers and flip the 'active' bit when switching slices.
   * In any case, before we can run we need to buffer up a slice worth. */
  const size_t sliceelems = dims[0]*dims[1];
  if(filled+n < sliceelems*sizeof(uint16_t)) {
    /* filled is in bytes, but data is uint16_t. */
    TRACE(iso, "copying %zu bytes to buffer %zu at offset %zu", n, active,
          filled);
    memcpy(data[active]+filled/sizeof(uint16_t), buf, n);
    filled += n;
  } else {
    /* fill up the remaining bytes of the buffer. */
    const size_t mn = minzu(n, (sliceelems*sizeof(uint16_t))-filled);
    assert(mn > 0);
    TRACE(iso, "%zu + %zu == %zu > %zu.  copying %zu bytes and running.",
          filled, n, filled+n, sliceelems*sizeof(uint16_t), mn);
    memcpy(data[active]+filled/sizeof(uint16_t), buf, mn);
    /* the front buffer is always the one we *didn't* just finish filling. */
    const void* dptr[2] = { (void*)data[!active], (void*)data[active] };
    mc->run(mc, dptr, dims[0]*dims[1]);

    /* now get set up for the next slice. "copy" the 'second half' to the 'first
     * half', and reset the number of bytes we've read to be 0.  but, first:
     * calculate if there are any bytes we missed, above.  If so, we'll have to
     * recurse. */
    active = !active;
    const size_t skipped = minzu(n, (sliceelems*sizeof(uint16_t))-filled);
    filled = 0;
    exec(fn, buf+skipped, n-skipped);
  }
}

void
finish(const char* fn)
{
  TRACE(iso, "%s done.", fn);
  free(data[0]); data[0] = NULL;
  free(data[1]); data[1] = NULL;
  mc->fini(mc);
  free(mc); mc = NULL;
  filled = 0;
  active = 1;
  /* null out lower half.  the top half will get read into for the next file
   * anyway. */
  memset(data[0], 0, dims[0]*dims[1]*sizeof(uint16_t));
}
