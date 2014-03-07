pkglib_LTLIBRARIES += libnetz.la
libnetz_la_SOURCES = \
  $(top_srcdir)/processors/netz/netz.mpic
libnetz_la_LDFLAGS = -module
libnetz_la_LIBADD = -lrt @LTLIBOBJS@
libnetz_la_CFLAGS = -I./
