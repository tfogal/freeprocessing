MPICC=mpicc
WARN=-Wall -Wextra
PY=-I/usr/include/python2.7
ADIOS=$(HOME)/sw/adios
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb $(PY) -I$(ADIOS)/include
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
LDFLAGS=-Wl,--no-allow-shlib-undefined -Wl,--no-undefined
LDFLAGS:=-Wl,--no-undefined -L$(ADIOS)/lib
LDLIBS=-ldl -lrt -ladios -lmxml
obj=adios.mpi.o ../debug.o ../parallel.mpi.o

all: $(obj) toadios.so

toadios.so: adios.mpi.o ../debug.o ../parallel.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(obj) toadios.so
