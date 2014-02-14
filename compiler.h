#ifndef FREEPROC_COMPILER_H
#define FREEPROC_COMPILER_H

/* MALLOC technically only tells the compiler that the returned value is
 * newly-allocated memory.  However we use it to additionally mean that the
 * *caller* is thereafter responsible for the memory. */
#ifdef __GNUC__
#	define CONST __attribute__((const))
#	define MALLOC __attribute__((malloc))
#	define PURE __attribute__((pure))
#else
#	define CONST /* no const function support */
#	define MALLOC /* no malloc function support */
#	define PURE /* no pure function support */
#endif

#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
# define INTEL(stmt) /* icc-only statement removed. */
#else
# define INTEL(stmt) do { stmt; } while(0)
#endif

#endif
