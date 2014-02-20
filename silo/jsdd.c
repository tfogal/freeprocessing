/* JSON data descriptor.  Reads data descriptions from a particular kind of
 * json. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../compiler.h"
#include "../debug.h"
#include "dtd.h"
#include "json.h"

DECLARE_CHANNEL(jsdd);

static void recurse(FILE* fp, const json_value* v);
static int64_t js_int(const json_value* v);
static double js_double(const json_value* v);
static const json_value* objderef(const json_value* v);
static json_value* js_by_idx(const json_value* v, size_t idx);
static size_t arrlen(const json_value* v);

MALLOC static size_t*
jsarr_to_zuv(const json_value* jsdims)
{
  assert(jsdims->type == json_array);
  const size_t ndims = arrlen(jsdims);
  size_t* rv = calloc(ndims, sizeof(size_t));
  for(size_t i=0; i < ndims; ++i) {
    const json_value* integer = objderef(js_by_idx(jsdims, i));
    assert(integer->type == json_integer);
    if(js_int(integer) < 0) {
      ERR(jsdd, "invalid number of coords (%" PRId64 ") in dimension %zu",
          js_int(integer), i);
    }
    rv[i] = (size_t)js_int(integer);
  }
  return rv;
}

static void
print_object(FILE* fp, const json_value* obj)
{
  assert(obj->type == json_object);
  fprintf(fp, "obj of length %u:\n", obj->u.object.length);
  for(unsigned i=0; i < obj->u.object.length; ++i) {
    fprintf(fp, "length-%u obj elem %u (%s):\n", obj->u.object.length, i,
            obj->u.object.values[i].name);
    const json_value* v = obj->u.object.values[i].value;
    recurse(fp, v);
  }
}
static void
print_integer(FILE* fp, const json_value* integer)
{
  assert(integer->type == json_integer);
  fprintf(fp, "integer: %" PRId64 "\n", integer->u.integer);
}
static void
print_double(FILE* fp, const json_value* dbl)
{
  assert(dbl->type == json_double);
  fprintf(fp, "double: %lf\n", dbl->u.dbl);
}
static void
print_array(FILE* fp, json_value** arr, unsigned len)
{
  printf("array of length %u:\n", len);
  for(size_t i=0; i < len; ++i) {
    json_value* v = arr[i];
    recurse(fp, v);
  }
}
/* useful for printing structure when debugging. */
static void
recurse(FILE* fp, const json_value* v)
{
  switch(v->type) {
    case json_object:
      print_object(fp, v);
      break;
    case json_array:
      print_array(fp, v->u.array.values, v->u.array.length);
      break;
    case json_integer:
      print_integer(fp, v);
      break;
    case json_double:
      print_double(fp, v);
      break;
    case json_string:
      fprintf(fp, "string!\n");
      break;
    case json_boolean:
      fprintf(fp, "boolean!\n");
      break;
    case json_none:
      fprintf(fp, "none?!\n");
      assert(false);
      break;
    default: assert(false);
  }
}

static json_value*
js_by_name(const json_value* v, const char* name)
{
  if(v->type != json_object) {
    return NULL;
  }
  for(unsigned i=0; i < v->u.object.length; ++i) {
    if(strcasecmp(v->u.object.values[i].name, name) == 0) {
      return v->u.object.values[i].value;
    }
  }
  return NULL;
}

static json_value*
js_by_idx(const json_value* v, size_t idx)
{
  if(v->type != json_array || idx >= v->u.array.length) {
    return NULL;
  }
  return v->u.array.values[idx];
}

static size_t
arrlen(const json_value* v)
{
  assert(v->type == json_array);
  return v->u.array.length;
}

static int64_t
js_int(const json_value* v)
{
  assert(v->type == json_integer);
  return v->u.integer;
}
static double
js_double(const json_value* v)
{
  assert(v->type == json_double);
  return v->u.dbl;
}

static const char*
js_string(const json_value* v)
{
  assert(v->type == json_string);
  return v->u.string.ptr;
}

/* dereferences the first first json_object.  only valid when there is a single
 * object */
static const json_value*
objderef(const json_value* v)
{
  assert(v->type == json_object);
  assert(v->u.object.length == 1);
  return v->u.object.values[0].value;
}

/* expects a json_array for which each element is an object that maps to an
 * integer.  in short:
 *    [ {"i": 1}, {"i": 19}, {"i": 6} ]
 * would return the array {1, 19, 6}.  "i" is superfluous; the actual object
 * name is completely ignored.
 * This completely breaks if the 'i' above maps to anything but a basic
 * integer. */
#if 0
MALLOC static int64_t*
arr_to_i64v(const json_value* v)
{
  assert(v->type == json_array);
  const size_t len = arrlen(v);
  int64_t* values = calloc(len, sizeof(int64_t));
  for(size_t i=0; i < len; ++i) {
    const json_value* integer = objderef(js_by_idx(v, i));
    assert(integer->type == json_integer);
    values[i] = js_int(integer);
  }
  return values;
}
#endif
MALLOC static double*
arr_to_lfv(const json_value* v)
{
  assert(v->type == json_array);
  const size_t len = arrlen(v);
  double* values = calloc(len, sizeof(double));
  for(size_t i=0; i < len; ++i) {
    const json_value* dbl = objderef(js_by_idx(v, i));
    assert(dbl->type == json_double);
    values[i] = js_double(dbl);
  }
  return values;
}

/* Given the root of the json tree, parse out the "dims" setting.  Returns an
 * (allocated) array of the dimensions, and sets the 'ndims' parameter to the
 * size of the array. */
MALLOC size_t*
js_dimensions(const json_value* root, size_t* ndims)
{
  const json_value* dims = js_by_name(root, "dims");
  if(dims == NULL) {
    ERR(jsdd, "json has no 'dims' field!");
    return NULL;
  }
  size_t* retval = jsarr_to_zuv(dims);
  if(retval == NULL) {
    ERR(jsdd, "error parsing dimensions; giving up.");
    return NULL;
  }
  *ndims = arrlen(dims);

  return retval;
}

enum dtype
js_datatype(const json_value* root)
{
  const json_value* type = js_by_name(root, "type");
  if(type == NULL) {
    ERR(jsdd, "No 'type' in json; don't know data type");
    assert(false);
    return GARBAGE;
  }
  const char* str = js_string(type);
  if(strcasecmp(str, "float32") == 0) {
    return FLOAT32;
  } else if(strcasecmp(str, "float64") == 0) {
    return FLOAT64;
  } else if(strcasecmp(str, "byte") == 0 || strcasecmp(str, "char") == 0) {
    return BYTE;
  }
  assert(false);
  return GARBAGE;
}
MALLOC double**
js_coord_arrays(const json_value* root, const size_t* dims, size_t ndims)
{
  assert(root);
  const json_value* jscoords = js_by_name(root, "coords");
  if(jscoords == NULL) {
    ERR(jsdd, "no 'coords' array in json.");
    return NULL;
  }
  if(arrlen(jscoords) != ndims) {
    ERR(jsdd, "'coords' and 'dims' arrays disagree on dimensionality "
        "(%zu versus %zu)", arrlen(jscoords), ndims);
    return NULL;
  }
  assert(ndims > 0);
  for(size_t i=0; i < ndims; ++i) {
    const json_value* jscoordi = objderef(js_by_idx(jscoords, i));
    if(arrlen(jscoordi) != dims[i]) {
      ERR(jsdd, "wrong number of coordinates (%zu) for dimension %zu "
          "(should be %zu)", arrlen(jscoordi), i, dims[i]);
      return NULL;
    }
  }
  TRACE(jsdd, "loading %zu-dimensional coord arrays.", ndims);
  double** rv = calloc(ndims, sizeof(double*));
  for(size_t i=0; i < ndims; ++i) {
    const json_value* jscoordi = objderef(js_by_idx(jscoords, i));
    rv[i] = arr_to_lfv(jscoordi);
  }
  return rv;
}
