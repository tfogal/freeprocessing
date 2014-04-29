if PYTHON
  pkglib_LTLIBRARIES += freepython.la
endif
freepython_la_SOURCES = \
  $(top_srcdir)/processors/python/tonumpy.mpic
freepython_la_LDFLAGS = -module @PYTHON_LDFLAGS@
freepython_la_LIBADD = -lrt @LTLIBOBJS@ @PYTHON_EXTRA_LIBS@
freepython_la_CFLAGS = -I./ @PYTHON_CPPFLAGS@
