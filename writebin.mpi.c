#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <mpi.h>

static FILE* fp = NULL;
static size_t slice_number = 0;
static int inotify = -1;
static int cfgwatchdes = -1;
static const char* cfgfile = "psiphi.cfg";

static size_t
rank()
{
  int rnk;
  MPI_Comm_rank(MPI_COMM_WORLD, &rnk);
  return (size_t)rnk;
}

static size_t
read_slicenum()
{
  FILE* cfg = fopen(cfgfile, "r");
  if(!cfg) {
    fprintf(stderr, "%s: no '%s'; using slice 0.\n", __FILE__, cfgfile);
    return 0;
  }
  size_t snum;
  if(fscanf(cfg, "%zu", &snum) != 1) {
    fprintf(stderr, "error reading slice number from %s!\n", cfgfile);
  }
  fclose(cfg);
  return snum;
}

static void
myfilename(char fname[256])
{
  snprintf(fname, 256, "tjfbin.%zu", rank());
}

static bool
slice_num_changed()
{
  /* FIXME implement this to select/read the cfgwatchdes */
  return false;
}

static void
start()
{
  assert(fp == NULL);
  char fname[256] = {0};
  myfilename(fname);
  fp = fopen(fname, "wb");
  assert(fp);

  slice_number = read_slicenum();
  if((inotify = inotify_init1(IN_CLOEXEC)) == -1) {
    fprintf(stderr, "%s error initializing inotify\n", __FILE__);
    return;
  }
  const int wflags = IN_CLOSE_WRITE;
  if((cfgwatchdes = inotify_add_watch(inotify, cfgfile, wflags)) == -1) {
    fprintf(stderr, "%s error adding watch for config file...\n", __FILE__);
    close(inotify); inotify = -1;
  }
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
  if(slice_num_changed()) {
    slice_number = read_slicenum();
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
  if(close(cfgwatchdes) != 0) {
    fprintf(stderr, "%s: error closing watch descriptor\n", __FILE__);
  }
  if(close(inotify) != 0) {
    fprintf(stderr, "%s: error closing inotify descriptor\n", __FILE__);
  }
}
