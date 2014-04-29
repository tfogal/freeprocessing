static int minmax[2];

void
file(const char* fn) {
  (void)fn;
  minmax[0] = INT_MAX;
  minmax[1] = INT_MIN;
}
void
finish(const char* fn) {
  printf("The data range of %s is: [%d:%d]\n", fn, minmax[0], minmax[1]);
}
