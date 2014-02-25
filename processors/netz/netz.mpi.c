#define _POSIX_C_SOURCE 201201L
#define _GNU_SOURCE
#include <aio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <mpi.h>
#include "debug.h"
#include "parallel.mpi.h"

DECLARE_CHANNEL(netz);

#define NULLIFY (void*)0xdeadbeef

struct field {
  char* name;
  size_t lower; /* offset of this field in bytestream */
  size_t upper; /* final byte in stream +1 for this field */
  bool out3d; /* should we output a unified 3D volume of this field? */
  size_t slice; /* the slice to output, +1.  0 represents no slice output. */
};
struct header {
  size_t nghost; /* number of ghost cells, per-dim */
  size_t dims[3]; /* dimensions of each brick, with ghost cells */
  size_t nfields;
  struct field* flds;
  size_t nbricks[3]; /* number of bricks, per dimension */
};

/* we will open any number of files---one per field they want written---and
 * stream the binary data to those files.   this array holds those fields.
 * NULL indicates that the simulation is not doing file I/O at the moment, or
 * is only writing to its header file.  The array will be non-null if we're in
 * the middle of some output files. */
static FILE** binfield = NULL;
/* ditto, except for when we're writing slices. oh, and we need descriptors. */
static int* slicefield = NULL;
/* Where we are in whatever output file we are at now.  Only tracked for the
 * restart files.  We use this to figure out if the current write is within any
 * of the fields the user has decided they want to see. */
static size_t offset = 0; /* current offset in output file. */
/* Header info is parsed when we close the 'header.txt' file.  See 'finish'. */
static struct header hdr = {0, {0,0,0}, 0, NULL, {0,0,0}};

void
wait_for_debugger()
{
  static bool entered = false;
  if(!entered) {
    if(rank() == 0) {
      volatile int i = 0;
      printf("[%ld] Waiting for debugger...\n", (long)getpid());
      while(i == 0) { ; }
    }
    (void) MPI_Barrier(MPI_COMM_WORLD);
    entered = true;
  }
}

static void
broadcast_header(struct header* hdr)
{
  broadcastzu(&hdr->nghost, 1);
  broadcastzu(hdr->dims, 3);
  broadcastzu(&hdr->nfields, 1);
  if(rank() != 0) {
    assert(hdr->flds == NULL);
    hdr->flds = calloc(hdr->nfields, sizeof(struct field));
    printf("allocated %zu fields: %p\n", hdr->nfields, hdr->flds);
  }
  for(size_t i=0; i < hdr->nfields; ++i) {
    size_t len;
    if(rank() == 0) { len = strlen(hdr->flds[i].name); }
    broadcastzu(&len, 1);
    if(rank() != 0) { hdr->flds[i].name = calloc(len+1, sizeof(char)); }
    broadcasts(hdr->flds[i].name, len);
    broadcastzu(&hdr->flds[i].lower, 1);
    broadcastzu(&hdr->flds[i].upper, 1);
    broadcastb(&hdr->flds[i].out3d, 1);
  }
  broadcastzu(hdr->nbricks, 3);
}

static void
print_header(const struct header hdr)
{
  TRACE(netz, "%zu ghost cells per dim\n", hdr.nghost);
  TRACE(netz, "brick size: %zux%zux%zu\n", hdr.dims[0],hdr.dims[1],hdr.dims[2]);
  TRACE(netz, "%zu x %zu x %zu bricks\n", hdr.nbricks[0], hdr.nbricks[1],
        hdr.nbricks[2]);
  TRACE(netz, "%zu fields:\n", hdr.nfields);
  for(size_t i=0; i < hdr.nfields; ++i) {
    TRACE(netz, "\t%-5s at offset %25zu\n", hdr.flds[i].name,hdr.flds[i].lower);
  }
}

static size_t
parse_uint(const char* s)
{
  errno = 0;
  const unsigned long rv = strtoul(s, NULL, 10);
  if(errno != 0) {
    ERR(netz, "error converting '%s' to unsigned number.\n", s);
    abort();
  }
  return (size_t)rv;
}

/* reads a field name + sets the appropriate/corresponding bit in the header. */
static bool
read_field_config(FILE* fp, struct header* h)
{
  assert(fp);
  assert(h);

  char* fld = calloc(sizeof(char), 512);
  char* cfg = calloc(sizeof(char), 512);
  const int m = fscanf(fp, "%511s { %511s }", fld, cfg);
  if(feof(fp)) {
    TRACE(netz, "EOF scanning config, we must be done.");
    free(fld);
    free(cfg);
    return false;
  }
  if(m != 2) {
    WARN(netz, "could not match field...");
    free(fld);
    free(cfg);
    return false;
  }
  for(size_t i=0; i < h->nfields; ++i) {
    if(strcasecmp(h->flds[i].name, fld) == 0) {
      /* the config can be either "3D" or "slice=Z" */
      if(strncasecmp(cfg, "3d", 2) == 0) {
        TRACE(netz, "will create 3D vol of '%s'", h->flds[i].name);
        h->flds[i].out3d = 1;
      } else if(strncasecmp(cfg, "slice", 5) == 0) {
        /* we use 0 to denote 'no slice'. */
        h->flds[i].slice = parse_uint(cfg+6) + 1; /* 6 == strlen("slice=") */
        TRACE(netz, "will write slice %zu of '%s'", h->flds[i].slice-1,
              h->flds[i].name);
      }
    }
  }
  free(cfg);
  free(fld);
  return !feof(fp);
}

/* reads the PsiPhi configuration file.  we expect to find a bunch of field
 * names; for each field name in the config file, set the appropriate bit in
 * the 'struct field' within the header. */
static void
read_config(const char* from, struct header* h)
{
  FILE* cfg = fopen(from, "r");
  if(!cfg) {
    ERR(netz, "could not read configuration file '%s'", from);
    return;
  }
  for(size_t i=0; i < h->nfields; ++i) { /* clear field configs. */
    h->flds[i].out3d = 0;
    h->flds[i].slice = 0;
  }
  while(read_field_config(cfg, h)) { ; }
  fclose(cfg);
}

static void
tjfstart()
{
  /* when we are starting the binary file, we know that we are done with the
   * ascii data descriptor. */
  broadcast_header(&hdr);
  if(rank() != 0) {
    print_header(hdr);
  }
  offset = 0;
  read_config("psiphi.cfg", &hdr);

  /* after reading the config, we should know how many fields we have. */
  assert(binfield == NULL);
  assert(slicefield == NULL);
  assert(hdr.nfields > 0);
  binfield = calloc(hdr.nfields, sizeof(FILE*));
  slicefield = calloc(hdr.nfields, sizeof(int));
  for(size_t i=0; i < hdr.nfields; ++i) {
    if(hdr.flds[i].out3d) {
      char fname[256];
      snprintf(fname, 256, "%s.%zu", hdr.flds[i].name, rank());
      binfield[i] = fopen(fname, "wb");
      if(!binfield[i]) {
        ERR(netz, "could not create '%s'", fname);
        return;
      }
    }
    slicefield[i] = -1; /* default to -1, i.e. 'no file open' */
    if(hdr.flds[i].slice) {
      char fname[256];
      snprintf(fname, 256, "%s.slice%zu", hdr.flds[i].name,
               hdr.flds[i].slice-1);
      slicefield[i] = open(fname, O_WRONLY | O_TRUNC | O_CLOEXEC | O_CREAT,
                           S_IWUSR | S_IRUSR | S_IRGRP);
      if(-1 == slicefield[i]) {
        ERR(netz, "could not create '%s'", fname);
        abort();
        return;
      }
    }
  }
  /* do we need a barrier to make sure that e.g. process 0 doesn't finish this
   * function, move on, start writing, and *then* process 1 comes along and
   * O_TRUNC's one of our slicefields? */
  assert(binfield);
  assert(slicefield);
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

/* the fields from PsiPhi are padded with "_"s so that they are all the same
 * length.  fix that padding so they are displayed nicely. */
static void
remove_underscores(char* str)
{
  assert(str);
  for(char* s=str; *s; ++s) {
    if(*s == '_') { *s = '\0'; break; }
  }
}

struct writelist {
  size_t n;
  struct aiocb** list; /* will be allocated as an n-element array. */
  size_t* srcoffset; /* will be allocated as an n-element array */
};
/* Writes are given to us as a byte range: 0 to 40000, for example.  Yet we are
 * MPI-parallel, and each process has a piece of the final file.  The local
 * 3D-array that process 0 has will actually end up as a series of chunks in
 * the final file.
 * This function takes the byte range, the size of the full domain, and the
 * size of local chunks, and calculates the offsets that they should appear in
 * a unified file.  It assumes that the source data is a single contiguous
 * stream.
 * NOTE that this cannot completely fill in the aiocb!  In particular,
 * it does not have access to the buffer to read from, nor the file
 * descriptor to write to.  Callers will need to fill this information in
 * themselves; 'srcoffset' is intended to be used as offsets into the buffer. */
static struct writelist
find_destination(const size_t pos[3], /* brick position of this processor */
                 const size_t voxels[3], /* number of voxels in full domain */
                 const size_t bsize[3] /* size of a single brick */)
{
  struct writelist rv;
  rv.n = bsize[1]*bsize[2];
  rv.list = calloc(rv.n, sizeof(struct aiocb*));
  rv.srcoffset = calloc(rv.n, sizeof(size_t));
  for(size_t z=0; z < bsize[2]; ++z) {
    for(size_t y=0; y < bsize[1]; ++y) {
      const size_t x = 0; /* just for legibility */
      const size_t tgt_z = z + (pos[2]*bsize[2]);
      const size_t tgt_y = y + (pos[1]*bsize[1]);
      const size_t tgt_x = x + (pos[0]*bsize[0]);
      const off_t dstoffset = sizeof(float) * ((tgt_z * voxels[1] * voxels[0]) +
                                               (tgt_y * voxels[0]) + tgt_x);
      const size_t len = sizeof(float) * bsize[0];
      const size_t i = z*bsize[1]+y;
      /* This is a bit infuriating.  lio_listio(2) for some reason takes an
       * array of aiocb /pointers/ instead of an array of aiocb's.  So we have
       * to allocate *each*individual*one*.  Awful. */
      rv.list[i] = calloc(1, sizeof(struct aiocb));
      rv.list[i]->aio_offset = dstoffset;
      rv.list[i]->aio_nbytes = len;
      rv.list[i]->aio_sigevent.sigev_notify = SIGEV_NONE;
      rv.list[i]->aio_lio_opcode = LIO_WRITE;
      rv.srcoffset[i] = sizeof(float) * (z*bsize[1]*bsize[0] + y*bsize[0] + x);
    }
  }
  return rv;
}
PURE static size_t minzu(size_t a, size_t b) { return a < b ? a : b; }

static struct writelist
wlist2d(const size_t vox[3], /* n voxels for whole domain */
        const size_t offset_bytes, /* bytes already written from this slice */
        const void* a, /* buffer to write. */
        const size_t n, /* number of *bytes* in 'a' */
        const size_t layout[3], /* local rank's layout: which brick they are */
        const size_t bs0) /* width (num elems) in a brick's scanline */
{
  const size_t c = sizeof(float); /* bytes per component. */
  const size_t offset_elem = offset_bytes / c;
  /* 'start offset': accounts for 'a' not starting at a brick boundary */
  const size_t s = offset_elem % bs0;
  const void* b = ((const char*)a) + n; /* end of 'a' buffer. */
  const size_t nelems = (b - a) / c;
  const size_t nwrites = (size_t) ceil((float)nelems / (float)bs0);
  const size_t sline = offset_elem / bs0;

  struct writelist writes;
  writes.n = nwrites;
  writes.list = calloc(nwrites, sizeof(struct aiocb*));
  /* we don't use srcoffset, since we have the buffer.  just set it to 0s. */
  writes.srcoffset = calloc(nwrites, sizeof(size_t));
  writes.list[0] = calloc(1, sizeof(struct aiocb));
  TRACE(netz, "sline(%zu), vox0(%zu), bs0(%zu), layout0(%zu), s(%zu)", sline,
        vox[0], bs0, layout[0], s);
  writes.list[0]->aio_offset = (sline*vox[0]+bs0*layout[0]+s) * c;
  writes.list[0]->aio_buf = (void*)a;
  writes.list[0]->aio_nbytes = (bs0 - s) * c;
  writes.list[0]->aio_sigevent.sigev_notify = SIGEV_NONE;
  writes.list[0]->aio_lio_opcode = LIO_WRITE;

  for(size_t i=1; i < nwrites; ++i) {
    writes.list[i] = calloc(1, sizeof(struct aiocb));
    writes.list[i]->aio_offset = c * (((sline+i) * vox[0]) + layout[0]*bs0);
    writes.list[i]->aio_buf = (void*)((const char*)a +
                                      c * ((bs0-s) + ((i-1)*bs0)));
    writes.list[i]->aio_nbytes = minzu(bs0*c, b - writes.list[i]->aio_buf);
  }
  return writes;
}

static void
free_writelist(struct writelist wr)
{
  for(size_t i=0; i < wr.n; ++i) {
    free(wr.list[i]);
    wr.list[i] = NULLIFY;
  }
  free(wr.list);
  free(wr.srcoffset);
  wr.list = NULLIFY;
  wr.srcoffset = NULLIFY;
}

/* reads the first 'n' bytes from the given file.  returns an (allocated)
 * pointer to the data. */
MALLOC static void*
slurp(const char* fn, size_t n)
{
  FILE* fp = fopen(fn, "rb");
  if(!fp) {
    ERR(netz, "could not slurp %zu bytes of %s", n, fn);
    return NULL;
  }
  void* buf = malloc(n);
  if(buf == NULL) {
    ERR(netz, "slurp could not alloc %zu bytes", n);
    fclose(fp);
    return NULL;
  }
  const size_t bytes = fread(buf, 1, n, fp);
  if(bytes != n) {
    WARN(netz, "short read (%zu bytes) slurping %s: %d", bytes, fn, errno);
  }
  fclose(fp);
  return buf;
}

static void
apply_writelist(struct writelist wl, const size_t bsize[3],
                const char* to, const char* from)
{
  const int fd = open(to, O_WRONLY | O_TRUNC | O_CLOEXEC | O_CREAT,
                      S_IWUSR | S_IRUSR | S_IRGRP);
  if(fd == -1) {
    ERR(netz, "open error on %s: %d.  giving up.", to, errno);
    return;
  }
  char* data = slurp(from, bsize[2]*bsize[1]*bsize[0]*sizeof(float));
  if(!data) {
    unlink(to);
    close(fd);
    return;
  }
  /* do we need a barrier here?  is it possible for process 0 to get to this
   * unlink before process 1 finishes the open? */
  if(unlink(from) != 0) {
    WARN(netz, "could not remove brick '%s': %d", from, errno);
  }
  for(size_t i=0; i < wl.n; ++i) {
    assert(wl.list[i]);
    wl.list[i]->aio_fildes = fd;
    wl.list[i]->aio_buf = data + wl.srcoffset[i];
  }
  if(lio_listio(LIO_WAIT, wl.list, wl.n, NULL) == -1) {
    WARN(netz, "listio failed: %d", errno);
  }
  free(data);
  close(fd);
}

static struct header
read_header(const char *filename)
{
  struct header rv = {0};
  TRACE(netz, "[%zu] reading header from %s", rank(), filename);
  FILE* fp = fopen(filename, "r");
  if(!fp) {
    ERR(netz, "[%zu] error reading header file %s: %d", rank(), filename,
        errno);
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
      ERR(netz, "getline error: %d", errno);
      free(line);
      break;
    } else if(bytes == -1) {
      free(line);
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
    } else if(strncasecmp(line, "dimsI", 5) == 0) {
      rv.nbricks[0]= parse_uint(line+6);
    } else if(strncasecmp(line, "dimsJ", 5) == 0) {
      rv.nbricks[1]= parse_uint(line+6);
    } else if(strncasecmp(line, "dimsK", 5) == 0) {
      rv.nbricks[2]= parse_uint(line+6);
    } else if(strncasecmp(line, "FieldNames", 10) == 0) {
      for(size_t i=0; i < rv.nfields; ++i) {
        errno = 0;
        free(line); line = NULL;
        bytes = getline(&line, &llen, fp);
        if(bytes == -1) {
          ERR(netz, "error while processing fields; is nF wrong?!");
          exit(EXIT_FAILURE);
        }
        rv.flds[field].name = strdup(strip(&line));
        remove_underscores(rv.flds[field].name);
        const size_t fldsize = rv.dims[0]*rv.dims[1]*rv.dims[2]*sizeof(float);
        rv.flds[field].lower = fldsize * field;
        rv.flds[field].upper = rv.flds[field].lower + fldsize;
        rv.flds[field].out3d = false;
        rv.flds[field].slice = 0;
        field++;
      }
    }
    free(line);
  } while(bytes > 0);
  fclose(fp);
  return rv;
}

static void
free_header(struct header* head)
{
  for(size_t i=0; i < head->nfields; ++i) {
    free(head->flds[i].name);
    head->flds[i].name = NULL;
  }
  free(head->flds);
  head->flds = NULL;
  head->nfields = 0;
}

__attribute__((destructor)) static void
cleanup()
{
  free_header(&hdr);
}

/* which bytes intersect with the ones we want to write?
 * @param skip number of bytes to skip before we start writing
 * @param nwrite number of bytes to write, maxes out at 'len'.
 * @return true if there is a non-zero byte overlap */
static bool
byteintersect(const size_t lower, const size_t upper,
              const size_t fldlower, const size_t fldupper,
              size_t* skip, size_t* nwrite)
{
  assert(upper > 0);
  assert(lower < upper);
  if(upper < fldlower || lower >= fldupper) {
    return false;
  }
  /* there is an intersection.  one of 4 cases: (1) the write goes beyond
   * the field; (2) the write starts before the field; (3) the write is
   * completely contained within the field; (4) the write is larger than the
   * field */
  *skip = 0;
  *nwrite = 0;
  if(lower > fldlower && upper > fldupper) {
    *skip = 0;
    *nwrite = fldupper - lower;
  } else if(lower < fldlower && upper < fldupper) {
    *skip = fldlower - lower;
    *nwrite = upper - fldlower;
  } else if(lower >= fldlower && upper < fldupper) {
    *skip = 0;
    *nwrite = upper - lower;
  } else { /* (4) write is greater than field! */
    assert(lower <= fldlower);
    assert(upper >= fldupper);
    assert(lower < fldupper);
    assert(upper > fldlower);
    *skip = fldlower - lower;
    *nwrite = fldupper - fldlower; /* the whole field. */
  }
  return true;
}

static bool
byteintersect2d(const size_t lower, const size_t upper,
                const size_t fldlower, const size_t fldupper,
                const size_t dims[3], size_t slice,
                size_t* skip, size_t* nwrite)
{
  (void)fldupper;
  /* This differs from the case above because our field is actually a subset of
   * the field, since we only care about a slice.  We use the same
   * calculation, but a modified field lower and upper byte range to account
   * for the subset we want to pull out. */
  const size_t flower = fldlower + slice*dims[1]*dims[0]*sizeof(float);
  const size_t fupper = flower + dims[1]*dims[0]*sizeof(float);
  return byteintersect(lower,upper, flower,fupper, skip,nwrite);
}

static void
to3d(const size_t proc, const size_t layout[3], size_t out[3])
{
  out[0] = proc % layout[0];
  out[1] = (proc / layout[0]) % layout[1];
  out[2] = proc / (layout[0]*layout[1]);
}

static void
create_nhdr(const char* rawfn, const size_t voxels[3])
{
  char* hname = calloc(strlen(rawfn)+8, sizeof(char));
  strcpy(hname, rawfn);
  strcat(hname, ".nhdr");
  FILE* fp = fopen(hname, "w");
  free(hname);
  if(!fp) {
    WARN(netz, "could not create header filename");
    return;
  }
  const int m = fprintf(fp,
    "NRRD0001\n"
    "encoding: raw\n"
    "dimension: 3\n"
    "type: float\n"
    "sizes: %zu %zu %zu\n"
    "spacings: 1 1 1\n"
    "data file: %s\n", voxels[0], voxels[1], voxels[2], rawfn);
  if(m < 4) {
    WARN(netz, "conversion error creating nhdr for %s: %d", rawfn, errno);
  }
  if(fclose(fp) != 0) {
    ERR(netz, "error creating .nhdr for %s", rawfn);
  }
}

/* When we get a 2D write, if that write crosses a scanline boundary, the
 * output needs to go to multiple places in the output file.  This takes a
 * write, figures out where it goes, and writes it out. */
static void
writes2d(size_t offset, const void* buf, size_t nbytes, int to,
         const struct header h)
{
  assert(to >= 0); /* can a descriptor be 0?  probably not, but... */
  TRACE(netz, "2d decomposition of %zu bytes.", nbytes);
  const size_t voxels[3] = { /* total # of voxels for whole output file */
    h.nbricks[0] * h.dims[0],
    h.nbricks[1] * h.dims[1],
    1 /* this is only for 2D writes! */
  };
  /* we have the same number of bricks in each dimension as in 3D... except
   * that there's only 1 brick in Z. */
  const size_t nbricks[3] = { h.nbricks[0], h.nbricks[1], 1 };
  size_t bpos[3];
  to3d(rank(), nbricks, bpos);

  struct writelist wl = wlist2d(voxels, offset, buf, nbytes, bpos, h.dims[0]);
  TRACE(netz, "writelist with %zu elements, merging into %d", wl.n, to);
  for(size_t i=0; i < wl.n; ++i) {
    wl.list[i]->aio_fildes = to;
    wl.list[i]->aio_sigevent.sigev_notify = SIGEV_NONE;
    wl.list[i]->aio_lio_opcode = LIO_WRITE;
  }
  errno = 0;
  if(lio_listio(LIO_WAIT, wl.list, wl.n, NULL) == -1) {
    WARN(netz, "listio failed: %d", errno);
      for(size_t i=0; i < wl.n; ++i) {
      const ssize_t bytes = aio_return(wl.list[i]);
      if(bytes <= 0) {
        ERR(netz, "write %zu failed: %d, %d", i, (int)bytes, errno);
      }
    }
  }
  free_writelist(wl);
}

void
exec(const char* fn, const void* buf, size_t n)
{
  if(fnmatch("*header*", fn, 0) == 0) {
    return; /* parse header when file is closed, not during write. */
  }
  assert(fnmatch("*Restart*", fn, 0) == 0);
  if(NULL == binfield) {
    tjfstart();
  }
  assert(binfield != NULL);
  for(size_t i=0; i < hdr.nfields; ++i) {
    size_t skip, nbytes;
    if(hdr.flds[i].out3d &&
       byteintersect(offset, offset+n, hdr.flds[i].lower, hdr.flds[i].upper,
                     &skip, &nbytes)) {
      assert(nbytes <= n);
      assert(skip < n);
      const char* pwrt = ((const char*)buf) + skip;
      errno = 0;
      const size_t written = fwrite(pwrt, 1, nbytes, binfield[i]);
      if(written != nbytes) {
        WARN(netz, "short write (%zu of %zu). errno=%d", written, n, errno);
        assert(0);
      }
      assert(!ferror(binfield[i]));
    }
    if(hdr.flds[i].slice &&
       byteintersect2d(offset,offset+n, hdr.flds[i].lower,hdr.flds[i].upper,
                       hdr.dims, hdr.flds[i].slice-1, &skip,&nbytes)) {
      assert(nbytes <= n);
      assert(skip < n);
      assert(slicefield[i]);
      const char* pwrt = ((const char*)buf) + skip;
      /* we don't want the raw file offset.  rather we want the offset in the
       * stream the user asked for.  that is the current offset sans the offset
       * of the current field, sans the offset of the slice. */
      const size_t slc_offset = (hdr.flds[i].slice-1)*hdr.dims[1]*hdr.dims[0] *
                                sizeof(float);
      const size_t strm_offset = offset+skip - hdr.flds[i].lower - slc_offset;
      writes2d(strm_offset, pwrt, nbytes, slicefield[i], hdr);
    }
  }
  offset += n;
}

/* 3D output works a little odd.  We have each rank write its own raw file,
 * then we reassemble it later.  This function is the 'reassemble it later'
 * part. */
static void
out3d()
{
  for(size_t i=0; i < hdr.nfields; ++i) {
    if(binfield[i]) {
      if(fclose(binfield[i]) != 0) {
        ERR(netz, "error closing field %s: %d", hdr.flds[i].name, errno);
      }
      /* now each of our N processes has written a file which contains a
       * single brick. let's merge all those bricks into a single file. */
      const size_t voxels[3] = { /* total for the whole domain */
        hdr.nbricks[0] * hdr.dims[0],
        hdr.nbricks[1] * hdr.dims[1],
        hdr.nbricks[2] * hdr.dims[2],
      };
      size_t bpos[3];
      to3d(rank(), hdr.nbricks, bpos);
      TRACE(netz, "layout(%zu): %zu %zu %zu", rank(),
            bpos[0], bpos[1], bpos[2]);
      struct writelist wl = find_destination(bpos, voxels, hdr.dims);
      char fname[256];
      snprintf(fname, 256, "%s.%zu", hdr.flds[i].name, rank());
      TRACE(netz, "got writelist with %zu elements. merging %s into %s",
            wl.n, fname, hdr.flds[i].name);
      apply_writelist(wl, hdr.dims, hdr.flds[i].name, fname);
      free_writelist(wl);
      if(rank() == 0) {
        create_nhdr(hdr.flds[i].name, voxels);
      }
    }
  }
}

void
finish(const char* fn)
{
  offset = 0; /* reset our current offset. */
  if(fnmatch("*header*", fn, 0) == 0) {
    free_header(&hdr);
    hdr = read_header(fn);
    return;
  }
  if(binfield) {
    out3d();
    free(binfield);
    binfield = NULL;
  }
  if(slicefield) {
    for(size_t i=0; i < hdr.nfields; ++i) {
      if(slicefield[i] != -1) {
        if(close(slicefield[i]) != 0) {
          ERR(netz, "error writing slicefield %zu: %d", i, errno);
        }
      }
    }
    free(slicefield);
    slicefield = NULL;
  }
}
