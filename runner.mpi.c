#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#ifdef BLAH
# define PURE __attribute__((pure))
#else
# define PURE /* pure attribute removed */
#endif

#define ROOT(stmt) \
  do { \
    if(rank() == 0) { stmt; } \
  } while(0)

PURE static size_t rank();
PURE static size_t size();

void rebuild_args(size_t argc, char* argv[], char** cmd, char* subv[]);

int
main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);
  if(rank() == 0 && argc < 2) {
    fprintf(stderr, "Need at least one argument: the binary to run.\n");
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
  }
  ROOT(printf("Running on %zu procs.\n", size()));
  /* MPI (annoyingly) displaces the argument list by one, so rebuild it. */
  char* subv[argc]; memset(subv, 0, sizeof(char*)*argc);
  char* command;
  assert(argc > 0);
  rebuild_args((size_t)argc, argv, &command, subv);
  if(rank() == 0) { printf("arguments for '%s':\n", command); }
  for(size_t i=0; rank()==0 && i < (size_t)argc; ++i) {
    printf("\t%s\n", subv[i]);
  }
  ROOT(printf("Spawning...\n"));
  MPI_Comm intercomm; /* we don't need, but MPI requires. */
  int errors[size()];
  MPI_Comm_spawn(command, subv, (int)size(), MPI_INFO_NULL, 0, MPI_COMM_WORLD,
                 &intercomm, errors);
  for(size_t i=0; rank()==0 && i < size(); ++i) {
    if(errors[i] != MPI_SUCCESS) {
      printf("process %zu error: %d\n", i, errors[i]);
    }
  }

  MPI_Finalize();
  return 0;
}

PURE static size_t
rank()
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return (size_t)rank;
}

PURE static size_t
size()
{
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return (size_t)size;
}

void
rebuild_args(size_t argc, char* argv[], char** cmd, char* subv[])
{
  /* argv[0] is the name of this program.
   * argv[1] is the name of the program the user wanted to run, "child"
   * argv[x] for x > 1 are the arguments of "child". */
  for(size_t i=2; i < argc; ++i) {
    subv[i-2] = argv[i];
  }
  *cmd = argv[1];
}
