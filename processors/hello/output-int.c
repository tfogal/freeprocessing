#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char* argv[]) {
  if(argc != 2) {
    fprintf(stderr, "expecting integer argument!\n");
    return EXIT_FAILURE;
  }
  const int value = atoi(argv[1]);
  FILE* fp = fopen("test-int", "wb");
  if(!fp) {
    fprintf(stderr, "could not open 'test-int' file for writing!\n");
    return EXIT_FAILURE;
  }
  if(fwrite(&value, sizeof(int), 1, fp) != 1) {
    fprintf(stderr, "oh noes!  write failed!\n");
    fclose(fp);
    return EXIT_FAILURE;
  }
  if(fclose(fp) != 0) {
    fprintf(stderr, "file close failed; data didn't make it to disk\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
