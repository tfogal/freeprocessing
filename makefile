MPICC=mpicc
WARN=-Wall -Wextra -pedantic
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb
# humorously, dlsym() actually violates the standard
CFLAGS_ORIDE=-std=c99 -ggdb -fPIC -Wall -Wextra
obj=evfork.o forkenv.o override.o csv.o simplesitu.o vis.o parenv.mpi.o \
  runner.mpi.o setenv.mpi.o

all: $(obj) libfp.so libsitu.so writecsv mpiwrapper envpar mpienv situ

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

envpar: parenv.mpi.o
	$(MPICC) -fPIC $^ -o $@ -ldl

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

situ: forkenv.o evfork.o
	$(CC) -fPIC $^ -o $@

clean:
	rm -f $(obj) libfp.so libsitu.so writecsv mpiwrapper envpar

override.o: override.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
simplesitu.o: simplesitu.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
