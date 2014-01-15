#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "vis.h"

#ifndef NDEBUG
# define LOG(x, ...) \
  do { \
    if(logstream) { \
      fprintf(logstream, x, __VA_ARGS__); \
    } else { \
      fprintf(stderr, x, __VA_ARGS__); \
    } \
  } while(0)
#else
# define LOG(x, ...) /* log function removed for release */
#endif

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
static int pid = 0;
static FILE* logstream;

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

__attribute__((constructor)) static void
fp_init()
{
  fprintf(stderr, "[%d] initializing function pointers.\n", (int)getpid());
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

  pid = (int)getpid();
  logstream = stderr;
  const char* logfile = getenv("LIBSITU_LOGFILE");
  if(logfile != NULL) {
    logstream = fopen(logfile, "w");
    if(logstream == NULL) {
      fprintf(stderr, "error opening log file '%s'\n", logfile);
      logstream = stderr;
    }
  }
}

__attribute__((destructor)) static void
fp_finalize()
{
  if(logstream != stderr) {
    if(fclose(logstream) != 0) {
      fprintf(stderr, "[%d] error closing log file! log probably truncated.\n",
              pid);
    }
  }
}

FILE*
fopen(const char* name, const char* mode)
{
  if(mode == NULL) { errno = EINVAL; return NULL; }
  /* are they opening it read-only?  It's not a simulation output, then. */
  if(mode[0] == 'r') {
    LOG("[%d] open for %s is read-only; ignoring it.\n", pid, name);
    /* in that case, skip it, we don't want to keep track of it. */
    return fopenf(name, mode);
  }
  /* what about a temp file?  probably don't want to visualize those. */
  if(strncmp(name, "/tmp", 4) == 0) {
    LOG("[%d] opening temp file (%s); ignoring.\n", pid, name);
    return fopenf(name, mode);
  }
  LOG("[%d] opening %s\n", pid, name);

  /* need an empty entry in the table to store the return value. */
  struct openfile* of = of_find(files, fp_of, NULL);
  if(of == NULL) {
    fprintf(stderr, "[%d] out of open files.  skipping '%s'\n", pid, name);
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
  LOG("[%d] closing %p\n", pid, fp);
  struct openfile* of = of_find(files, fp_of, fp);
  if(of == NULL) {
    fprintf(stderr, "[%d] I don't know %p.  Ignoring for in-situ.\n", pid, fp);
    return fclosef(fp);
  }
  assert(of->fp == fp);
  assert(of->name != NULL);
  const int rv = fclosef(fp);
  if(rv != 0) {
    LOG("[%d] close failure! (%d)\n", pid, rv);
    /* bail without doing vis; the file is invalid anyway, and the vis tool's
     * error message[s] will just obscure ours. */
    of->fp = NULL; free(of->name);
    return rv;
  }
  launch_vis(of->name, logstream);
  of->fp = NULL;
  free(of->name);
  return rv;
}

int
open(const char* fn, int flags, ...)
{
  if((!(flags & O_RDWR) && !(flags & O_WRONLY)) ||
     strncmp(fn, "/tmp", 4) == 0 ||
     strncmp(fn, "/dev", 4) == 0) {
    va_list aq;
    va_start(aq, flags);
    fprintf(stderr, "[%d] %s opened, but ignored by policy.\n", pid, fn);
    const int des = openf(fn, flags, aq);
    va_end(aq);
    return des;
  }
  va_list aq;
  va_start(aq, flags);
  fprintf(stderr, "[%d] posix-opening %s ...", pid, fn);
  const int des = openf(fn, flags, aq);
  va_end(aq);
  if(des <= 0) { /* open failed; ignoring this file. */
    fprintf(stderr, "[%d] ignoring due to open failure\n", pid);
    return des;
  }
  fprintf(stderr, " -> %d\n", des);

  /* need an empty entry in the table to store the return value. */
  int empty = 0;
  struct openposixfile* of = ofposix_find(posix_files, fd_of, &empty);
  if(of == NULL) {
    fprintf(stderr, "[%d] out of open files.  skipping '%s'\n", pid, fn);
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
    /*fprintf(stderr, "[%d] FD %d unknown, ignoring for in situ\n", pid, fd);*/
    return writef(fd, buf, sz);
  }
  fprintf(stderr, "[%d] writing %zu bytes to %d\n", pid, sz, fd);
  return writef(fd, buf, sz);
}

int
close(int des)
{
  struct openposixfile* of = ofposix_find(posix_files, fd_of, &des);
  if(of == NULL) {
    LOG("[%d] don't know FD %d; skipping 'close' instrumentation.\n", pid, des);
    return closef(des);
  }
  assert(of->name != NULL);
  assert(of->fd == des);
  LOG("[%d] closing %s (FD %d)\n", pid, of->name, des);
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
  fprintf(stderr, "[%d] mpi_file_open(%s, %d)\n", pid, filename, amode);
  assert(mpi_file_openf != NULL);
  #define MPI_MODE_WRONLY 4 /* hack! */
  if((amode & MPI_MODE_WRONLY) == 0) {
    fprintf(stderr, "[%d] simulation output would probably be write-only.  "
            "File %s was opened %d, and so we are ignoring it for in-situ.\n",
            pid, filename, amode);
    return mpi_file_openf(comm, filename, amode, info, fh);
  }
  /* find an empty spot in the table for this file. */
  struct openmpifile* of = (struct openmpifile*) of_find(
    (struct openfile*)mpifiles, fp_of, NULL
  );
  if(of == NULL) { /* table full?  then just ignore it. */
    fprintf(stderr, "[%d] out of open mpi files.  skipping '%s'\n", pid,
            filename);
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
