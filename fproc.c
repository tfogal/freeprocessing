#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "fproc.h"

DECLARE_CHANNEL(freeproc);

static bool
patternmatch(const struct teelib* tl, const char* match)
{
  return fnmatch(tl->pattern, match, 0) == 0;
}

struct teelib*
load_processor(FILE* from)
{
  struct teelib* lib = calloc(1, sizeof(struct teelib));
  char* libname = NULL;
  /* we're trying to parse something like this:
   * '*stuff* { exec: libnetz.so }' */
  int m = fscanf(from, "%ms { exec: %ms }", &lib->pattern, &libname);
  if(m == -1 && feof(from)) {
    free(lib); lib = NULL;
    return NULL;
  }
  if(m != 2) {
    ERR(freeproc, "only matched %d, error loading processors (%d)", m, errno);
    free(lib); lib = NULL;
    return NULL;
  }
  TRACE(freeproc, "processor '%s' { %s }", lib->pattern, libname);
  dlerror();
  lib->lib = dlopen(libname, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
  if(NULL == lib->lib) {
    ERR(freeproc, "failed loading processor for %s ('%s')", lib->pattern,
        libname);
    free(lib); lib = NULL;
    free(libname); libname = NULL;
    return NULL;
  }
  dlerror();
  lib->transfer = dlsym(lib->lib, "exec");
  if(NULL == lib->transfer) {
    ERR(freeproc, "failed loading 'exec' function from %s: %s", libname,
        dlerror());
    free(lib); lib = NULL;
    free(libname); libname = NULL;
    return NULL;
  }
  dlerror();
  lib->finish = dlsym(lib->lib, "finish");
  if(NULL == lib->finish) {
    /* just a warning; this function isn't required. */
    WARN(freeproc, "failed loading 'finish' function from %s: %s", libname,
         dlerror());
  }
  dlerror();
  lib->gridsize = dlsym(lib->lib, "grid_size");
  if(NULL == lib->finish) {
    /* just a warning; this function isn't required. */
    WARN(freeproc, "failed loading 'grid_size' function from %s: %s", libname,
         dlerror());
  }
  free(libname); libname = NULL;
  return lib;
}

void
load_processors(struct teelib* tlibs, const char* cfgfile)
{
  FILE* fp = fopen(cfgfile, "r");
  if(fp == NULL) { /* no config file; not much to do, then. */
    WARN(freeproc, "config file '%s' not available, no situ to be done.",
         cfgfile);
    return;
  }

  for(size_t i=0; !feof(fp) && i < MAX_FREEPROCS; ++i) {
    struct teelib* tl = load_processor(fp);
    if(ferror(fp)) { ERR(freeproc, "ferr in '%s' cfg", cfgfile); }
    if(tl == NULL && !feof(fp)) {
      WARN(freeproc, "loading processor failed, giving up.");
      break;
    }
    if(feof(fp)) { break; }
    TRACE(freeproc, "copying....");
    tlibs[i] = *tl;
    TRACE(freeproc, "done!");
    free(tl);
  }

  fclose(fp);
}

void
unload_processors(struct teelib* tlibs)
{
  for(size_t i=0; i < MAX_FREEPROCS; ++i) {
    if(tlibs[i].pattern != NULL) {
      assert(tlibs[i].lib); /* can't have pattern w/o lib. */
      if(dlclose(tlibs[i].lib) != 0) {
        WARN(freeproc, "error closing '%s' library.", tlibs[i].pattern);
      }
      free(tlibs[i].pattern);
      tlibs[i].pattern = NULL;
    }
  }
}

bool
matches(const struct teelib* tlibs, const char* ptrn)
{
  for(size_t i=0; i < MAX_FREEPROCS && tlibs[i].pattern; ++i) {
    if(patternmatch(&tlibs[i], ptrn)) {
      return true;
    }
  }
  return false;
}

void
stream(const struct teelib* tlibs, const char* ptrn, const void* buf,
       const size_t n)
{
  for(size_t i=0; i < MAX_FREEPROCS && tlibs[i].pattern; ++i) {
    if(patternmatch(&tlibs[i], ptrn)) {
      tlibs[i].transfer(ptrn, buf, n);
    }
  }
}

void
gridsize(const struct teelib* tlibs, const char* ptrn, const size_t dims[3])
{
  for(size_t i=0; i < MAX_FREEPROCS && tlibs[i].pattern; ++i) {
    if(patternmatch(&tlibs[i], ptrn)) {
      if(tlibs[i].gridsize) {
        tlibs[i].gridsize(ptrn, dims);
      }
    }
  }
}

void
finish(const struct teelib* tlibs, const char* ptrn)
{
  for(size_t i=0; i < MAX_FREEPROCS && tlibs[i].pattern; ++i) {
    const struct teelib* tl = &tlibs[i];
    if(patternmatch(tl, ptrn)) {
      tl->finish(ptrn);
    }
  }
}
