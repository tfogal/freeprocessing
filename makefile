WARN=-Wall -Wextra -pedantic
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb
# humorously, dlsym() actually violates the standard
CFLAGS_ORIDE=-std=c99 -ggdb -fPIC -Wall -Wextra
obj=override.o csv.o

all: $(obj) libfp.so writecsv

writecsv: csv.o
	$(CC) $^ -o $@ -ldl

libfp.so: override.o
	$(CC) -shared $^ -o $@ -ldl

clean:
	rm -f $(obj) libfp.so writecsv

override.o: override.c
	$(CC) -c $(CFLAGS_ORIDE) $^ -o $@
