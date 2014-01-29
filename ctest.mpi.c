#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "parallel.mpi.h"

extern void exec(const char*, const void*, size_t n);
extern void finish(const char*);

int
main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);
  if(rank() == 0) {
    finish("header.txt");
  }

  void* buf = calloc(30, sizeof(float));
  char fn[64];
  snprintf(fn, 64, "RestartFile.Rank%zu", rank());
  exec(fn, buf, 30*sizeof(float));
  finish(fn);

  free(buf);
  MPI_Finalize();
  return EXIT_SUCCESS;
}
