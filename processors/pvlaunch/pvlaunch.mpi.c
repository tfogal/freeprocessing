#include <stdio.h>
#include "parallel.mpi.h"
#include "vis.h"

void
exec(const char* filename, const void* buf, size_t n)
{
  (void)filename;
  (void)buf;
  (void)n;
}

void
finish(const char* filename)
{
  if(rank() == 0) {
    launch_vis(filename, stderr);
  }
}
