#define _POSIX_C_SOURCE 201112L
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
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
static const char* C_LBLUE  = "\033[01;36m";
__attribute__((unused)) static const char* C_WHITE  = "\033[01;27m";

static const char*
color(const enum SymbiontChanClass cls)
{
  if(!color_enabled) { return ""; }
  switch(cls) {
    case SymbiontTrace: return C_WHITE;
    case SymbiontWarn: return C_YELLOW;
    case SymbiontErr: return C_RED;
    case SymbiontFixme: return C_LBLUE;
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
    const char* fixit = type==SymbiontFixme ? "-FIXME" : "";
    printf("%s[%ld](%s%s) ", color(type), pid, func, fixit);
    vprintf(format, args);
    printf("%s\n", C_NORM);
  }
  va_end(args);
}

/* maps a string name to a class.  there should be a one-to-one mapping from
 * every entry in 'enum SymbiontChanClass' to this. */
static enum SymbiontChanClass
name_class(const char* name)
{
  if(strncmp(name, "err", 3) == 0) { return SymbiontErr; }
  if(strncmp(name, "warn", 4) == 0) { return SymbiontWarn; }
  if(strncmp(name, "trace", 5) == 0) { return SymbiontTrace; }
  if(strncmp(name, "fixme", 5) == 0) { return SymbiontFixme; }
  /* hack.  what do we do if they give us a class that isn't defined?  well,
   * since we use this to find the flag's position by bit-shifting, let's just
   * do something we know will shit off the end of our flag sizes.  that way,
   * undefined classes are just silently ignored. */
  return 64;
}
/* parses options of the form "chname=+a,-b,+c;chname2=+d,-c". */
void
symb_parse_options(struct symbdbgchannel* ch, const char* opt)
{
  /* outer loop iterates over channels.  channel names are separated by ';' */
  for(const char* chan=opt; chan && chan!=(const char*)0x1;
      chan=strchr(chan, ';')+1) {
    /* extract a substring to make parsing easier. */
    char* chopts = strdup(chan);
    { /* if there's another channel after, cut the string there. */
      char* nextopt = strchr(chopts, ';');
      if(nextopt) { *nextopt = '\0'; }
    }
    if(strncmp(chopts, ch->name, strlen(ch->name)) == 0) {
      /* matched our channel name.  now we want to parse the list of options,
       * separated by commas, e.g.: "+x,-y,+blah,+abc" */
      for(char* olist=strchr(chopts, '=')+1;
          olist && olist != (const char*)0x1;
          olist = strchr(olist, ',')+1) {
        /* the "+1" gets rid of the minus or plus */
        enum SymbiontChanClass cls = name_class(olist+1);
        /* temporarily null out the subsequent options, for printing. */
        char* optend = strchr(olist, ',');
        if(optend) { *optend = '\0'; }
        if(*olist == '+') {
          printf("[%ld] %s: enabling %s\n", pid, ch->name, olist+1);
          ch->flags |= (1<<cls);
        } else if(*olist == '-') {
          printf("[%ld] %s: disabling %s\n", pid, ch->name, olist+1);
          ch->flags &= ~(1<<cls);
        }
        /* 'de-null' it. */
        if(optend) { *optend = ','; }
      }
    }
    free(chopts);
  }
}
