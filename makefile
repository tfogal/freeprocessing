MPICC=mpicc
WARN=-Wall -Wextra
PY=-I/usr/include/python2.7
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb -I/usr/include/python2.7
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
# humorously, dlsym() actually violates the standard
CFLAGS_ORIDE=-std=c99 -ggdb -fPIC -Wall -Wextra
LDFLAGS=-Wl,--no-allow-shlib-undefined -Wl,--no-undefined
LDFLAGS:=-Wl,--no-undefined
LDLIBS=-ldl -lrt
obj=evfork.o forkenv.o override.o csv.o simplesitu.o vis.o parenv.mpi.o \
  runner.mpi.o setenv.mpi.o open.mpi.o binaryio.o debug.o \
  writebin.mpi.o netz.mpi.o parallel.mpi.o ctest.mpi.o modified.o \
  testmodified.o enzo.mpi.o echo.mpi.o nek5k.mpi.o png.o tonumpy.mpi.o \
  fproc.o posix.o h5.o

all: $(obj) libfp.so libsitu.so writecsv mpiwrapper envpar mpienv situ \
  mpifopen f95-write-array libecho.so libmpitee.so libnetz.so hacktest \
  modtest libenzo.so libnek.so libtopython.so

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

libnetz.so: debug.o netz.mpi.o parallel.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libenzo.so: debug.o enzo.mpi.o parallel.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libnek.so: debug.o nek5k.mpi.o parallel.mpi.o png.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS) -lpng

libtopython.so: debug.o parallel.mpi.o tonumpy.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS) -lpython2.7

hacktest: ctest.mpi.o debug.o netz.mpi.o parallel.mpi.o
	$(MPICC) -fPIC $^ -o $@ $(LDLIBS)

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

f95-write-array: binaryio.o
	$(FC) $^ -o $@

clean:
	rm -f $(obj) libfp.so libsitu.so libecho.so libmpitee.so libenzo.so \
    envpar mpienv mpifopen mpiwrapper situ writecsv f95-write-array

override.o: override.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
simplesitu.o: simplesitu.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@

%.o: %.f95
	$(FC) $(FFLAGS) -c $^ -o $@
