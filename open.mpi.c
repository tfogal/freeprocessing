#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

static size_t
rank()
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return (size_t)rank;
}

#define ROOTp() (rank() == 0)

static void
wait_for_debugger()
{
  if(ROOTp() && getenv("TJF_DEBUG_MPI") != NULL) {
    volatile int i=0;
    while(i == 0) { }
  }
  MPI_Barrier(MPI_COMM_WORLD);
}

int
main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);
  wait_for_debugger();
  MPI_File fh;
  int rv =  MPI_File_open(MPI_COMM_WORLD, "atestfile", MPI_MODE_WRONLY,
                          MPI_INFO_NULL, &fh);
  fprintf(stderr, "open returned: %d\n", rv);
  rv = MPI_File_close(&fh);
  fprintf(stderr, "close returned: %d\n", rv);

  MPI_Finalize();
  return EXIT_SUCCESS;
}
