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
#include "fproc.h"

DECLARE_CHANNEL(generic);
DECLARE_CHANNEL(opens);
DECLARE_CHANNEL(writes);
DECLARE_CHANNEL(hdf5);

/* types for HDF5 */
typedef int herr_t;
typedef int hid_t;
typedef unsigned long long hsize_t;

typedef FILE* (fopenfqn)(const char*, const char*);
typedef size_t (fwritefqn)(const void*, size_t, size_t, FILE*);
typedef int (fclosefqn)(FILE*);
typedef int (openfqn)(const char*, int, ...);
typedef ssize_t (writefqn)(int, const void*, size_t);
typedef int (closefqn)(int);
typedef hid_t h5fcreatefqn(const char*, unsigned, hid_t, hid_t);
typedef hid_t h5gcreate1fqn(hid_t, const char*, size_t);
typedef hid_t h5screate_simplefqn(int, const hsize_t*, const hsize_t*);
typedef herr_t h5sclosefqn(hid_t);
typedef hid_t h5dopen1fqn(hid_t, const char*);
typedef hid_t h5dopen2fqn(hid_t, const char*, hid_t);
typedef herr_t h5dclosefqn(hid_t);
typedef hid_t h5dcreate1fqn(hid_t, const char*, hid_t, hid_t, hid_t);
typedef hid_t h5dcreate2fqn(hid_t, const char*, hid_t, hid_t, hid_t);
typedef herr_t h5dwritefqn(hid_t, hid_t, hid_t, hid_t, hid_t, const void*);

typedef void* MPI_Comm;
typedef void* MPI_Info;
typedef void* MPI_File;
typedef int (mpi_file_openfqn)(MPI_Comm, char*, int, MPI_Info, MPI_File*);

static openfqn* openf = NULL;
static writefqn* writef = NULL;
static closefqn* closef = NULL;
static fopenfqn* fopenf = NULL;
static fwritefqn* fwritef = NULL;
static fclosefqn* fclosef = NULL;
static mpi_file_openfqn* mpi_file_openf = NULL;
static h5fcreatefqn* h5fcreatef = NULL;
static h5gcreate1fqn* h5gcreate1f = NULL;
static h5screate_simplefqn* h5screatesimplef = NULL;
static h5sclosefqn* h5sclosef = NULL;
static h5dopen1fqn* h5dopen1f  = NULL;
static h5dopen2fqn* h5dopen2f  = NULL;
static h5dclosefqn* h5dclosef = NULL;
static h5dcreate1fqn* h5dcreate1f = NULL;
static h5dcreate2fqn* h5dcreate2f = NULL;
static h5dwritefqn* h5dwritef = NULL;

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
struct openhdffile {
  char* name;
  size_t dims[3]; /* dims of the next impending write */
  size_t sz; /* sizeof() the type being written */
};
struct h5spaces {
  hid_t id;
  size_t dims[3];
};
struct h5meta {
  hid_t dset;
  hid_t space;
  char* name;
};
#define MAX_FILES 1024
static const size_t N_SPACES = MAX_FILES;
static struct openfile files[MAX_FILES] = {{NULL, NULL}};
static struct openposixfile posix_files[MAX_FILES] = {{NULL, 0}};
static struct openmpifile mpifiles[MAX_FILES] = {{NULL, NULL}};
static struct h5spaces spaces[MAX_FILES] = {{0, {0,0,0}}};
static struct h5meta metah5[MAX_FILES] = {{0, 0, NULL}};

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

static struct teelib transferlibs[MAX_FREEPROCS] = {{NULL,NULL,NULL,NULL,NULL}};

__attribute__((destructor)) static void
free_processors() /* ha, ha */
{
  unload_processors(transferlibs);
}

__attribute__((constructor(200))) static void
fp_init()
{
  TRACE(generic, "[%ld (%ld)] loading fqn pointers..", (long)getpid(),
        (long)getppid());
  openf = dlsym(RTLD_NEXT, "open");
  writef = dlsym(RTLD_NEXT, "write");
  closef = dlsym(RTLD_NEXT, "close");
  fopenf = dlsym(RTLD_NEXT, "fopen");
  fwritef = dlsym(RTLD_NEXT, "fwrite");
  fclosef = dlsym(RTLD_NEXT, "fclose");
  mpi_file_openf = dlsym(RTLD_NEXT, "MPI_File_open");
  h5fcreatef = dlsym(RTLD_NEXT, "H5Fcreate");
  h5gcreate1f = dlsym(RTLD_NEXT, "H5Gcreate1");
  h5screatesimplef = dlsym(RTLD_NEXT, "H5Screate_simple");
  h5sclosef = dlsym(RTLD_NEXT, "H5Sclose");
  h5dopen1f  = dlsym(RTLD_NEXT, "H5Dopen1");
  h5dopen2f  = dlsym(RTLD_NEXT, "H5Dopen2");
  h5dclosef = dlsym(RTLD_NEXT, "H5Dclose");
  h5dcreate1f = dlsym(RTLD_NEXT, "H5Dcreate1");
  h5dcreate2f = dlsym(RTLD_NEXT, "H5Dcreate2");
  h5dwritef = dlsym(RTLD_NEXT, "H5Dwrite");
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
  if(strncmp(name, "/tmp", 4) == 0 || !matches(transferlibs, name)) {
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
  return of->fp;
}

size_t
fwrite(const void* buf, size_t n, size_t nmemb, FILE* fp)
{
  assert(fwritef != NULL);
  struct openfile* of = of_find(files, fp_of, fp);
  if(of == NULL) {
    TRACE(opens, "I don't know %p.  Ignoring for in-situ.", fp);
    return fwritef(buf, n, nmemb, fp);
  }
  stream(transferlibs, of->name, buf, n*nmemb);
  TRACE(writes, "writing %zu*%zu bytes to %s", n,nmemb, of->name);
  return fwritef(buf, n, nmemb, fp);
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
  }
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
  if((!(flags & O_RDWR) && !(flags & O_WRONLY)) || !matches(transferlibs, fn) ||
     strncmp(fn, "/tmp", 4) == 0 ||
     strncmp(fn, "/dev", 4) == 0) {
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
  TRACE(writes, "writing %zu bytes to %d", sz, fd);
  stream(transferlibs, of->name, buf, sz);
  /* It will cause us a lot of problems if a write ends up being short, and the
   * application then resubmits the next part of the partial write.  So,
   * iterate and make sure we avoid any partial writes. */
  {
    ssize_t written = 0;
    do {
      errno=0;
      ssize_t bytes = writef(fd, ((const char*)buf)+written, sz-written);
      if(bytes == -1 && errno == EINTR) { continue; }
      if(bytes == -1 && written == 0) { return -1; }
      if(bytes == -1) { return written; }
      written += bytes;
    } while((size_t)written < sz);
    return written;
  }
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

  finish(transferlibs, of->name);

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

hid_t
H5Fcreate(const char* name, unsigned flags, hid_t fcpl, hid_t fapl)
{
  const hid_t rv = h5fcreatef(name, flags, fcpl, fapl);
  TRACE(hdf5, "(%s, %u, %d, %d) = %d", name, flags, fcpl, fapl, rv);
  return rv;
}

hid_t
H5Gcreate1(hid_t loc, const char* name, size_t hint)
{
  const hid_t rv = h5gcreate1f(loc, name, hint);
  TRACE(hdf5, "(%d, %s, %zu) = %d", loc, name, hint, rv);
  return rv;
}

hid_t
H5Screate_simple(int rank, const hsize_t* dims, const hsize_t* maxdims)
{
  assert(h5screatesimplef);
  const hid_t rv = h5screatesimplef(rank, dims, maxdims);
  char dstr[256];
  char mstr[256];
  strcpy(dstr, "[");
  strcpy(mstr, "[");
  for(int i=0; i < rank; ++i) {
    char s[32];
    snprintf(s, 32, "%llu ", dims[i]);
    strncat(dstr, s, 32);
    if(maxdims) { /* maxdims can be null */
      snprintf(s, 32, "%llu ", maxdims[i]);
      strncat(mstr, s, 32);
    }
  }
  strncat(dstr, "]", 2);
  strncat(mstr, "]", 2);
  TRACE(hdf5, "(%d, %s, %s) = %d", rank, dstr, mstr, rv);
  /* for now, we only care about 3D data; so skip if the rank is small. */
  for(size_t i=0; rank >= 3 && i < N_SPACES; ++i) {
    if(spaces[i].id == 0 || spaces[i].id == -1) {
      spaces[i].id = rv;
      assert(rank >= 3);
      spaces[i].dims[0] = dims[0];
      spaces[i].dims[1] = dims[1];
      spaces[i].dims[2] = dims[2];
    }
  }
  return rv;
}

herr_t
H5Sclose(hid_t id)
{
  const herr_t rv = h5sclosef(id);
  assert(id > 0);
  for(size_t i=0; i < N_SPACES; ++i) {
    if(spaces[i].id == id) {
      TRACE(hdf5, "closing data space %d", id);
      spaces[i].id = -1;
      spaces[i].dims[0] = spaces[i].dims[1] = spaces[i].dims[2] = 0;
      break;
    }
  }
  return rv;
}

hid_t
H5Dopen1(hid_t loc, const char* name)
{
  TRACE(hdf5, "(%d, %s)", loc, name);
  return h5dopen1f(loc, name);
}

hid_t
H5Dopen2(hid_t loc, const char* name, hid_t dapl)
{
  TRACE(hdf5, "(%d, %s, %d)", loc, name, dapl);
  return h5dopen2f(loc, name, dapl);
}

static size_t
find_dataspace(hid_t id, const struct h5spaces* sp, size_t n)
{
  for(size_t i=0; i < n; ++i) {
    if(sp[i].id == id) {
      return i;
    }
  }
  return n;
}

hid_t
H5Dcreate1(hid_t loc, const char *name, hid_t type, hid_t space, hid_t dcpl)
{
  hid_t rv = h5dcreate1f(loc, name, type, space, dcpl);
  TRACE(hdf5, "(%d, %s, %d, %d, %d) = %d",loc, name, type, space, dcpl, rv);
  const size_t sid = find_dataspace(space, spaces, N_SPACES);
  if(sid == N_SPACES) { /* not found */
    return rv;
  }
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(metah5[i].dset <= 0 && metah5[i].space <= 0) {
      assert(metah5[i].name == NULL);
      metah5[i].dset = rv;
      metah5[i].space = space;
      metah5[i].name = strdup(name);
      break;
    }
  }
  return rv;
}

hid_t
H5Dcreate2(hid_t loc, const char* name, hid_t type, hid_t space, hid_t dcpl)
{
  TRACE(hdf5, "(%d, %s, %d, %d, %d)",loc, name, type, space, dcpl);
  return h5dcreate2f(loc, name, type, space, dcpl);
}

herr_t
H5Dclose(hid_t id)
{
  /* there's an issue here that if two data sets are open with different names,
   * we close the first one that matches the ID, which might be the other
   * dataset.
   * this hasn't been an issue for the software we've instrumented so far... */
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(metah5[i].dset == id) {
      TRACE(hdf5, "cleaning up dset %d (%s)", id, metah5[i].name);
      free(metah5[i].name);
      metah5[i].name = NULL;
      metah5[i].dset = -1;
      metah5[i].space = -1;
      break;
    }
  }
  return h5dclosef(id);
}

herr_t
H5Dwrite(hid_t dset, hid_t memtype, hid_t memspace, hid_t filespace,
         hid_t plist, const void* buf)
{
  TRACE(hdf5, "(%d, %d, %d, %d, %d, %p)", dset, memtype, memspace,
        filespace, plist, buf);
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(metah5[i].dset == dset) {
      const size_t sidx = find_dataspace(metah5[i].space, spaces, N_SPACES);
      TRACE(hdf5, "h-write %s [%zu %zu %zu] %p", metah5[i].name,
            spaces[sidx].dims[0], spaces[sidx].dims[1], spaces[sidx].dims[2],
            buf);
      gridsize(transferlibs, metah5[i].name, spaces[sidx].dims);
      /* sizeof(double) is a hack; TODO pull it from HDF5. */
      const size_t n = spaces[sidx].dims[0] * spaces[sidx].dims[1] *
                       spaces[sidx].dims[2] * sizeof(double);
      stream(transferlibs, metah5[i].name, buf, n);
      break;
    }
  }
  return h5dwritef(dset, memtype, memspace, filespace, plist, buf);
}
