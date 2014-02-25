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
obj=binaryio.o

all: $(obj) f95-write-array

f95-write-array: binaryio.o
	$(FC) $^ -o $@

clean:
	rm -f $(obj) \
    f95-write-array

%.o: %.f95
	$(FC) $(FFLAGS) -c $^ -o $@
