#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE 1
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "debug.h"

DECLARE_CHANNEL(track);

static char** list = NULL;
static size_t nstr = 0;

static void
append(const char* name)
{
  nstr++;
  void* mem = realloc(list, sizeof(char*)*nstr);
  if(mem == NULL) {
    /* just give up. */
    return;
  } else {
    list = mem;
  }
  list[nstr-1] = strdup(name);
  time_t tm = time(NULL);
  char tms[256];
  struct tm localtm;
  if(localtime_r(&tm, &localtm) == NULL) {
    ERR(track, "ltime_r failed (%d).", errno);
    return;
  }
  if(strftime(tms, 256, "%F %T", &localtm) == 0) {
    ERR(track, "strftime %d", errno);
  }
  FILE* fp = fopen("/home/tfogal/.provenance/files", "a");
  if(!fp) {
    ERR(track, "could not open tracking file...");
    return; /* forget it, it's not that important. */
  }
  fprintf(fp, "%s %s\n", tms, name);
  /* we don't check for a write error... it's not a huge deal if we miss a file
   * or two here and there, it would be annoyingly spammy if we did, and
   * there's not really anything the user could do about it anyway. */
  fclose(fp);
}

static bool
unique(const char* name)
{
  for(size_t i=0; i < nstr; ++i) {
    if(strcmp(list[i], name) == 0) { return false; }
  }
  return true;
}

__attribute__((destructor(500))) static void
cleanup_list()
{
  for(size_t i=0; i < nstr; ++i) {
    free(list[i]);
    list[i] = NULL;
  }
  free(list);
  list = NULL;
}

void
file(const char* fn)
{
  char* pth = realpath(fn, NULL); /* fails w/o POSIX 2008! */
  if(strstr(pth, "data") == NULL) { /* not in data */
    TRACE(track, "%s isn't in data; ignoring.", fn);
    free(pth);
    return;
  }
  if(unique(pth)) {
    TRACE(track, "unique file!  adding %s", pth);
    append(pth);
  } else {
    TRACE(track, "%s already in list, skipping.", pth);
  }
  free(pth);
}

/* just ignore. */
void
exec(const char* fn, const void* buf, size_t n)
{
  (void) fn; (void) buf; (void) n;
}
#if 0
void finish(const char* fn) { (void)fn; }
#endif
