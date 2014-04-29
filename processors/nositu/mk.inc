pkglib_LTLIBRARIES += nositu.la
nositu_la_SOURCES = \
  $(top_srcdir)/debug.c \
  $(top_srcdir)/processors/nositu/launcher.c \
  $(top_srcdir)/processors/nositu/practice.c
nositu_la_LDFLAGS = -module
nositu_la_LIBADD = -lrt @LTLIBOBJS@
