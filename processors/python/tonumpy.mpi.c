/* A simple freeprocessor which forwards its array into numpy and runs a
 * script, for processing there. */
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <fnmatch.h>
#include <stdbool.h>
#include <Python.h>
#include <numpy/arrayobject.h>
#include "debug.h"
#include "parallel.mpi.h"

DECLARE_CHANNEL(py);

/* we need a method table to be able to create a module, but we don't
 * (currently) have any functions to expose.
 * Still, we need the empty method table to make python happy. */
static PyMethodDef FreePyMethods[] = {
  {NULL, NULL, 0, NULL}
};
/* the python module which we'll use to forward our data */
static PyObject* fpmodule = NULL;
/* a dictionary that the user script will pull the data out of.  lives within
 * the 'fpmodule' module. */
static PyObject* fpdict = NULL;
/* have we initialized the FP module yet? */
static bool module_initialized = false;
/* which script we'll run */
static const char* scriptname = "vis.py";
/* script we'll execute.  global so we don't have to open the file every time
 * we do a damn write. */
static FILE* script = NULL;
/* the dimensions of the next write */
static size_t dims[3] = {0};
/* what timestep we're on, based on the heuristic of how many 'close's we've
 * done. */
static size_t timestep = 0;
static enum _datatype { int8=0, int16, int32, int64,
                        uint8, uint16, uint32, uint64,
                        float32, float64 } datatype;

static void
create_module()
{
  assert(fpmodule == NULL); /* i.e. this should only happen once */
  assert(fpdict == NULL);
  _import_array();
  PyErr_PrintEx(0);

  TRACE(py, "creating 'freeprocessing' module...");

  fpmodule = Py_InitModule("freeprocessing", FreePyMethods);
  if(fpmodule == NULL) {
    ERR(py, "error creating freeprocessing module\n");
    return;
  }
  fpdict = PyModule_GetDict(fpmodule);
  if(fpdict == NULL) {
    ERR(py, "error creating freeprocessing module dictionary\n");
    return;
  }
  if(PyRun_SimpleString("import freeprocessing") != 0) {
    ERR(py, "Error importing freeprocessing python module");
  }
  /* Make "freeprocessing.rank" give the rank of the current process. */
  if(0 != PyModule_AddIntConstant(fpmodule, "rank", rank())) {
    WARN(py, "could not add rank information to module.");
  }
  script = fopen(scriptname, "r");
  if(!script) {
    ERR(py, "Error opening script file '%s'", scriptname);
    abort();
  }
  module_initialized = true;
}

__attribute__((destructor(1000))) static void
teardown_py()
{
  if(Py_IsInitialized()) {
    Py_Finalize();
    PyDict_Clear(fpdict);
  }
  fpdict = NULL;
  fpmodule = NULL;
  if(script) { fclose(script); script = NULL; }
  module_initialized = false;
}

void
metadata(const char* fn, const size_t d[3], int dtype)
{
  (void) fn;
  dims[0] = d[0];
  dims[1] = d[1];
  dims[2] = d[2];
  datatype = dtype;
  TRACE(py, "next write will be: %zu x %zu x %zu", dims[0], dims[1], dims[2]);
}

static int
pytype(enum _datatype dt)
{
  switch(dt) {
    case float32: return NPY_FLOAT;
    case float64: return NPY_DOUBLE;
    default: assert(false);
  }
  assert(false);
}

static size_t
typewidth(enum _datatype dt)
{
  switch(dt) {
    case int8: case uint8: return 1;
    case int16: case uint16: return 2;
    case int32: case uint32: return 4;
    case int64: case uint64: return 8;
    case float32: return 4;
    case float64: return 8;
  }
  assert(false);
}

void
exec(const char* fn, const void* buf, size_t n)
{
  /* Skip writes to the .cpu files; we only use those to count the timesteps. */
  if(fnmatch("*cpu?*", fn, 0) == 0) {
    return;
  }
  TRACE(py, "[%zu] write %p to %s: %zu bytes", rank(), buf, fn, n);
  if(!Py_IsInitialized()) {
    WARN(py, "initializing python");
    Py_Initialize();
  }
  if(!module_initialized) {
    create_module();
  }
  PyArrayObject* ds = NULL;

  WARN(py, "write; %zu %zu %zu *8 == %zu (n=%zu)", dims[0],dims[1],dims[2],
       dims[0]*dims[1]*dims[2]*typewidth(datatype), n);
  if(dims[0]*dims[1]*dims[2]*typewidth(datatype) == n) {
    npy_intp npdims[3] = { dims[0], dims[1], dims[2] };
    ds = (PyArrayObject*) PyArray_SimpleNewFromData(3, npdims,
                                                    pytype(datatype),
                                                    (void*)buf);
  } else { /* We don't know the array dimensions.  Skip it. */
    /* .. but first kill our dims, so we don't use them incorrectly later. */
    dims[0] = dims[1] = dims[2] = 0;
    return;
  }
  /* add the field name as an available variable */
  if(0 != PyModule_AddStringConstant(fpmodule, "field", fn)) {
    WARN(py, "could not add field name to module.");
    abort();
  }
  if(0 != PyModule_AddIntConstant(fpmodule, "timestep", timestep)) {
    WARN(py, "could not add timestep information to module.");
  }
  if(0 != PyDict_SetItemString(fpdict, "stream", (PyObject*)ds)) {
    WARN(py, "could not add stream data to dict!");
  }
  TRACE(py, "running '%s'...", scriptname);
  rewind(script);
  if(PyRun_SimpleFile(script, scriptname) != 0) {
    ERR(py, "error running script '%s'", scriptname);
    abort();
  }
}

void
finish(const char* fn)
{
  TRACE(py, "[%zu] done with %s", rank(), fn);
  if(fnmatch("*cpu?*", fn, 0) == 0) {
    ++timestep;
    TRACE(py, "[%zu] timestep is now %zu", rank(), timestep);
  }
}
