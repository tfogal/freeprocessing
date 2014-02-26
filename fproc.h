#ifndef FREEPROCESSING_FPROC_H
#define FREEPROCESSING_FPROC_H

#include <stdbool.h>
#include <stdio.h>

typedef void (ffqn)(const char* fn);
typedef void (tfqn)(const char* fn, const void* buf, size_t n);
typedef void (cfqn)(const char* fn);
typedef void (mdfqn)(const char* fn, const size_t d[3], int);
/* a library to load and exec as we move data, tee-style. */
struct teelib {
  char* pattern;
  void* lib;
  ffqn* file;
  tfqn* transfer;
  cfqn* finish;
  mdfqn* metadata;
};
#define MAX_FREEPROCS 128U
extern struct teelib transferlibs[MAX_FREEPROCS];

/** @returns NULL when it failed to read a processor (e.g. on EOF, error) */
struct teelib* load_processor(FILE* from);
void load_processors(struct teelib* tlibs, FILE* from);
void unload_processors(struct teelib* tlibs);

/* do any libraries match the given pattern? */
bool matches(const struct teelib*, const char* ptrn);

/* call our 'file' function on all libraries that match 'ptrn'. */
void file(const struct teelib* tlibs, const char* ptrn);
/* call our 'stream' function on all libraries that match 'ptrn'. */
void stream(const struct teelib* tlibs, const char* ptrn, const void* buf,
            const size_t n);
enum FPDataType { FP_INT8=0, FP_INT16, FP_INT32, FP_INT64,
                  FP_UINT8, FP_UINT16, FP_UINT32, FP_UINT64,
                  FP_FLOAT32, FP_FLOAT64 };
/* call our 'metadata' function on all libraries that match 'ptrn'. */
void metadata(const struct teelib* tlibs, const char* ptrn,
              const size_t dims[3], enum FPDataType);
/* call our 'finish' function on all libraries that match 'ptrn'. */
void finish(const struct teelib* tlibs, const char* ptrn);

#endif
