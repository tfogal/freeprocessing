#define _POSIX_C_SOURCE 200801L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "vis.h"

void launch_vis(const char* filename)
{
  pid_t pid = fork();
  if(pid == -1) {
    fprintf(stderr, "Could not fork vis process: %d\n", errno);
    return;
  }
  struct sigaction nocld;
  memset(&nocld, 0, sizeof(struct sigaction));
  nocld.sa_flags = SA_NOCLDWAIT;
  sigaction(SIGCLD, &nocld, NULL);
  if(pid == 0) {
    unsetenv("LD_PRELOAD");
    execl("/usr/bin/pvbatch", "pvbatch", "--use-offscreen-rendering",
          "batch.py", filename, NULL);
    fprintf(stderr, "could not exec pvbatch: %d\n", errno);
  }
}
