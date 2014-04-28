pkglib_LTLIBRARIES += libhello.la
libhello_la_SOURCES = \
  $(top_srcdir)/processors/hello/hello.c
libhello_la_LDFLAGS = -module
libhello_la_LIBADD = -lrt @LTLIBOBJS@
libhello_la_CFLAGS = -I./

bin_PROGRAMS += hw
hw_SOURCES = $(top_srcdir)/processors/output-int.c
