MPICC=mpicc
WARN=-Wall -Wextra
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb
CXXFLAGS=-std=c++11 -fPIC $(WARN) -ggdb
LDFLAGS:=-Wl,--no-undefined
LDLIBS=
obj=isosurf.o mcubes.o MC.o

all: $(obj) fpiso.so

fpiso.so: ../debug.o isosurf.o mcubes.o MC.o
	$(CXX) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(obj) fpiso.so
