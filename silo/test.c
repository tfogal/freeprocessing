#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../compiler.h"
#include "../debug.h"
#include "jsdd.h"
#include "json.h"

DECLARE_CHANNEL(silo);

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

int
main()
{
  char* cfgtext = slurp("silo.cfg");
  char* err = calloc(2048, sizeof(char));
  json_settings settings = { 0 };
  json_value* js = json_parse_ex(&settings, cfgtext, strlen(cfgtext), err);
  free(cfgtext);
  if(js == NULL) {
    ERR(silo, "Parse error: %s", err);
    free(err);
    return 1;
  }
  TRACE(silo, "type: %d", js->type);
  TRACE(silo, "none (%d), obj (%d), array (%d), integer (%d), double (%d), "
        "string (%d), bool (%d), null (%d)", json_none, json_object,
        json_array, json_integer, json_double, json_string, json_boolean,
        json_null);

  size_t ndims;
  size_t* dimens = js_dimensions(js, &ndims);

  printf("dtype: %d\n", (int)js_datatype(js));
  double** mycoords = js_coord_arrays(js, dimens, ndims);
  assert(mycoords);

  for(size_t i=0; i < ndims; ++i) { free(mycoords[i]); }
  free(mycoords);
  free(dimens);
  free(err);
  json_value_free(js);
  return 0;
}
