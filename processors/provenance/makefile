WARN=-Wall -Wextra
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb -I../../
LDFLAGS:=-Wl,--no-undefined
LDLIBS=-ldl -lrt
obj=trackdata.o

all: $(obj) libtrack.so

libtrack.so: trackdata.o ../../debug.o
	$(CC) -ggdb -fPIC -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(obj) libtrack.so
