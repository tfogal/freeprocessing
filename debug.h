#ifndef FP_SYMBIONT_DEBUG_H
#define FP_SYMBIONT_DEBUG_H 1

#include <stdbool.h>
#include <stdlib.h>

enum SymbiontChanClass {
  SymbiontErr=0,
  SymbiontWarn,
  SymbiontTrace,
};

struct symbdbgchannel {
  unsigned flags;
  char name[32];
};

#define DEFAULT_CHFLAGS (1 << SymbiontErr) | (1 << SymbiontWarn)
#define DECLARE_CHANNEL(ch) \
  static struct symbdbgchannel symb_chn_##ch = { DEFAULT_CHFLAGS, #ch }; \
  __attribute__((constructor)) static void \
  ch_init_##ch() { \
    const char* dbg_ = getenv("LIBSITU_DEBUG"); \
    symb_parse_options(&symb_chn_##ch, dbg_); \
  }

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
extern void symb_parse_options(struct symbdbgchannel*, const char* opt);

#endif /* FP_SYMBIONT_DEBUG_H */
