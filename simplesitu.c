#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "vis.h"

#ifndef NDEBUG
# define LOG(x, ...) fprintf(logstream, x, __VA_ARGS__)
#else
# define LOG(x, ...) /* log function removed for release */
#endif

typedef FILE* (fopenfqn)(const char*, const char*);
typedef int (fclosefqn)(FILE*);

static fopenfqn* fopenf = NULL;
static fclosefqn* fclosef = NULL;
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
#define MAX_FILES 1024
static struct openfile files[MAX_FILES] = {{NULL, NULL}};

typedef int (ofpredicate)(const struct openfile*, const void*);

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
  fopenf = dlsym(RTLD_NEXT, "fopen");
  fclosef = dlsym(RTLD_NEXT, "fclose");
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
    free(of->name);
  }
  return of->fp;
}

int
fclose(FILE* fp)
{
  LOG("[%d] closing %p\n", pid, fp);
  struct openfile* of = of_find(files, fp_of, fp);
  if(of == NULL) {
    fprintf(stderr, "[%d] I don't know %p.  Ignoring for in situ.\n", pid, fp);
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
