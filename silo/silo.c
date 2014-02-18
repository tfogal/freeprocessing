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
    int64_t** coords = js_coord_arrays(js, md->dims, 3);
    for(size_t i=0; i < ndims; ++i) {
      memcpy(md->coords[i], coords[i], md->dims[i]*sizeof(int64_t));
      free(coords[i]);
    }
    free(coords);
  }

  json_value_free(js);
}

static void
add_mesh(DBfile* sl, const struct dtd* md)
{
  const size_t ndims = md->dims[2] == 0 ? 2 : 3;
  /* since Silo's argument types are broken, copy into int array. */
  int dims[3] = { md->dims[0], md->dims[1], md->dims[2] };
  /* final option here should set DBOPT_COORDSYS to DB_CARTESIAN... */
  if(DBPutQuadmesh(sl, "fpmesh", NULL, (void*)md->coords, dims, ndims,
     DB_INT, DB_COLLINEAR, NULL) == -1) {
    ERR(silo, "Error adding mesh to silo file.");
  }
  /* _Static_assert(1 == 0, "blah");*/
}

void
file(const char* fn)
{
  if(slf != NULL) {
    ERR(silo, "We can only handle one silo file at a time. Ignoring %s.", fn);
    return;
  }
  char* fname = malloc(sizeof(char)*strlen(fn)+8);
  strcpy(fname, fn);
  strcat(fname, ".silo");
  slf = DBCreate(fname, DB_CLOBBER, DB_LOCAL, NULL, DB_PDB);
  if(NULL == slf) {
    ERR(silo, "Could not create %s!\n", fname);
  }
  free(fname);
  read_metadata("silo.cfg", &smd);
  add_mesh(slf, &smd);
}

void
exec(const char* fn, const void* buf, size_t n)
{
  TRACE(silo, "%s(%p %zu)", fn, buf, n);
}
