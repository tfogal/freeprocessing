MPICC=mpicc
WARN=-Wall -Wextra
PY=-I/usr/include/python2.7
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb $(PY) -I../../
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
LDFLAGS=-Wl,--no-allow-shlib-undefined -Wl,--no-undefined
LDFLAGS:=-Wl,--no-undefined
LDLIBS=-ldl -lrt
obj=tonumpy.mpi.o ../../debug.o ../../parallel.mpi.o

all: $(obj) libtopython.so

analyze: $(obj)
	clang --analyze $(shell mpicc -showme:compile) -I/usr/include/python2.7 *.c
	rm -f *.plist # clang creates a bunch of these annoying files.
	cppcheck --quiet *.c

libtopython.so: ../../debug.o ../../parallel.mpi.o tonumpy.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS) -lpython2.7

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(obj) libtopython.so
