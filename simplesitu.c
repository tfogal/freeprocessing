#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "vis.h"
#include "debug.h"

DECLARE_CHANNEL(generic);
DECLARE_CHANNEL(opens);
DECLARE_CHANNEL(writes);

typedef FILE* (fopenfqn)(const char*, const char*);
typedef int (fclosefqn)(FILE*);
typedef int (openfqn)(const char*, int, ...);
typedef ssize_t (writefqn)(int, const void*, size_t);
typedef int (closefqn)(int);

typedef void* MPI_Comm;
typedef void* MPI_Info;
typedef void* MPI_File;
typedef int (mpi_file_openfqn)(MPI_Comm, char*, int, MPI_Info, MPI_File*);

static openfqn* openf = NULL;
static writefqn* writef = NULL;
static closefqn* closef = NULL;
static fopenfqn* fopenf = NULL;
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
/* keeps track of files opened using the POSIX interface. */
struct openposixfile {
  char* name;
  int fd;
};
struct openmpifile {
  char* name;
  MPI_File* fp;
};
#define MAX_FILES 1024
static struct openfile files[MAX_FILES] = {{NULL, NULL}};
static struct openposixfile posix_files[MAX_FILES] = {{NULL, 0}};
static struct openmpifile mpifiles[MAX_FILES] = {{NULL, NULL}};

typedef int (ofpredicate)(const struct openfile*, const void*);
typedef int (ofposixp)(const struct openposixfile*, const void*);

static struct openposixfile*
ofposix_find(struct openposixfile* arr, ofposixp* p, const void* userdata)
{
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(p(&arr[i], userdata)) {
      return &arr[i];
    }
  }
  return NULL;
}
static int
fd_of(const struct openposixfile* f, const void* fdvoid)
{
  const int* fd = (const int*)fdvoid;
  return f->fd == (*fd);
}

/* find the openfile in the 'arr'ay which matches the 'p'redicate.
 * 'userdata' is the 'p'redicate's second argument */
static struct openfile*
of_find(struct openfile* arr, ofpredicate *p, const void* userdata)
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

enum DataType { FLOAT32=0, FLOAT64, ASCII };
typedef void (tfqn)(unsigned, const size_t dims[3], const void* buf,
                    size_t n);
typedef void (cfqn)(unsigned, const size_t dims[3]);
/* a library to load and exec as we move data, tee-style. */
struct teelib {
  char* pattern;
  enum DataType type;
  size_t dims[3];
  void* lib;
  tfqn* transfer;
  cfqn* finish;
};
/* sizeof(), but takes our type instead of a 'real' type. */
static size_t
nbytes(enum DataType dt)
{
  switch(dt) {
    case FLOAT32: return 4;
    case FLOAT64: return 8;
    case ASCII: return 1;
  }
  assert(false);
  return 0;
}
#define MAX_FREEPROCS 128U
static struct teelib transferlibs[MAX_FREEPROCS] = {{0,0,{1,1,1},NULL,NULL,NULL}};

typedef bool (tlpredicate)(const struct teelib*, const void*);
static bool
patternmatch(const struct teelib* tl, const void* thing)
{
  const char* match = (const char*)thing;
  return fnmatch(tl->pattern, match, 0) == 0;
}

static struct teelib*
tl_find(struct teelib* libs, size_t n, tlpredicate* p,
        const void* userdata)
{
  for(size_t i=0; i < n && libs[i].pattern; ++i) {
    if(p(&libs[i], userdata)) {
      return &libs[i];
    }
  }
  return NULL;
}

static void
load_processors(struct teelib* tlibs, const char* cfgfile)
{
  FILE* fp = fopen(cfgfile, "r");
  if(fp == NULL) { /* no config file; not much to do, then. */
    WARN(opens, "config file '%s' not available, no vis to be done.", cfgfile);
    return;
  }
  if(ferror(fp)) { ERR(opens, "ferr in '%s' cfg", cfgfile); }
  if(feof(fp)) { ERR(opens, "eof in '%s' cfg", cfgfile); }

  FIXME(opens, "loop and load all processors");
  struct teelib* lib = tlibs;
  char* libname;
  char* typename;
  /* we're trying to parse something like this:
   * '*stuff* { type: float32\n\tdims: 24 24 24\n\texec: libwhatever.so }' */
  int m = fscanf(fp, "%ms { type: %ms dims: %zu %zu %zu exec: %ms }",
                 &lib->pattern, &typename,
                 &lib->dims[0], &lib->dims[1], &lib->dims[2], &libname);
  if(m != 6) {
    ERR(opens, "error loading processors (%d)", errno);
  }
  TRACE(opens, "processor '%s' { %s, %zu %zu %zu, %s }", lib->pattern,
        typename, lib->dims[0], lib->dims[1], lib->dims[2], libname);
  dlerror();
  lib->lib = dlopen(libname, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
  if(NULL == lib->lib) {
    ERR(opens, "failed loading processor for %s ('%s')", lib->pattern,
        libname);
  }
  dlerror();
  lib->transfer = dlsym(lib->lib, "exec");
  if(NULL == lib->transfer) {
    ERR(opens, "failed loading 'exec' function from %s: %s", libname,
        dlerror());
  }
  dlerror();
  lib->finish = dlsym(lib->lib, "finish");
  if(NULL == lib->finish) {
    /* just a warning; this function isn't required. */
    WARN(opens, "failed loading 'finish' function from %s: %s", libname,
         dlerror());
  }
  dlerror();
  if(strncasecmp(typename, "float32", 7) == 0) { lib->type = FLOAT32;
  } else if(strncasecmp(typename, "float64", 7) == 0) { lib->type = FLOAT64;
  } else if(strncasecmp(typename, "ascii", 5) == 0) { lib->type = ASCII;
  } else {
    WARN(opens, "unhandled data type '%s' in processor", typename);
  }
  free(typename);
  free(libname);

  fclose(fp);
}

__attribute__((destructor)) static void
free_processors() /* ha, ha */
{
  for(size_t i=0 ; i < MAX_FREEPROCS; ++i) {
    if(transferlibs[i].pattern != NULL) {
      assert(transferlibs[i].lib); /* can't have pattern w/o lib. */
      if(dlclose(transferlibs[i].lib) != 0) {
        WARN(opens, "error closing '%s' library.", transferlibs[i].pattern);
      }
      free(transferlibs[i].pattern);
      transferlibs[i].pattern = NULL;
    }
  }
}

__attribute__((constructor(200))) static void
fp_init()
{
  TRACE(generic, "[%ld (%ld)] loading fqn pointers..\n", (long)getpid(),
        (long)getppid());
  openf = dlsym(RTLD_NEXT, "open");
  writef = dlsym(RTLD_NEXT, "write");
  closef = dlsym(RTLD_NEXT, "close");
  fopenf = dlsym(RTLD_NEXT, "fopen");
  fclosef = dlsym(RTLD_NEXT, "fclose");
  mpi_file_openf = dlsym(RTLD_NEXT, "MPI_File_open");
  assert(fopenf != NULL);
  assert(writef != NULL);
  assert(closef != NULL);
  assert(fclosef != NULL);

  /* look for a config file and load libraries. */
  load_processors(transferlibs, "situ.cfg");

  /* make sure we don't instrument any more children. */
  unsetenv("LD_PRELOAD");
}

FILE*
fopen(const char* name, const char* mode)
{
  if(mode == NULL) { errno = EINVAL; return NULL; }
  /* are they opening it read-only?  It's not a simulation output, then. */
  if(mode[0] == 'r') {
    TRACE(opens, "open for %s read-only; ignoring.", name);
    /* in that case, skip it, we don't want to keep track of it. */
    return fopenf(name, mode);
  }
  /* what about a temp file?  probably don't want to visualize those. */
  if(strncmp(name, "/tmp", 4) == 0) {
    TRACE(opens, "opening temp file (%s); ignoring.\n", name);
    return fopenf(name, mode);
  }
  TRACE(opens, "opening %s\n", name);

  /* need an empty entry in the table to store the return value. */
  struct openfile* of = of_find(files, fp_of, NULL);
  if(of == NULL) {
    WARN(opens, "out of open files.  skipping '%s'", name);
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
  return of->fp;
}

int
fclose(FILE* fp)
{
  assert(fclosef != NULL); /* only happens if fqn pointers don't load. */
  TRACE(opens, "fclosing %p", fp);
  struct openfile* of = of_find(files, fp_of, fp);
  if(of == NULL) {
    TRACE(opens, "I don't know %p.  Ignoring for in-situ.", fp);
    return fclosef(fp);
  }
  assert(of->fp == fp);
  assert(of->name != NULL);
  const int rv = fclosef(fp);
  if(rv != 0) {
    WARN(opens, "close failure! (%d)", rv);
    /* bail without doing vis; the file is invalid anyway, and the vis tool's
     * error message[s] will just obscure ours. */
    of->fp = NULL; free(of->name);
    return rv;
  }
  launch_vis(of->name, stderr);
  of->fp = NULL;
  free(of->name);
  return rv;
}

int
open(const char* fn, int flags, ...)
{
  mode_t mode;
  if(flags & O_CREAT) {
    va_list lst;
    va_start(lst, flags);
    mode = va_arg(lst, mode_t);
    va_end(lst);
  }
  const char* pattern = getenv("LIBSITU_FILENAME");
  if((!(flags & O_RDWR) && !(flags & O_WRONLY)) ||
     strncmp(fn, "/tmp", 4) == 0 ||
     strncmp(fn, "/dev", 4) == 0 ||
     (pattern != NULL && fnmatch(pattern, fn, 0) != 0)) {
    TRACE(opens, "%s opened, but ignored by policy.", fn);
    const int des = openf(fn, flags, mode);
    return des;
  }
  const int des = openf(fn, flags, mode);
  if(des <= 0) { /* open failed; ignoring this file. */
    TRACE(opens, "posix-opening %s ignored due to open failure", fn);
    return des;
  } else {
    TRACE(opens, "posix-opening %s...", fn);
  }

  /* need an empty entry in the table to store the return value. */
  int empty = 0;
  struct openposixfile* of = ofposix_find(posix_files, fd_of, &empty);
  if(of == NULL) {
    WARN(opens, "out of open files.  skipping '%s'", fn);
    return des;
  }
  assert(of->name == NULL);
  of->name = strdup(fn);
  of->fd = des;
  return des;
}

ssize_t
write(int fd, const void *buf, size_t sz)
{
  struct openposixfile* of = ofposix_find(posix_files, fd_of, &fd);
  if(of == NULL) {
    return writef(fd, buf, sz);
  }
  const struct teelib* tl = tl_find(transferlibs, MAX_FREEPROCS, patternmatch,
                                    of->name);
  if(tl) {
    TRACE(writes, "writing %zu bytes to %d", sz, fd);
    tl->transfer(tl->type, tl->dims, buf, sz/nbytes(tl->type));
  }
  return writef(fd, buf, sz);
}

int
close(int des)
{
  struct openposixfile* of = ofposix_find(posix_files, fd_of, &des);
  if(of == NULL) {
    TRACE(opens, "don't know FD %d; skipping 'close' instrumentation.", des);
    return closef(des);
  }
  TRACE(opens, "closing %s (FD %d [%d])", of->name, des, of->fd);
  assert(of->fd == des);
  if(of->name == NULL) { /* don't bother, not a real file? */
    WARN(opens, "null filename!?");
    return closef(des);
  }

  struct teelib* tl = tl_find(transferlibs, MAX_FREEPROCS, patternmatch,
                              of->name);
  if(tl && tl->finish) {
    tl->finish(tl->type, tl->dims);
  }

  { /* free up rid of table entry */
    free(of->name);
    of->name = NULL;
    of->fd = 0;
  }
  return closef(des);
}

int MPI_File_open(MPI_Comm comm, char* filename, int amode,
                  MPI_Info info, MPI_File* fh)
{
  TRACE(opens, "mpi_file_open(%s, %d)", filename, amode);
  assert(mpi_file_openf != NULL);
  #define MPI_MODE_WRONLY 4 /* hack! */
  if((amode & MPI_MODE_WRONLY) == 0) {
    TRACE(opens, "simulation output would probably be write-only.  "
          "File %s was opened %d, and so we are ignoring it for in-situ.\n",
          filename, amode);
    return mpi_file_openf(comm, filename, amode, info, fh);
  }
  /* find an empty spot in the table for this file. */
  struct openmpifile* of = (struct openmpifile*) of_find(
    (struct openfile*)mpifiles, fp_of, NULL
  );
  if(of == NULL) { /* table full?  then just ignore it. */
    WARN(opens, "out of open mpi files.  skipping '%s'\n", filename);
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
