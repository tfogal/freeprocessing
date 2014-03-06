AUTOMAKE_OPTIONS = no-dependencies
ACLOCAL_AMFLAGS = -I m4

LIB_LTLIBRARIES = libsitu.la
libsitu_la_SOURCES = \
  debug.c \
  fproc.c \
  h5.c \
  posix.c \
  simplesitu.c
libsitu_la_LIBADD = -ldl -lrt @LTLIBOBJS@

# for dlopen libs:
# pkglib_LTLIBRARIES = ...
# libname_la_SOURCES = ....
# libname_la_LDFLAGS = -module

# set it to an empty value, because automake complains if one tries to '+=' on
# a variable which wasn't yet defined.
pkglib_LTLIBRARIES =

include processors/netz/mk.inc
