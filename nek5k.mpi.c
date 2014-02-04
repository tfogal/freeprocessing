/* A freeprocessor for the nek5000 code.
 * We hardcode a field size of 35*35*1 since this is just a demonstration, and
 * that's what our test program generates.  If you wanted to generalize this,
 * you'd probably want to read this from a config file or something. */
#include <inttypes.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "debug.h"
#include "fp-png.h"
#include "parallel.mpi.h"

DECLARE_CHANNEL(nek);

/* dimensions of the data we're looking for.  should probably read this from
 * somewhere instead of hard coding it. */
static const size_t dims[3] = { 35, 35, 1 };
static uint16_t* quant = NULL; /* tmp array for quantized float data */
/* timestep and field number.  we expect to get 5 fields per timestep */
static size_t ts=0;
static size_t fld=0;

static void
range(const float* data, size_t n, float* low, float* high)
{
  *low = FLT_MAX;
  *high = FLT_MIN;
  for(size_t i=0; i < n; ++i) {
    *low = data[i] < *low ? data[i] : *low;
    *high = data[i] > *high ? data[i] : *high;
  }
}

static uint16_t
lerp(float v, float imin,float imax, uint16_t omin,uint16_t omax)
{
  return (uint16_t)(omin + (v-imin) * ((omax-omin)/(imax-imin)));
}

static void
quantize(const float* flt, size_t n, float low, float high, uint16_t* q)
{
#if 0
  for(const float* f=flt; f < flt+n; ++f, ++q) {
    *q = lerp(*f, low,high, 0,65535);
  }
#else
  for(size_t i=0; i < n; ++i) {
    q[i] = lerp(flt[i], low,high, 0,65535);
  }
#endif
}

static void
next_field()
{
  if(++fld == 5) {
    ++ts;
    fld = 0;
  }
}

void
exec(const char* fn, const void* buf, size_t n)
{
  (void)fn;
  if(n == dims[0]*dims[1]*dims[2]*sizeof(float)) {
    float rng[2];
    range((const float*)buf, n/sizeof(float), &rng[0], &rng[1]);
    TRACE(nek, "[%zu] range: [%14.7g %14.7g]", rank(), rng[0], rng[1]);

    if(quant == NULL) {
      quant = calloc(dims[0]*dims[1]*dims[2], sizeof(uint16_t));
    }
    if(fld != 4) { /* 4 is always blank, it seems. */
      memset(quant, 0, dims[0]*dims[1]*dims[2]*sizeof(uint16_t));
      quantize((const float*)buf, n/sizeof(float), rng[0], rng[1], quant);
      char fname[256];
      snprintf(fname, 256, "%zu.fld%zu.png", ts, fld);
      writepng(fname, quant, dims[0], dims[1]);
    }

    next_field();
  }
}

void
finish(const char* fn)
{
  TRACE(nek, "[%zu] done with %s", rank(), fn);
  free(quant);
  quant = NULL;
}
