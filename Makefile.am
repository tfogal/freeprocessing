AUTOMAKE_OPTIONS = no-dependencies
ACLOCAL_AMFLAGS = -I m4

# todo: we don't want std=c99 in there, but AX_MPI changes *CC*, not CFLAGS!
# so we don't know the real option for c99, need a workaround.
.mpic.lo:
	$(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
    --mode=compile $(MPICC) -x c $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
    $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -std=c99 -c -o $@ $<

LIB_LTLIBRARIES = libsitu.la
libsitu_la_SOURCES = \
  debug.c \
  fproc.c \
  h5.c \
  posix.c \
  simplesitu.c
libsitu_la_LIBADD = -ldl -lrt @LTLIBOBJS@

bin_PROGRAMS = situ
situ_SOURCES = forkenv.c evfork.c

# set it to an empty value, because automake complains if one tries to '+=' on
# a variable which wasn't yet defined.
pkglib_LTLIBRARIES =

include processors/netz/mk.inc
