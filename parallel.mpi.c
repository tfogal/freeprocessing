#include <mpi.h>
#include "parallel.mpi.h"

PURE size_t
rank()
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  return (size_t)rank;
}

PURE size_t
size()
{
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return (size_t)size;
}

void par_abort() { MPI_Abort(MPI_COMM_WORLD, -1); }

void
broadcastf(float* f, size_t n)
{
  MPI_Bcast(f, n, MPI_FLOAT, 0, MPI_COMM_WORLD);
}

void
broadcastlf(double* lf, size_t n)
{
  MPI_Bcast(lf, n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

void
broadcastzu(size_t* zu, size_t n)
{
  /* MPI doesn't have a "size_t" equivalent. Use a tempvar instead. */
  unsigned data[n];
  if(rank() == 0) {
    unsigned* v;
    size_t* z;
    for(v=data, z=zu; v < data+n; ++v, ++z) {
      *v = *z;
    }
  }
  MPI_Bcast(data, n, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  unsigned* v; size_t* z;
  for(v=data, z=zu; v < data+n; ++v, ++z) { *z = *v; }
}

void
broadcasts(char* str, size_t len)
{
  MPI_Bcast(str, len, MPI_BYTE, 0, MPI_COMM_WORLD);
}

void
broadcastb(bool* b, size_t n)
{
  MPI_Bcast(b, n, MPI_BYTE, 0, MPI_COMM_WORLD);
}

void barrier() { MPI_Barrier(MPI_COMM_WORLD); }

void
allgatherf(float* f, int* rcvcount, int* displacements)
{
  MPI_Allgatherv(MPI_IN_PLACE, 0 /* ignored  */, MPI_DATATYPE_NULL /*ignored*/,
                 f, rcvcount, displacements, MPI_FLOAT, MPI_COMM_WORLD);
}

void
allgatherlf(double* lf, int* rcvcount, int* displacements)
{
  MPI_Allgatherv(MPI_IN_PLACE, 0 /* ignored  */, MPI_DATATYPE_NULL /*ignored*/,
                 lf, rcvcount, displacements, MPI_DOUBLE, MPI_COMM_WORLD);
}
