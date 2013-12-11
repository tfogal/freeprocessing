MPICC=mpicc
WARN=-Wall -Wextra -pedantic
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb
# humorously, dlsym() actually violates the standard
CFLAGS_ORIDE=-std=c99 -ggdb -fPIC -Wall -Wextra
obj=override.o csv.o simplesitu.o vis.o runner.mpi.o

all: $(obj) libfp.so libsitu.so writecsv mpiwrapper

writecsv: csv.o
	$(CC) $^ -o $@ -ldl

libfp.so: override.o
	$(CC) -fPIC -shared $^ -o $@ -ldl

libsitu.so: simplesitu.o vis.o
	$(CC) -fPIC -shared $^ -o $@ -ldl

mpiwrapper: runner.mpi.o
	$(MPICC) -fPIC $^ -o $@ -ldl

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(obj) libfp.so libsitu.so writecsv mpiwrapper

override.o: override.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
simplesitu.o: simplesitu.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
