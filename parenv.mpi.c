#include <stdio.h>
#include <mpi.h>

static size_t
rank()
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return (size_t)rank;
}

extern char** environ;

int
main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);
  for(char** ev=environ; rank() == 0 && *ev; ++ev) {
    printf("env: %s\n", *ev);
  }
  MPI_Finalize();
}
