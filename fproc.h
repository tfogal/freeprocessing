#ifndef FREEPROCESSING_FPROC_H
#define FREEPROCESSING_FPROC_H

#include <stdbool.h>
#include <stdio.h>

typedef void (ffqn)(const char* fn);
typedef void (tfqn)(const char* fn, const void* buf, size_t n);
typedef void (cfqn)(const char* fn);
typedef void (szfqn)(const char* fn, const size_t d[3]);
/* a library to load and exec as we move data, tee-style. */
struct teelib {
  char* pattern;
  void* lib;
  ffqn* file;
  tfqn* transfer;
  cfqn* finish;
  szfqn* gridsize;
};
#define MAX_FREEPROCS 128U
extern struct teelib transferlibs[MAX_FREEPROCS];

/** @returns NULL when it failed to read a processor (e.g. on EOF, error) */
struct teelib* load_processor(FILE* from);
void load_processors(struct teelib* tlibs, const char* cfgfile);
void unload_processors(struct teelib* tlibs);

/* do any libraries match the given pattern? */
bool matches(const struct teelib*, const char* ptrn);

/* call our 'file' function on all libraries that match 'ptrn'. */
void file(const struct teelib* tlibs, const char* ptrn);
/* call our 'stream' function on all libraries that match 'ptrn'. */
void stream(const struct teelib* tlibs, const char* ptrn, const void* buf,
            const size_t n);
/* call our 'grid_size' function on all libraries that match 'ptrn'. */
void gridsize(const struct teelib* tlibs, const char* ptrn,
              const size_t dims[3]);
/* call our 'finish' function on all libraries that match 'ptrn'. */
void finish(const struct teelib* tlibs, const char* ptrn);

#endif
