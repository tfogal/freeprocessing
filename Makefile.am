ACLOCAL_AMFLAGS = -I m4
LIB_LTLIBRARIES = libsitu.la
libsitu_la_SOURCES = \
  debug.c \
  fproc.c \
  h5.c \
  posix.c \
  simplesitu.c
libsitu_la_LIBADD = -ldl -lrt @LTLIBOBJS@
# fixme don't hack in gnu-specific option for c99 mode.
libsitu_la_CFLAGS = -std=c99

# for dlopen libs:
# pkglib_LTLIBRARIES = ...
# libname_la_SOURCES = ....
# libname_la_LDFLAGS = -module
