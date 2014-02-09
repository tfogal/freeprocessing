#define _GNU_SOURCE 1
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "debug.h"
#include "fproc.h"

DECLARE_CHANNEL(posix);

typedef int (openfqn)(const char*, int, ...);
typedef ssize_t (writefqn)(int, const void*, size_t);
typedef int (closefqn)(int);

static openfqn* openf = NULL;
static writefqn* writef = NULL;
static closefqn* closef = NULL;

/* When the file is closed, we need the filename so we can pass it to the vis
 * code.  But we're only given the filename on open, not close.  This table
 * maps FILE*s to filenames, which we populate on open.
 * \note This is *not* currently thread-safe. */
struct openposixfile {
  char* name;
  int fd;
};
#define MAX_FILES 1024
static struct openposixfile posix_files[MAX_FILES] = {{NULL, 0}};
/* predicate for finding a posix_file.  2nd argument should be a pointer to the
 * file descriptor.*/
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

__attribute__((constructor(201))) static void
fp_posix_init()
{
  TRACE(posix, "[%ld] loading function pointers.", (long)getpid());
  openf = dlsym(RTLD_NEXT, "open");
  writef = dlsym(RTLD_NEXT, "write");
  closef = dlsym(RTLD_NEXT, "close");
  assert(openf != NULL);
  assert(writef != NULL);
  assert(closef != NULL);
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
    TRACE(posix, "%s opened, but ignored by policy.", fn);
    const int des = openf(fn, flags, mode);
    return des;
  }
  const int des = openf(fn, flags, mode);
  if(des <= 0) { /* open failed; ignoring this file. */
    TRACE(posix, "posix-opening %s ignored due to open failure", fn);
    return des;
  } else {
    TRACE(posix, "posix-opening %s...", fn);
  }

  /* need an empty entry in the table to store the return value. */
  int empty = 0;
  struct openposixfile* of = ofposix_find(posix_files, fd_of, &empty);
  if(of == NULL) {
    WARN(posix, "out of open files.  skipping '%s'", fn);
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
  TRACE(posix, "writing %zu bytes to %d", sz, fd);
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
    TRACE(posix, "don't know FD %d; skipping 'close' instrumentation.", des);
    return closef(des);
  }
  TRACE(posix, "closing %s (FD %d [%d])", of->name, des, of->fd);
  assert(of->fd == des);
  if(of->name == NULL) { /* don't bother, not a real file? */
    WARN(posix, "null filename!?");
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
