pkglib_LTLIBRARIES += bad-minmax.la minmax.la
bad_minmax_la_SOURCES = $(top_srcdir)/processors/minmax/incorrect-minmax.c
bad_minmax_la_LDFLAGS = -module
bad_minmax_la_LIBADD = -lrt @LTLIBOBJS@
bad_minmax_la_CFLAGS = -I./

minmax_la_SOURCES = $(top_srcdir)/processors/minmax/minmax.c
minmax_la_LDFLAGS = -module
minmax_la_LIBADD = -lrt @LTLIBOBJS@
minmax_la_CFLAGS = -I./
