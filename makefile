MPICC=mpicc
WARN=-Wall -Wextra -pedantic
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
# humorously, dlsym() actually violates the standard
CFLAGS_ORIDE=-std=c99 -ggdb -fPIC -Wall -Wextra
obj=evfork.o forkenv.o override.o csv.o simplesitu.o vis.o parenv.mpi.o \
  runner.mpi.o setenv.mpi.o open.mpi.o binaryio.o

all: $(obj) libfp.so libsitu.so writecsv mpiwrapper envpar mpienv situ \
  mpifopen f95-write-array

writecsv: csv.o
	$(CC) $^ -o $@ -ldl

libfp.so: override.o
	$(CC) -fPIC -shared $^ -o $@ -ldl

libsitu.so: simplesitu.o vis.o
	$(CC) -fPIC -shared $^ -o $@ -ldl

mpiwrapper: runner.mpi.o
	$(MPICC) -fPIC $^ -o $@ -ldl
mpienv: setenv.mpi.o
	$(MPICC) -fPIC $^ -o $@ -ldl
mpifopen: open.mpi.o
	$(MPICC) -fPIC $^ -o $@ -ldl

envpar: parenv.mpi.o
	$(MPICC) -fPIC $^ -o $@ -ldl

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

situ: forkenv.o evfork.o
	$(CC) -fPIC $^ -o $@

f95-write-array: binaryio.o
	$(FC) $^ -o $@

clean:
	rm -f $(obj) libfp.so libsitu.so \
    envpar mpienv mpifopen mpiwrapper situ writecsv f95-write-array

override.o: override.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
simplesitu.o: simplesitu.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@

%.o: %.f95
	$(FC) $(FFLAGS) -c $^ -o $@
