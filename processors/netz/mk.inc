pkglib_LTLIBRARIES += libnetz.la
libnetz_la_SOURCES = \
  netz-mpi.c
libnetz_la_LDFLAGS = -module
libnetz_la_LIBADD = -lrt @LTLIBOBJS@
