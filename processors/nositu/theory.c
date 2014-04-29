void
finish(const char* fn) {
  char cmd[1024];
  snprintf(cmd, 1024, "pvbatch --use-offscreen-rendering script.py %s", fn);
  consumer(cmd);
}
