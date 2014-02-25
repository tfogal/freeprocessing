#include <assert.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "mcubes.h"
#include "MC.h"

DECLARE_CHANNEL(mc);

struct mcubes {
  func_init* init;
  func_process* run;
  func_finished* fini;
  bool skip;

  /* given state */
  bool is_signed;
  size_t bpc;
  size_t dims[3];
  float isoval;

  /* internal state */
  FILE* vertices;
  FILE* faces;
  size_t n_vertices; /* vertices output so far. */
  size_t slice; /* which slice are we processing? */
  float points[12][3]; /* for triangulating intermediate pieces */
};

static void
init(void* self, const bool is_signed, const size_t bpc, const size_t d[3])
{
  struct mcubes* mc = (struct mcubes*) self;
  mc->is_signed = is_signed;
  mc->bpc = bpc;
  mc->slice = 0;
  mc->vertices = fopen(".vertices", "wb");
  mc->faces = fopen(".faces", "wb");
  mc->n_vertices = 0;
  memcpy(mc->dims, d, sizeof(size_t)*3);

  FILE* fp = fopen(".isovalue", "r");
  if(!fp) {
    ERR(mc, "could not read isovalue from '.isovalue'!\n");
    abort();
  }
  if(fscanf(fp, "%f", &mc->isoval) != 1) {
    ERR(mc, "could not scan FP from .isovalue!\n");
    abort();
  }
  fclose(fp);

  if(mc->vertices == NULL) {
    ERR(mc, "error open vertex file.\n"); abort();
  }
  if(mc->faces == NULL) {
    ERR(mc, "error open face data file.\n"); abort();
  }
}

static void
march(void* self, const void* d[2], const size_t nelems)
{
  struct mcubes* this = (struct mcubes*) self;

  if(nelems != this->dims[0]*this->dims[1]) {
    ERR(mc, "not enough data!\n");
    abort();
  }

  switch(this->bpc) {
    case 1:
      if(this->is_signed) {
        const int8_t** data = (const int8_t**) d;
        this->n_vertices += marchlayer8(data[0], data[1], this->dims,
                                        this->slice,
                                        this->isoval, this->vertices,
                                        this->faces, this->n_vertices);
      } else {
        const uint8_t** data = (const uint8_t**) d;
        this->n_vertices += marchlayeru8(data[0], data[1], this->dims,
                                         this->slice,
                                         this->isoval, this->vertices,
                                         this->faces, this->n_vertices);
      }
      break;
    case 2:
      if(this->is_signed) {
        const int16_t** data = (const int16_t**) d;
        this->n_vertices += marchlayer16(data[0], data[1], this->dims,
                                         this->slice,
                                         this->isoval, this->vertices,
                                         this->faces, this->n_vertices);
      } else {
        const uint16_t** data = (const uint16_t**) d;
        this->n_vertices += marchlayeru16(data[0], data[1], this->dims,
                                          this->slice,
                                          this->isoval, this->vertices,
                                          this->faces, this->n_vertices);
      }
      break;
    default: abort(); /* unimplemented. */ break;
  }
#ifndef NDEBUG
  TRACE(mc, "%zu vertices at slice %zu\n", this->n_vertices, this->slice);
#endif
  this->slice++;
}

static void
finalize(void* self)
{
  struct mcubes* mc = (struct mcubes*) self;
  if(fclose(mc->vertices) != 0) {
    ERR(mc, "error closing vertex data file: %d", errno);
  }
  if(fclose(mc->faces) != 0) {
    ERR(mc, "error closing face data file: %d", errno);
  }
  mc->vertices = mc->faces = NULL;
}

struct processor*
mcubes()
{
  struct mcubes* mc = calloc(sizeof(struct mcubes), 1);
  mc->init = init;
  mc->run = march;
  mc->fini = finalize;
  mc->skip = false;
  assert(offsetof(struct mcubes, init) == offsetof(struct processor, init));
  assert(offsetof(struct mcubes, run ) == offsetof(struct processor, run));
  assert(offsetof(struct mcubes, fini) == offsetof(struct processor, fini));
  assert(offsetof(struct mcubes, skip) == offsetof(struct processor, skip));
  return (struct processor*)mc;
}
