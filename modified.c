#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <errno.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>
#include "debug.h"
#include "modified.h"

DECLARE_CHANNEL(not);
static int inotify = -1;

struct watch_t {
  int des;
};

__attribute__((constructor(1000))) static void
setup_inotify()
{
  TRACE(not, "initializing inotify");
  if((inotify = inotify_init1(IN_CLOEXEC)) == -1) {
    ERR(not, "error initializing inotify!\n");
    return;
  }
}

__attribute__((destructor(1000))) static void
cleanup_inotify()
{
  if(inotify != -1) {
    close(inotify);
    inotify = -1;
  }
}

watch*
Watch(const char* fn)
{
  struct watch_t* wt = malloc(sizeof(struct watch_t));
  if(NULL == wt) { errno = ENOMEM; return NULL; }

  const uint32_t wflags = IN_CLOSE_WRITE;
  if((wt->des = inotify_add_watch(inotify, fn, wflags)) == -1) {
    const int saved_errno = errno;
    free(wt);
    errno = saved_errno;
    return NULL;
  }
  TRACE(not, "now watching file %s with fdes %d", fn, wt->des);
  return wt;
}

bool
Modified(const watch* w)
{
  assert(w);
  const struct watch_t* wt = (const struct watch_t*)w;

  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(wt->des, &rfds);
  FD_SET(inotify, &rfds);

  struct timeval tv;
  tv.tv_sec = tv.tv_usec = 0;
  tv.tv_usec = 1;

  const int rv = select(inotify+1, &rfds, NULL, NULL, &tv);
  TRACE(not, "seeing if fdes %d changed... (%d)", wt->des, rv);
  if(rv == -1) {
    WARN(not, "error in select: %d\n", errno);
    return false;
  }
  if(FD_ISSET(inotify, &rfds)) {
    /* read the event to clear the queue.. but we don't care about the data,
     * since we know what had to happen. */
    struct inotify_event ev;
    const ssize_t b = read(inotify, &ev, sizeof(struct inotify_event));
    if(b == -1) {
      ERR(not, "err clearing event queue: %d", errno);
      return true;
    }
    if(b < (ssize_t)sizeof(struct inotify_event)) {
      WARN(not, "could not read full inotify struct.. can this happen?");
      return true;
    }
  }
  return FD_ISSET(inotify, &rfds);
}

void
Unwatch(watch* w)
{
  assert(w);
  const struct watch_t* wt = (const struct watch_t*)w;
  if(close(wt->des) != 0) {
    WARN(not, "failed closing watchfd %d: %d\n", wt->des, errno);
  }
}
