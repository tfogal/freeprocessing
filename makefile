MPICC=mpicc
WARN=-Wall -Wextra
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
# humorously, dlsym() actually violates the standard
CFLAGS_ORIDE=-std=c99 -ggdb -fPIC -Wall -Wextra
LDFLAGS=-Wl,--no-allow-shlib-undefined -Wl,--no-undefined
LDLIBS=-ldl
obj=evfork.o forkenv.o override.o csv.o simplesitu.o vis.o parenv.mpi.o \
  runner.mpi.o setenv.mpi.o open.mpi.o binaryio.o debug.o echo.o

all: $(obj) libfp.so libsitu.so writecsv mpiwrapper envpar mpienv situ \
  mpifopen f95-write-array libecho.so

writecsv: csv.o
	$(CC) $^ -o $@ $(LDLIBS)

libfp.so: override.o
	$(CC) -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libsitu.so: debug.o simplesitu.o vis.o
	$(CC) -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

libecho.so: echo.o
	$(CC) -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

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
	rm -f $(obj) libfp.so libsitu.so libecho.so \
    envpar mpienv mpifopen mpiwrapper situ writecsv f95-write-array

override.o: override.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
simplesitu.o: simplesitu.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@

%.o: %.f95
	$(FC) $(FFLAGS) -c $^ -o $@
