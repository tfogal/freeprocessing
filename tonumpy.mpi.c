/* A simple freeprocessor which forwards its array into numpy and runs a
 * script, for processing there. */
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
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
grid_size(const char* fn, const size_t d[3])
{
  (void) fn;
  dims[0] = d[0];
  dims[1] = d[1];
  dims[2] = d[2];
  TRACE(py, "next write will be: %zu x %zu x %zu", dims[0], dims[1], dims[2]);
}

void
exec(const char* fn, const void* buf, size_t n)
{
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
       dims[0]*dims[1]*dims[2]*sizeof(double), n);
  if(dims[0]*dims[1]*dims[2]*sizeof(double) == n) {
    npy_intp npdims[3] = { dims[0], dims[1], dims[2] };
    ds = (PyArrayObject*) PyArray_SimpleNewFromData(3, npdims, NPY_DOUBLE,
                                                    (void*)buf);
  } else { /* We don't know the array dimensions.  Skip it. */
    /* .. but first kill our dims, so we don't use them incorrectly later. */
    dims[0] = dims[1] = dims[2] = 0;
    return;
  }
  if(0 != PyDict_SetItemString(fpdict, "stream", (PyObject*)ds)) {
    fprintf(stderr, "could not add mydata to dict");
  }
  TRACE(py, "running '%s'...", scriptname);
  rewind(script);
  if(PyRun_SimpleFile(script, scriptname) != 0) {
    ERR(py, "error running script '%s'", scriptname);
  }
}

void
finish(const char* fn)
{
  TRACE(py, "[%zu] done with %s", rank(), fn);
}
