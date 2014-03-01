MPICC=mpicc
WARN=-Wall -Wextra
DBG:=-ggdb -fno-omit-frame-pointer
CFLAGS=-std=c99 -fPIC $(WARN) $(DBG)
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
# humorously, dlsym() actually violates the standard
CFLAGS_ORIDE=-std=c99 -ggdb -fPIC -Wall -Wextra $(DBG)
LDFLAGS=-Wl,--no-allow-shlib-undefined -Wl,--no-undefined
LDFLAGS:=-Wl,--no-undefined
LDLIBS=-ldl -lrt
obj=evfork.o forkenv.o override.o csv.o simplesitu.o vis.o parenv.mpi.o \
  runner.mpi.o setenv.mpi.o open.mpi.o debug.o \
  writebin.mpi.o parallel.mpi.o \
  enzo.mpi.o echo.mpi.o nek5k.mpi.o png.o \
  fproc.o posix.o h5.o

all: $(obj) libfp.so libsitu.so writecsv mpiwrapper envpar mpienv situ \
  mpifopen libecho.so libmpitee.so \
  libenzo.so libnek.so

analyze: $(obj)
	clang --analyze $(shell mpicc -showme:compile) -I/usr/include/python2.7 *.c
	rm -f *.plist # clang creates a bunch of these annoying files.
	cppcheck --quiet *.c

writecsv: csv.o
	$(CC) $^ -o $@ $(LDLIBS)

libfp.so: override.o
	$(CC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libsitu.so: debug.o fproc.o h5.o posix.o simplesitu.o
	$(CC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libecho.so: debug.o echo.mpi.o parallel.mpi.o
	$(MPICC) -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libmpitee.so: writebin.mpi.o
	$(MPICC) -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libenzo.so: debug.o enzo.mpi.o parallel.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libnek.so: debug.o nek5k.mpi.o parallel.mpi.o png.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS) -lpng

modtest: debug.o modified.o testmodified.o
	$(CC) -fPIC $^ -o $@ $(LDLIBS)

mpiwrapper: runner.mpi.o
	$(MPICC) -fPIC $^ -o $@ $(LDLIBS)
mpienv: setenv.mpi.o
	$(MPICC) -fPIC $^ -o $@ $(LDLIBS)
mpifopen: open.mpi.o
	$(MPICC) -fPIC $^ -o $@ $(LDLIBS)

envpar: parenv.mpi.o
	$(MPICC) -fPIC $^ -o $@ $(LDLIBS)

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

situ: forkenv.o evfork.o
	$(CC) -fPIC $^ -o $@

clean:
	rm -f $(obj) \
    libecho.so libenzo.so libfp.so libmpitee.so libnek.so \
    libsitu.so \
    envpar mpienv mpifopen mpiwrapper situ writecsv \
    modtest

override.o: override.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
simplesitu.o: simplesitu.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
