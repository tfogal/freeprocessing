MPICC=mpicc
WARN=-Wall -Wextra
DBG:=-ggdb -fno-omit-frame-pointer
CFLAGS=-std=c99 -fPIC $(WARN) -I../../ $(DBG)
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
LDFLAGS=-Wl,--no-allow-shlib-undefined -Wl,--no-undefined
LDFLAGS:=-Wl,--no-undefined
LDLIBS=-ldl -lrt
obj=ctest.mpi.o netz.mpi.o ../../debug.o ../../parallel.mpi.o
cfanalyze:=$(shell mpicc -showme:compile) -I/usr/include/python2.7

all: $(obj) libnetz.so hacktest

analyze: $(obj)
	clang --analyze $(CFLAGS) $(cfanalyze) *.c
	rm -f *.plist # clang creates a bunch of these annoying files.
	cppcheck --quiet *.c

libnetz.so: ../../debug.o netz.mpi.o ../../parallel.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

hacktest: ctest.mpi.o ../../debug.o netz.mpi.o ../../parallel.mpi.o
	$(MPICC) -fPIC $^ -o $@ $(LDLIBS)

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(obj) libnetz.so hacktest
