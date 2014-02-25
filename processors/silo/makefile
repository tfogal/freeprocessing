MPICC=mpicc
WARN=-Wall -Wextra
SILO=$(HOME)/sw
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb -I$(SILO)/include -I../../
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
LDFLAGS=-Wl,--no-allow-shlib-undefined -Wl,--no-undefined
LDFLAGS:=-Wl,--no-undefined -L$(SILO)/lib
LDLIBS=-ldl -lrt -lsilo -lm
obj=silo.o json.o jsdd.o test.o

all: $(obj) libsilositu.so Testjs

libsilositu.so: ../../debug.o jsdd.o json.o silo.o
	$(CC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

Testjs: ../../debug.o jsdd.o json.o test.o
	$(CC) -ggdb -fPIC $^ -o $@ $(LDFLAGS) $(LDLIBS)

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(obj) libsilositu.so Testjs
