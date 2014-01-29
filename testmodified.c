#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "debug.h"
#include "modified.h"

DECLARE_CHANNEL(tst);
static watch* wtch = NULL;

int
main(int argc, char* argv[])
{
  if(argc != 2) {
    ERR(tst, "need arg: filename to watch\n");
    return EXIT_FAILURE;
  }
  wtch = Watch(argv[1]);
  if(NULL == wtch) {
    ERR(tst, "could not create watch of %s: %d\n", argv[1], errno);
    return EXIT_FAILURE;
  }
  do {
    if(Modified(wtch)) {
      printf("%s modified.\n", argv[1]);
    }
    sleep(2);
  } while(1);
  Unwatch(wtch);
  return EXIT_SUCCESS;
}
