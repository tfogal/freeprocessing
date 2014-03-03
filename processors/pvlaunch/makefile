MPICC=mpicc
WARN=-Wall -Wextra
DBG:=-ggdb -fno-omit-frame-pointer
CFLAGS=-std=c99 -fPIC $(WARN) -I../../ $(DBG)
LDFLAGS:=-Wl,--no-undefined
LDLIBS=-ldl -lrt
obj=pvlaunch.mpi.o vis.o ../../parallel.mpi.o
cfanalyze:=$(shell mpicc -showme:compile) -I/usr/include/python2.7

all: $(obj) libpv.so

analyze: $(obj)
	clang --analyze $(CFLAGS) $(cfanalyze) *.c
	rm -f *.plist # clang creates a bunch of these annoying files.
	cppcheck --quiet *.c

libpv.so: pvlaunch.mpi.o vis.o ../../parallel.mpi.o
	$(MPICC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(obj) libpv.so
