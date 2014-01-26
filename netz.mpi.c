#define _POSIX_C_SOURCE 201201L
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <sys/types.h>
#include <unistd.h>

struct field {
  char* name;
  size_t offset;
};
struct header {
  size_t nghost; /* number of ghost cells, per-dim */
  size_t dims[3]; /* dimensions of each brick, with ghost cells */
  size_t nfields;
  struct field* flds;
  size_t nbricks[3]; /* number of bricks, per dimension */
};

static FILE* bin = NULL;
static struct header hdr;

static size_t
rank()
{
  int rnk;
  MPI_Comm_rank(MPI_COMM_WORLD, &rnk);
  return (size_t)rnk;
}

static void
wait_for_debugger()
{
  if(rank() == 0) {
    volatile int x = 0;
    printf("[%ld] Waiting for debugger...\n", (long)getpid());
    while(x == 0) { ; }
  }
  MPI_Barrier(MPI_COMM_WORLD);
}

static void
start()
{
  assert(bin == NULL);
  char fname[256];
  snprintf(fname, 256, "tjfbin.%zu", rank());
  bin = fopen(fname, "wb");
  assert(bin);
}

/* strips a string.  returns the stripped string, but also modifies it. */
static const char*
strip(char** s)
{
  char* nl = strchr(*s, '\n');
  if(nl != NULL) {
    *nl = '\0';
  }
  return *s;
}

static size_t
parse_uint(const char* s)
{
  errno = 0;
  const unsigned long rv = strtoul(s, NULL, 10);
  if(errno != 0) {
    fprintf(stderr, "error converting '%s' to unsigned number.\n", s);
    abort();
  }
  return (size_t)rv;
}

static void
print_header(const struct header hdr)
{
  printf("%zu ghost cells per dim\n", hdr.nghost);
  printf("brick size: %zu x %zu x %zu\n", hdr.dims[0], hdr.dims[1], hdr.dims[2]);
  printf("%zu x %zu x %zu bricks\n", hdr.nbricks[0], hdr.nbricks[1],
         hdr.nbricks[2]);
  printf("%zu fields:\n", hdr.nfields);
  for(size_t i=0; i < hdr.nfields; ++i) {
    printf("\t%s at offset %25zu\n", hdr.flds[i].name, hdr.flds[i].offset);
  }
}

static struct header
read_header(const char *filename)
{
  struct header rv;
  fprintf(stderr, "[%zu] reading header info from %s...\n", rank(), filename);
  FILE* fp = fopen(filename, "r");
  if(!fp) {
    fprintf(stderr, "[%zu] error reading header file %s\n", rank(), filename);
    return rv;
  }
  char* line;
  size_t llen;
  ssize_t bytes;
  size_t field = 0; /* which field we are reading now */
  do {
    line = NULL;
    llen = 0;
    errno = 0;
    bytes = getline(&line, &llen, fp);
    if(bytes == -1 && errno != 0) {
      fprintf(stderr, "getline err: %d\n", errno);
      break;
    } else if(bytes == -1) {
      break;
    }
    strip(&line);
    if(strncasecmp(line, "nG", 2) == 0) {
      rv.nghost = parse_uint(line+3); /* +3 for "nG=" */
    } else if(strncasecmp(line, "ImaAll", 6) == 0) {
      rv.dims[0] = parse_uint(line+7);
    } else if(strncasecmp(line, "JmaAll", 6) == 0) {
      rv.dims[1] = parse_uint(line+7);
    } else if(strncasecmp(line, "KmaAll", 6) == 0) {
      rv.dims[2] = parse_uint(line+7);
    } else if(strncasecmp(line, "nF", 2) == 0) {
      rv.nfields = parse_uint(line+3);
      rv.flds = calloc(sizeof(struct field), rv.nfields);
      printf("fixme need provision for freeing the field array\n");
    } else if(strncasecmp(line, "dimsI", 5) == 0) {
      rv.nbricks[0]= parse_uint(line+6);
    } else if(strncasecmp(line, "dimsJ", 5) == 0) {
      rv.nbricks[1]= parse_uint(line+6);
    } else if(strncasecmp(line, "dimsK", 5) == 0) {
      rv.nbricks[2]= parse_uint(line+6);
    } else if(strncasecmp(line, "FieldNames", 10) == 0) {
      for(size_t i=0; i < rv.nfields; ++i) {
        errno = 0;
        bytes = getline(&line, &llen, fp);
        if(bytes == -1) {
          fprintf(stderr, "error in the middle of field parsing; "
                  "is nF wrong?!\n");
          exit(EXIT_FAILURE);
        }
        rv.flds[field].name = strdup(strip(&line));
        printf("fixme need something to free up the field names\n");
        rv.flds[field].offset = rv.dims[0]*rv.dims[1]*rv.dims[2]*sizeof(float)*
                                field;
        field++;
      }
    }
  } while(bytes > 0);
  fclose(fp);
  return rv;
}

static void
broadcast_header(struct header* hdr)
{
  (void)hdr;
  printf("[%zu] broadcasting header (fixme: do so)..\n", rank());
}

void
exec(const char* fn, const void* buf, size_t n)
{
  if(fnmatch("*header*", fn, 0) == 0) {
    return; /* parse header when file is closed, not during write. */
  }
  assert(fnmatch("*Restart*", fn, 0) == 0);
  if(NULL == bin) {
    start();
  }
  assert(bin != NULL);
  if(ferror(bin)) {
    MPI_Abort(MPI_COMM_WORLD, (long)getpid());
  }
  assert(!ferror(bin));
  fprintf(stderr, "[%ld] about to write %zu bytes..\n", (long)getpid(), n);
  errno = 0;
  const size_t written = fwrite(buf, 1, n, bin);
  if(written != n) {
    long pid = (long)getpid();
    fprintf(stderr, "[%ld] %s: write should be %zu, short: %zu? errno=%d\n",
            pid, __FILE__, n, written, errno);
    if(ferror(bin)) {
      fprintf(stderr, "[%ld] %s: binfile is in error.\n", pid, __FILE__);
    } else {
      fprintf(stderr, "[%ld] %s: binfile is OK.\n", pid, __FILE__);
    }
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "[%ld] finished write of %zu bytes\n", (long)getpid(), n);
  assert(!ferror(bin));
  MPI_Barrier(MPI_COMM_WORLD);
}

void
finish(const char* fn)
{
  if(fnmatch("*header*", fn, 0) == 0) {
    fprintf(stderr, "[%zu] header time\n", rank());
    hdr = read_header("RESTART/header.txt");
    print_header(hdr);
    return;
  }
  if(bin && fclose(bin) != 0) {
    fprintf(stderr, "%s: error closing file!\n", __FILE__);
  }
  bin = NULL;
}
