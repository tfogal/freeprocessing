#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "debug.h"
#include "fproc.h"

DECLARE_CHANNEL(generic);
DECLARE_CHANNEL(opens);
DECLARE_CHANNEL(writes);

typedef FILE* (fopenfqn)(const char*, const char*);
typedef size_t (fwritefqn)(const void*, size_t, size_t, FILE*);
typedef int (fclosefqn)(FILE*);

typedef void* MPI_Comm;
typedef void* MPI_Info;
typedef void* MPI_File;
typedef int (mpi_file_openfqn)(MPI_Comm, char*, int, MPI_Info, MPI_File*);

static fopenfqn* fopenf = NULL;
static fwritefqn* fwritef = NULL;
static fclosefqn* fclosef = NULL;
static mpi_file_openfqn* mpi_file_openf = NULL;

/* When the file is closed, we need the filename so we can pass it to the vis
 * code.  But we're only given the filename on open, not close.  This table
 * maps FILE*s to filenames, which we populate on open.
 * \note This is *not* currently thread-safe. */
struct openfile {
  char* name;
  FILE* fp;
};
struct openmpifile {
  char* name;
  MPI_File* fp;
};
#define MAX_FILES 1024
static const size_t N_SPACES = MAX_FILES;
static struct openfile files[MAX_FILES] = {{NULL, NULL}};
static struct openmpifile mpifiles[MAX_FILES] = {{NULL, NULL}};

typedef int (ofpredicate)(const struct openfile*, const void*);

/* find the openfile in the 'arr'ay which matches the 'p'redicate.
 * 'userdata' is the 'p'redicate's second argument */
static struct openfile*
of_find(struct openfile* arr, ofpredicate * const p, const void* userdata)
{
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(p(&arr[i], userdata)) {
      return &arr[i];
    }
  }
  return NULL;
}
static int
fp_of(const struct openfile* f, const void* fp)
{
  return f->fp == fp;
}

__attribute__((destructor)) static void
free_processors() /* ha, ha */
{
  unload_processors(transferlibs);
}

__attribute__((constructor(200))) static void
fp_init()
{
  /* make sure we don't instrument any more children. */
  unsetenv("LD_PRELOAD");

  TRACE(generic, "[%ld (%ld)] loading fqn pointers..", (long)getpid(),
        (long)getppid());
  fopenf = dlsym(RTLD_NEXT, "fopen");
  fwritef = dlsym(RTLD_NEXT, "fwrite");
  fclosef = dlsym(RTLD_NEXT, "fclose");
  mpi_file_openf = dlsym(RTLD_NEXT, "MPI_File_open");
  assert(fopenf != NULL);
  assert(fclosef != NULL);

  /* look for a config file and load libraries. */
  FILE* cfg = fopen("situ.cfg", "r");
  if(cfg == NULL) {
    const char* home = getenv("HOME");
    char situcfg[512];
    snprintf(situcfg, 512, "%s/.situ/situ.cfg", home);
    cfg = fopen(situcfg, "r");
  }
  if(cfg == NULL) {
    WARN(opens, "could not find a 'situ.cfg'; will not apply any processing.");
    return;
  }
  load_processors(transferlibs, cfg);
  fclose(cfg);
}

FILE*
fopen(const char* name, const char* mode)
{
  if(mode == NULL) { errno = EINVAL; return NULL; }
  if(mode[0] == 'r' || strncmp(name, "/tmp", 4) == 0 ||
     !matches(transferlibs, name)) {
    TRACE(opens, "%s ignored by policy.", name);
    return fopenf(name, mode);
  }
  TRACE(opens, "opening %s", name);

  /* need an empty entry in the table to store the return value. */
  struct openfile* of = of_find(files, fp_of, NULL);
  if(of == NULL) {
    WARN(opens, "internal table overflow.  skipping '%s'", name);
    return fopenf(name, mode);
  }
  assert(of->fp == NULL);
  of->name = strdup(name);
  of->fp = fopenf(name, mode);
  /* if the open failed, then we have some spare memory lying around for the
   * filename.  It'll never get freed, because an "empty entry" in the table is
   * just a NULL fp, and so we can't differentiate between the cases.  So free
   * up our string now. */
  if(of->fp == NULL) {
    free(of->name); of->name = NULL;
  }
  file(transferlibs, name);
  return of->fp;
}

size_t
fwrite(const void* buf, size_t n, size_t nmemb, FILE* fp)
{
  assert(fwritef != NULL);
  struct openfile* of = of_find(files, fp_of, fp);
  if(of == NULL) {
    TRACE(writes, "I don't know %p.  Ignoring.", fp);
    return fwritef(buf, n, nmemb, fp);
  }
  stream(transferlibs, of->name, buf, n*nmemb);
  TRACE(writes, "writing %zu*%zu bytes to %s", n,nmemb, of->name);
  return fwritef(buf, n, nmemb, fp);
}

int
fclose(FILE* fp)
{
  /* only happens if fqn pointers don't load. */
  if(fclosef == NULL) { fp_init(); }
  struct openfile* of = of_find(files, fp_of, fp);
  if(of == NULL) {
    TRACE(opens, "I don't know %p.  Ignoring.", fp);
    return fclosef(fp);
  }
  assert(of->fp == fp);
  assert(of->name != NULL);
  TRACE(opens, "fclosing %p (%s)", fp, of->name);
  const int rv = fclosef(fp);
  if(rv != 0) {
    WARN(opens, "close failure! (%d)", rv);
  }
  finish(transferlibs, of->name);
  of->fp = NULL;
  free(of->name);
  of->name = NULL;
  return rv;
}

int MPI_File_open(MPI_Comm comm, char* filename, int amode,
                  MPI_Info info, MPI_File* fh)
{
  TRACE(opens, "mpi_file_open(%s, %d)", filename, amode);
  assert(mpi_file_openf != NULL);
  #define MPI_MODE_WRONLY 4 /* hack! */
  WARN(opens, "write detection is based on OMPI-specific hack..");
  if((amode & MPI_MODE_WRONLY) == 0) {
    TRACE(opens, "simulation output would probably be write-only.  "
          "File %s was opened %d, and so we are ignoring it for in-situ.",
          filename, amode);
    return mpi_file_openf(comm, filename, amode, info, fh);
  }
  /* find an empty spot in the table for this file. */
  struct openmpifile* of = (struct openmpifile*) of_find(
    (struct openfile*)mpifiles, fp_of, NULL
  );
  if(of == NULL) { /* table full?  then just ignore it. */
    WARN(opens, "out of open mpi files.  skipping '%s'", filename);
    return mpi_file_openf(comm, filename, amode, info, fh);
  }
  assert(of->fp == NULL);
  of->name = strdup(filename);
  const int rv = mpi_file_openf(comm, filename, amode, info, fh);
  of->fp = fh;
  if(of->fp == NULL) {
    free(of->name); of->name = NULL;
  }

  return rv;
}
