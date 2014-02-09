#ifndef FREEPROCESSING_FPROC_H
#define FREEPROCESSING_FPROC_H

typedef void (tfqn)(const char* fn, const void* buf, size_t n);
typedef void (cfqn)(const char* fn);
typedef void (szfqn)(const char* fn, const size_t d[3]);
/* a library to load and exec as we move data, tee-style. */
struct teelib {
  char* pattern;
  void* lib;
  tfqn* transfer;
  cfqn* finish;
  szfqn* gridsize;
};
#define MAX_FREEPROCS 128U

/** @returns NULL when it failed to read a processor (e.g. on EOF, error) */
struct teelib* load_processor(FILE* from);

void load_processors(struct teelib* tlibs, const char* cfgfile);

#endif
