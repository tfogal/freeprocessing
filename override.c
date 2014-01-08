#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef int (openfqn)(const char*, int, ...);
typedef ssize_t (readfqn)(int, void*, size_t);
typedef ssize_t (writefqn)(int, const void*, size_t);
typedef int (closefqn)(int);
typedef int (dup2fqn)(int, int);
typedef FILE* (fopenfqn)(const char*, const char*);
typedef int (fclosefqn)(FILE*);

typedef void* MPI_Comm;
typedef void* MPI_Info;
typedef void* MPI_File;
typedef int (mpi_file_openfqn)(MPI_Comm, char*, int, MPI_Info, MPI_File*);

static openfqn* openf = NULL;
static readfqn* readf = NULL;
static writefqn* writef = NULL;
static closefqn* closef = NULL;
static dup2fqn* dup2f = NULL;
static fopenfqn* fopenf = NULL;
static fclosefqn* fclosef = NULL;
static mpi_file_openfqn* mpi_file_openf = NULL;

__attribute__((constructor)) void
fp_init()
{
  fprintf(stderr, "[%d] initializing function pointers.\n", (int)getpid());
  openf = dlsym(RTLD_NEXT, "open");
  readf = dlsym(RTLD_NEXT, "read");
  writef = dlsym(RTLD_NEXT, "write");
  closef = dlsym(RTLD_NEXT, "close");
  dup2f = dlsym(RTLD_NEXT, "dup2");
  fopenf = dlsym(RTLD_NEXT, "fopen");
  fclosef = dlsym(RTLD_NEXT, "fclose");
  mpi_file_openf = dlsym(RTLD_NEXT, "MPI_File_open");

  assert(openf != NULL);
  assert(readf != NULL);
  assert(writef != NULL);
  assert(closef != NULL);
  assert(dup2f != NULL);
  assert(fopenf != NULL);
  assert(fclosef != NULL);
  assert(mpi_file_openf != NULL);
}

int
open(const char* fn, int flags, ...)
{
  va_list aq;
  va_start(aq, flags);
  fprintf(stderr, "[%d] opening %s ...", (int)getpid(), fn);
  int des = openf(fn, flags, aq);
  fprintf(stderr, " -> %d\n", des);
  va_end(aq);
  return des;
}

ssize_t
read(int d, void* buf, size_t sz)
{
  fprintf(stderr, "[%d] reading %zu bytes from %d...\n", (int)getpid(), sz, d);
  assert(readf != NULL);
  return readf(d, buf, sz);
}

ssize_t
write(int d, const void *buf, size_t sz)
{
  fprintf(stderr, "[%d] writing %zu bytes to %d\n", (int)getpid(), sz, d);
  return writef(d, buf, sz);
}

int
close(int d)
{
  fprintf(stderr, "[%d] closing %d\n", (int)getpid(), d);
  return closef(d);
}

int
dup2(int old, int new)
{
  fprintf(stderr, "[%d] dup2-ing %d to %d\n", (int)getpid(), old, new);
  return dup2f(old, new);
}

FILE*
fopen(const char* name, const char* mode)
{
  fprintf(stderr, "[%d] opening %s\n", (int)getpid(), name);
  return fopenf(name, mode);
}

int
fclose(FILE* fp)
{
  fprintf(stderr, "[%d] closing %p\n", (int)getpid(), fp);
  return fclosef(fp);
}

int MPI_File_open(MPI_Comm comm, char *filename, int amode,
                  MPI_Info info, MPI_File *fh)
{
  fprintf(stderr, "[%d] mpi_file_open '%s'\n", (int)getpid(), filename);
  return mpi_file_openf(comm, filename, amode, info, fh);
}
