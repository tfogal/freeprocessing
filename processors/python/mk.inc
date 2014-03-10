if PYTHON
  pkglib_LTLIBRARIES += libfreepython.la
endif
libfreepython_la_SOURCES = \
  $(top_srcdir)/processors/python/tonumpy.mpic
libfreepython_la_LDFLAGS = -module @PYTHON_LDFLAGS@
libfreepython_la_LIBADD = -lrt @LTLIBOBJS@ @PYTHON_EXTRA_LIBS@
libfreepython_la_CFLAGS = -I./ @PYTHON_CPPFLAGS@
