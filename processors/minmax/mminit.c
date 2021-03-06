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
void
exec(const char* fn, const void* buf, size_t n) {
  (void)fn;
  const int* data = (const int*)buf;
  const size_t elems = n / sizeof(int);
  for(size_t i=0; i < elems; ++i) {
    minmax[0] = data[i] < minmax[0] ? data[i] : minmax[0];
    minmax[1] = data[i] > minmax[1] ? data[i] : minmax[1];
  }
}
