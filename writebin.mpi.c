#include <assert.h>
#include <stdio.h>
#include <mpi.h>

static FILE* fp = NULL;

static size_t
rank()
{
  int rnk;
  MPI_Comm_rank(MPI_COMM_WORLD, &rnk);
  return (size_t)rnk;
}

static void
start()
{
  assert(fp == NULL);
  char fname[256];
  snprintf(fname, 256, "tjfbin.%zu", rank());
  fp = fopen(fname, "wb");
  assert(fp);
}

void
exec(unsigned dtype, const size_t dims[3], const void* buf, size_t n)
{
  /* we don't need data type args, for now.. we just copy the binary data
   * somewhere else. */
  (void)dtype; (void)dims;
  if(NULL == fp) {
    start();
  }
  if(fwrite(buf, 1, n, fp) != n) {
    fprintf(stderr, "%s: short write?\n", __FILE__);
  }
}

void
finish(unsigned dtype, const size_t dims[3])
{
  (void)dtype; (void)dims;
  if(fclose(fp) != 0) {
    fprintf(stderr, "%s: error closing file!\n", __FILE__);
  }
}
