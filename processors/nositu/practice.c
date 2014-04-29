/* This is a freeprocessor which synchronizes between producers and consumers.
 * Whenever the producer is finished with a file, this freeprocessor fires off
 * a consumer.  Configuration is done entirely via environment variables:
 *   LIBSITU_CONSUMER: the consumer program to be run.  The filename will be
 *                     appended as the final argument.  Required.
 *   LIBSITU_SYNC: if set, then consuming is synchronous with production.
 *                 Optional.
 * Be careful.  There is no consumer constraints without _SYNC, so if your
 * producer creates products (closes files) faster than your consumers can
 * consume them, you can quickly overload your system. */
#define _POSIX_C_SOURCE 200812L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "debug.h"
#include "launcher.h"

DECLARE_CHANNEL(pc);

static void consumer(const char* fn);

void
file(const char* fn)
{
  consumer(fn);
}

static void
consumer(const char* fn)
{
  const char* cons = getenv("LIBSITU_CONSUMER");
  if(cons == NULL) {
    ERR(pc, "'CONSUMER' environment variable not defined; what do you want me"
        " to run?");
  }
  char cmd[1024];
  snprintf(cmd, 1024, "%s %s", cons, fn);
  TRACE(pc, "Using '%s' as consumer command.", cmd);
  launch(cmd);
}

/* Freeprocessing requires this, for now. Just define it do nothing. */
void
exec(const char* fn, const void* buf, const size_t n) {
  (void)fn; (void)buf; (void)n;
}
