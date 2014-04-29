#include <limits.h>
#include <stdio.h>

/* Where we will store our minimum and maximum. */
static int minmax[2];

/* Initialize routine.  Invoked every time the program opens a new
 * output file.  If this is invoked more than you would like, try filtering
 * things out by judicious use of your 'situ.cfg' file.  If this is never
 * getting invoked, your 'situ.cfg' might have an incorrect pattern.  Try
 * setting the environment variable "LIBSITU_DEBUG" to the value "opens=+trace"
 * to debug your patterns. */
void
file(const char* filename) {
  (void)filename;
  minmax[0] = INT_MAX;
  minmax[1] = INT_MIN;
}

/* "Meat" of the work.  This function is called every time the program performs
 * a write.  You get essentially the same parameters that `write` does.
 * Most implementations will want to ignore the "fn" parameter, but you could
 * do a match against it to differentiate streams, if you desire.  This
 * filename is guaranteed to match that given to "file", earlier. */
void
exec(const char* fn, const void* buf, size_t n) {
  (void)fn;
  const int* data = (const int*)buf;
  const size_t elems = n / sizeof(int);
  for(size_t i=0; i < elems; ++i) {
    minmax[0] = data[i] < minmax[0] ? data[i] : minmax[0];
    minmax[1] = data[i] > minmax[1] ? data[i] : minmax[1];
  }
  printf("The data range is: [%d:%d]\n", minmax[0], minmax[1]);
}

/* Cleanup routine.  The program is now finished with the given file.  This
 * signals that you should output any computations you have done and deallocate
 * any resources you have in doing so. */
void
finish(const char* fn) {
  printf("The data range of %s is: [%d:%d]\n", fn, minmax[0], minmax[1]);
  /* This is unnecessary, but it can be useful for debugging: a negative 42 in
   * the output is then a red flag that something was not (re)initialized
   * properly. */
  minmax[0] = minmax[1] = -42;
}
