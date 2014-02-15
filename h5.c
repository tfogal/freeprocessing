#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE 1
#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include "debug.h"
#include "fproc.h"

DECLARE_CHANNEL(hdf5);

/* types for HDF5 */
typedef int herr_t;
typedef int hid_t;
typedef unsigned long long hsize_t;

typedef hid_t h5fcreatefqn(const char*, unsigned, hid_t, hid_t);
typedef hid_t h5gcreate1fqn(hid_t, const char*, size_t);
typedef hid_t h5screate_simplefqn(int, const hsize_t*, const hsize_t*);
typedef herr_t h5sclosefqn(hid_t);
typedef hid_t h5dopen1fqn(hid_t, const char*);
typedef hid_t h5dopen2fqn(hid_t, const char*, hid_t);
typedef herr_t h5dclosefqn(hid_t);
typedef hid_t h5dcreate1fqn(hid_t, const char*, hid_t, hid_t, hid_t);
typedef hid_t h5dcreate2fqn(hid_t, const char*, hid_t, hid_t, hid_t);
typedef herr_t h5dwritefqn(hid_t, hid_t, hid_t, hid_t, hid_t, const void*);

static h5fcreatefqn* h5fcreatef = NULL;
static h5gcreate1fqn* h5gcreate1f = NULL;
static h5screate_simplefqn* h5screatesimplef = NULL;
static h5sclosefqn* h5sclosef = NULL;
static h5dopen1fqn* h5dopen1f  = NULL;
static h5dopen2fqn* h5dopen2f  = NULL;
static h5dclosefqn* h5dclosef = NULL;
static h5dcreate1fqn* h5dcreate1f = NULL;
static h5dcreate2fqn* h5dcreate2f = NULL;
static h5dwritefqn* h5dwritef = NULL;

struct openhdffile {
  char* name;
  size_t dims[3]; /* dims of the next impending write */
  size_t sz; /* sizeof() the type being written */
};
struct h5spaces {
  hid_t id;
  size_t dims[3];
};
struct h5meta {
  hid_t dset;
  hid_t space;
  char* name;
};

#define MAX_FILES 1024
static const size_t N_SPACES = MAX_FILES;
static struct h5spaces spaces[MAX_FILES] = {{0, {0,0,0}}};
static struct h5meta metah5[MAX_FILES] = {{0, 0, NULL}};

__attribute__((constructor(250))) static void
fp_h5_init()
{
  h5fcreatef = dlsym(RTLD_NEXT, "H5Fcreate");
  h5gcreate1f = dlsym(RTLD_NEXT, "H5Gcreate1");
  h5screatesimplef = dlsym(RTLD_NEXT, "H5Screate_simple");
  h5sclosef = dlsym(RTLD_NEXT, "H5Sclose");
  h5dopen1f  = dlsym(RTLD_NEXT, "H5Dopen1");
  h5dopen2f  = dlsym(RTLD_NEXT, "H5Dopen2");
  h5dclosef = dlsym(RTLD_NEXT, "H5Dclose");
  h5dcreate1f = dlsym(RTLD_NEXT, "H5Dcreate1");
  h5dcreate2f = dlsym(RTLD_NEXT, "H5Dcreate2");
  h5dwritef = dlsym(RTLD_NEXT, "H5Dwrite");
}

hid_t
H5Fcreate(const char* name, unsigned flags, hid_t fcpl, hid_t fapl)
{
  const hid_t rv = h5fcreatef(name, flags, fcpl, fapl);
  TRACE(hdf5, "(%s, %u, %d, %d) = %d", name, flags, fcpl, fapl, rv);
  return rv;
}

hid_t
H5Gcreate1(hid_t loc, const char* name, size_t hint)
{
  const hid_t rv = h5gcreate1f(loc, name, hint);
  TRACE(hdf5, "(%d, %s, %zu) = %d", loc, name, hint, rv);
  return rv;
}

hid_t
H5Screate_simple(int rank, const hsize_t* dims, const hsize_t* maxdims)
{
  assert(h5screatesimplef);
  const hid_t rv = h5screatesimplef(rank, dims, maxdims);
  char dstr[256];
  char mstr[256];
  strcpy(dstr, "[");
  strcpy(mstr, "[");
  for(int i=0; i < rank; ++i) {
    char s[32];
    snprintf(s, 32, "%llu ", dims[i]);
    strncat(dstr, s, 32);
    if(maxdims) { /* maxdims can be null */
      snprintf(s, 32, "%llu ", maxdims[i]);
      strncat(mstr, s, 32);
    }
  }
  strncat(dstr, "]", 2);
  strncat(mstr, "]", 2);
  TRACE(hdf5, "(%d, %s, %s) = %d", rank, dstr, mstr, rv);
  /* for now, we only care about 3D data; so skip if the rank is small. */
  for(size_t i=0; rank >= 3 && i < N_SPACES; ++i) {
    if(spaces[i].id == 0 || spaces[i].id == -1) {
      spaces[i].id = rv;
      assert(rank >= 3);
      spaces[i].dims[0] = dims[0];
      spaces[i].dims[1] = dims[1];
      spaces[i].dims[2] = dims[2];
    }
  }
  return rv;
}

herr_t
H5Sclose(hid_t id)
{
  const herr_t rv = h5sclosef(id);
  assert(id > 0);
  for(size_t i=0; i < N_SPACES; ++i) {
    if(spaces[i].id == id) {
      TRACE(hdf5, "closing data space %d", id);
      spaces[i].id = -1;
      spaces[i].dims[0] = spaces[i].dims[1] = spaces[i].dims[2] = 0;
      break;
    }
  }
  return rv;
}

hid_t
H5Dopen1(hid_t loc, const char* name)
{
  TRACE(hdf5, "(%d, %s)", loc, name);
  return h5dopen1f(loc, name);
}

hid_t
H5Dopen2(hid_t loc, const char* name, hid_t dapl)
{
  TRACE(hdf5, "(%d, %s, %d)", loc, name, dapl);
  return h5dopen2f(loc, name, dapl);
}

static size_t
find_dataspace(hid_t id, const struct h5spaces* sp, size_t n)
{
  for(size_t i=0; i < n; ++i) {
    if(sp[i].id == id) {
      return i;
    }
  }
  return n;
}

hid_t
H5Dcreate1(hid_t loc, const char *name, hid_t type, hid_t space, hid_t dcpl)
{
  hid_t rv = h5dcreate1f(loc, name, type, space, dcpl);
  TRACE(hdf5, "(%d, %s, %d, %d, %d) = %d",loc, name, type, space, dcpl, rv);
  const size_t sid = find_dataspace(space, spaces, N_SPACES);
  if(sid == N_SPACES) { /* not found */
    return rv;
  }
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(metah5[i].dset <= 0 && metah5[i].space <= 0) {
      assert(metah5[i].name == NULL);
      metah5[i].dset = rv;
      metah5[i].space = space;
      metah5[i].name = strdup(name);
      break;
    }
  }
  return rv;
}

hid_t
H5Dcreate2(hid_t loc, const char* name, hid_t type, hid_t space, hid_t dcpl)
{
  TRACE(hdf5, "(%d, %s, %d, %d, %d)",loc, name, type, space, dcpl);
  return h5dcreate2f(loc, name, type, space, dcpl);
}

herr_t
H5Dclose(hid_t id)
{
  /* there's an issue here that if two data sets are open with different names,
   * we close the first one that matches the ID, which might be the other
   * dataset.
   * this hasn't been an issue for the software we've instrumented so far... */
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(metah5[i].dset == id) {
      TRACE(hdf5, "cleaning up dset %d (%s)", id, metah5[i].name);
      free(metah5[i].name);
      metah5[i].name = NULL;
      metah5[i].dset = -1;
      metah5[i].space = -1;
      break;
    }
  }
  return h5dclosef(id);
}

herr_t
H5Dwrite(hid_t dset, hid_t memtype, hid_t memspace, hid_t filespace,
         hid_t plist, const void* buf)
{
  TRACE(hdf5, "(%d, %d, %d, %d, %d, %p)", dset, memtype, memspace,
        filespace, plist, buf);
  for(size_t i=0; i < MAX_FILES; ++i) {
    if(metah5[i].dset == dset) {
      const size_t sidx = find_dataspace(metah5[i].space, spaces, N_SPACES);
      TRACE(hdf5, "h-write %s [%zu %zu %zu] %p", metah5[i].name,
            spaces[sidx].dims[0], spaces[sidx].dims[1], spaces[sidx].dims[2],
            buf);
      gridsize(transferlibs, metah5[i].name, spaces[sidx].dims);
      /* sizeof(double) is a hack; TODO pull it from HDF5. */
      const size_t n = spaces[sidx].dims[0] * spaces[sidx].dims[1] *
                       spaces[sidx].dims[2] * sizeof(double);
      stream(transferlibs, metah5[i].name, buf, n);
      break;
    }
  }
  return h5dwritef(dset, memtype, memspace, filespace, plist, buf);
}
