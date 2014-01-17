#ifndef FP_SYMBIONT_DEBUG_H
#define FP_SYMBIONT_DEBUG_H 1

#include <stdbool.h>

enum SymbiontChanClass {
  SymbiontErr=0,
  SymbiontWarn,
  SymbiontTrace,
};

struct symbdbgchannel {
  unsigned flags;
  char name[32];
};

#define DECLARE_CHANNEL(ch) \
  static struct symbdbgchannel symb_chn_##ch = { ~0, #ch }

#define TRACE(ch, args...) \
  symb_dbg(SymbiontTrace, &symb_chn_##ch, __FUNCTION__, args)
#define ERR(ch, args...) \
  symb_dbg(SymbiontErr, &symb_chn_##ch, __FUNCTION__, args)
#define WARN(ch, args...) \
  symb_dbg(SymbiontWarn, &symb_chn_##ch, __FUNCTION__, args)

/* for internal use only. */
extern void symb_dbg(enum SymbiontChanClass, const struct symbdbgchannel*,
                     const char* func, const char* format, ...)
                     __attribute__((format(printf, 4, 5)));

#endif /* FP_SYMBIONT_DEBUG_H */
