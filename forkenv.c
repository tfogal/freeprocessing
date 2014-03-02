#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evfork.h"

static void rebuild_args(size_t argc, char* argv[], char* subv[]);

int
main(int argc, char* argv[])
{
  assert(argc > 0);
  char* subv[argc]; memset(subv, 0, sizeof(char*)*argc);
  rebuild_args((size_t)argc, argv, subv);
  if(setenv("LD_PRELOAD", "./libsitu.so", 1) != 0) {
    fprintf(stderr, "failed setting 'LD_PRELOAD' env var.\n");
    exit(EXIT_FAILURE);
  }
  launch(subv);
}

static void
rebuild_args(size_t argc, char* argv[], char* subv[])
{
  /* argv[0] is the name of this program.
   * argv[1] is the name of the program the user wanted to run, "child"
   * argv[x] for x > 1 are the arguments of "child". */
  for(size_t i=1; i < argc; ++i) {
    subv[i-1] = argv[i];
  }
}
