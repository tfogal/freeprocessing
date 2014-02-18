/* JSON data descriptor.  Reads data descriptions from a particular kind of
 * json. */
#ifndef TJF_JSDD_H
#define TJF_JSDD_H

#include <inttypes.h>
#include "../compiler.h"
#include "dtd.h"
#include "json.h"

/* Given the root of the json tree, parse out the "dims" setting.  Returns an
 * (allocated) array of the dimensions, and sets the 'ndims' parameter to the
 * size of the array. */
MALLOC size_t* js_dimensions(const json_value* root, size_t* ndims);

/* Figures out the datatype from the JSON parse. */
enum dtype js_datatype(const json_value* root);
MALLOC int64_t** js_coord_arrays(const json_value* root, const size_t* dims,
                                 size_t ndims);
#endif
