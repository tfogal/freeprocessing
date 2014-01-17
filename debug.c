#define _POSIX_C_SOURCE 201112L
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include "debug.h"

static long pid = -1;
static bool color_enabled = false;
__attribute__((constructor)) static void
fp_dbg_init()
{
  pid = (int) getpid();
  color_enabled = isatty(fileno(stdout));
}

static bool
dbgchannel_enabled(const struct symbdbgchannel* chn, enum SymbiontChanClass c)
{
  return chn->flags & (1 << c);
}

/* ANSI escape codes for colors. */
__attribute__((unused)) static const char* C_DGRAY  = "\033[01;30m";
static const char* C_NORM   = "\033[00m";
static const char* C_RED    = "\033[01;31m";
static const char* C_YELLOW = "\033[01;33m";
__attribute__((unused)) static const char* C_GREEN  = "\033[01;32m";
__attribute__((unused)) static const char* C_MAG    = "\033[01;35m";
__attribute__((unused)) static const char* C_LBLUE  = "\033[01;36m";
__attribute__((unused)) static const char* C_WHITE  = "\033[01;27m";

static const char*
color(const enum SymbiontChanClass cls)
{
  if(!color_enabled) { return ""; }
  switch(cls) {
    case SymbiontTrace: return C_WHITE;
    case SymbiontWarn: return C_YELLOW;
    case SymbiontErr: return C_RED;
  }
  assert(false);
  return C_NORM;
}

void
symb_dbg(enum SymbiontChanClass type, const struct symbdbgchannel* channel,
         const char* func, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  if(dbgchannel_enabled(channel, type)) {
    printf("%s[%ld](%s) ", color(type), pid, func);
    vprintf(format, args);
    printf("%s\n", C_NORM);
  }
  va_end(args);
}
