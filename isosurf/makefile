MPICC=mpicc
WARN=-Wall -Wextra -fno-omit-frame-pointer
OPT=-Ofast -DNDEBUG -fopenmp -march=native -mtune=native -msse4
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb $(OPT)
CXXFLAGS=-std=c++11 -fPIC $(WARN) -ggdb $(OPT)
LDFLAGS:=-Wl,--no-undefined $(OPT)
LDLIBS=
obj=isosurf.o mcubes.o MC.o

all: $(obj) fpiso.so

fpiso.so: ../debug.o isosurf.o mcubes.o MC.o
	$(CXX) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(obj) fpiso.so
