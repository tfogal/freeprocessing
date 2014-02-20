#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <silo.h>
#include "../compiler.h"
#include "../debug.h"
#include "jsdd.h"
#include "json.h"
#include "dtd.h"

DECLARE_CHANNEL(silo);

/* Silo file we'll create/output into. */
static DBfile* slf = NULL;
/* metadata for the silo file, read from 'silo.cfg'. */
static struct dtd smd;
/* buffer to use for input data when writes are small */
static void* data = NULL;
/* how much we've filled within data, in bytes. */
static size_t filled = 0;

MALLOC static char*
slurp(const char* from)
{
  FILE* fp = fopen(from, "r");
  if(!fp) {
    ERR(silo, "could not slurp %s", from);
    return NULL;
  }
  if(fseek(fp, 0, SEEK_END) != 0) {
    ERR(silo, "error getting fsize from %s", from);
    fclose(fp);
    return NULL;
  }
  const long sz = ftell(fp);
  if(-1 == sz) {
    ERR(silo, "ftell failed.");
    fclose(fp);
    return NULL;
  }
  rewind(fp);

  char* rv = calloc(sizeof(char), (sz+1));
  if(fread(rv, sizeof(char), sz, fp) <= 0) {
    WARN(silo, "short read for %s", from);
  }
  fclose(fp);
  return rv;
}

static void
read_metadata(const char* from, struct dtd* md)
{
  char* cfgtext = slurp(from);
  json_value* js = json_parse(cfgtext, strlen(cfgtext));
  free(cfgtext);
  TRACE(silo, "type: %d", js->type);
  md->datatype = js_datatype(js);
  size_t ndims=0;
  {
    size_t* dims = js_dimensions(js, &ndims);
    if(ndims != 3) {
      ERR(silo, "currently only 3D data are supported."); /* Hack. */
      free(dims);
      json_value_free(js);
      return;
    }
    memcpy(md->dims, dims, sizeof(size_t)*3);
    free(dims);
  }
  {
    double** coords = js_coord_arrays(js, md->dims, 3);
    for(size_t i=0; i < ndims; ++i) {
      md->coords[i] = malloc(sizeof(double) * md->dims[i]);
      memcpy(md->coords[i], coords[i], md->dims[i]*sizeof(double));
      free(coords[i]);
    }
    free(coords);
  }

  json_value_free(js);
}

static void
add_mesh(DBfile* sl, struct dtd* md)
{
  const size_t ndims = md->dims[2] == 0 ? 2 : 3;
  /* since Silo's argument types are broken, copy into int array. */
  int dims[3] = { md->dims[0], md->dims[1], md->dims[2] };
  /* final option here should set DBOPT_COORDSYS to DB_CARTESIAN... */
  assert(ndims == 3);
  TRACE(silo, "adding mesh.");
  if(DBPutQuadmesh(sl, "fpmesh", NULL, md->coords, dims, ndims,
                   DB_DOUBLE, DB_NONCOLLINEAR, NULL) == -1) {
    ERR(silo, "Error adding mesh to silo file.");
  }
}

/* derives the silo filename from the given filename. */
MALLOC static char*
silo_fname(const char* f)
{
  char* fname = malloc(sizeof(char)*strlen(f)+8);
  strcpy(fname, f);
  strcat(fname, ".silo");
  return fname;
}

static size_t
width(enum dtype dt)
{
  switch(dt) {
    case BYTE: return 1;
    case FLOAT32: return sizeof(float);
    case FLOAT64: return sizeof(double);
    case GARBAGE: assert(false); return 0;
  }
  assert(false);
  return 0;
}

void
file(const char* fn)
{
  if(slf != NULL) {
    ERR(silo, "We can only handle one silo file at a time. Ignoring %s.", fn);
    return;
  }
  char* fname = silo_fname(fn);
  TRACE(silo, "creating '%s'", fname);
  slf = DBCreate(fname, DB_CLOBBER, DB_LOCAL, NULL, DB_PDB);
  if(NULL == slf) {
    ERR(silo, "Could not create %s!\n", fname);
  }
  free(fname);
  TRACE(silo, "creating directories.");
  if(DBMkDir(slf, "fpdir") != 0) {
    ERR(silo, "could not create silo directory.");
    DBClose(slf);
    slf = NULL;
  }
  if(DBSetDir(slf, "fpdir") != 0) {
    ERR(silo, "could not change silo directory.");
    DBClose(slf);
    slf = NULL;
  }
  read_metadata("silo.cfg", &smd);
  add_mesh(slf, &smd);

  /* create buffer we'll fill up before dumping to silo. */
  const size_t bytes = smd.dims[0]*smd.dims[1]*smd.dims[2] *
                       width(smd.datatype);
  const int err = posix_memalign((void**)&data, sizeof(void*), bytes);
  if(err != 0) {
    ERR(silo, "error (%d) allocating buffer for slice 0", err);
    return;
  }
  memset(data, 0, bytes);
  filled = 0;
}

static int
sdb_type(enum dtype dt)
{
  switch(dt) {
    case FLOAT32: return DB_FLOAT;
    case FLOAT64: return DB_DOUBLE;
    case BYTE: return DB_CHAR;
    case GARBAGE: assert(false); return DB_FLOAT;
  }
  assert(false);
  return DB_FLOAT;
}

/* called when we fill up a buffer. Expects the buffer is described by 'smd'. */
static void
silodump(const void* buf)
{
  int dims[3] = { smd.dims[0], smd.dims[1], smd.dims[2] };
  const int sdbtype = sdb_type(smd.datatype);
  if(DBPutQuadvar1(slf, "fpvar", "fpmesh", (void*)buf, dims, 3, NULL, 0,
                   sdbtype, DB_NODECENT, NULL) != 0) {
    ERR(silo, "error writing quad var");
  }
}
PURE static size_t minzu(size_t a, size_t b) { return a < b ? a : b; }

typedef void (filledfunc)(const void*);
static void
streaming(const void* buf, size_t n, filledfunc* f)
{
  (void)buf; (void)n; (void)f;
  if(n == 0) { return; }
  const size_t bufsize = smd.dims[0]*smd.dims[1]*smd.dims[2] *
                         width(smd.datatype);
  if(filled+n < bufsize) {
    TRACE(silo, "%zu < %zu, copying %zu bytes to %zu", filled+n, bufsize, n,
          filled);
    memcpy(data+filled, buf, n);
    filled += n;
  } else {
    const size_t mn = minzu(n, bufsize-filled);
    assert(mn > 0);
    TRACE(silo, "copying %zu bytes of %zu and then running.", mn, n);
    memcpy(data+filled, buf, mn);
    f(data);

    filled = 0;
    TRACE(silo, "recursing, offset by %zu bytes", mn);
    streaming(buf+mn, n-mn, f);
  }
}

void
exec(const char* fn, const void* buf, size_t n)
{
  (void) fn;
  assert(slf);
  streaming(buf, n, silodump);
}

void
finish(const char* fn)
{
  assert(slf);
  TRACE(silo, "closing %s", fn);
  if(DBClose(slf) != 0) {
    WARN(silo, "could not close silo file.. data probably corrupt.");
  }
  free(data); data = NULL;
  filled = 0;
  slf = NULL;
}
