/* Debugging for symbiont execution.  Example usage:
 *
 *   DECLARE_CHANNEL(stuff);
 *   TRACE(stuff, "is happening!");
 *   ERR(stuff, "something really bad happened.");
 *   WARN(stuff, "i think something's wrong?");
 *
 * The user could enable / disable the above channel by setting the
 * LIBSITU_DEBUG environment variable:
 *
 *   export LIBSITU_DEBUG="stuff=+err,-warn,+trace" */
#ifndef FP_SYMBIONT_DEBUG_H
#define FP_SYMBIONT_DEBUG_H 1

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum SymbiontChanClass {
  SymbiontErr=0,
  SymbiontWarn,
  SymbiontTrace,
  SymbiontFixme,
};

struct symbdbgchannel {
  unsigned flags;
  char name[32];
};

#define DEFAULT_CHFLAGS \
  (1U << SymbiontErr) | (1U << SymbiontWarn) | (1U << SymbiontFixme)
/* creates a new debug channel.  debug channels are private to implementation,
 * and should not be declared in header files. */
#define DECLARE_CHANNEL(ch) \
  static struct symbdbgchannel symb_chn_##ch = { DEFAULT_CHFLAGS, #ch }; \
  __attribute__((constructor(200))) static void \
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
#define FIXME(ch, args...) \
  symb_dbg(SymbiontFixme, &symb_chn_##ch, __FUNCTION__, args)

/* for internal use only. */
extern void symb_dbg(enum SymbiontChanClass, const struct symbdbgchannel*,
                     const char* func, const char* format, ...)
                     __attribute__((format(printf, 4, 5)));
extern void symb_parse_options(struct symbdbgchannel*, const char* opt);

#ifdef __cplusplus
}
#endif

#endif /* FP_SYMBIONT_DEBUG_H */
