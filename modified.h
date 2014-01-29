#ifndef SYMBIOTE_MODIFIED_H
#define SYMBIOTE_MODIFIED_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void watch;

watch* Watch(const char* fn);
bool Modified(const watch*);
void Unwatch(watch*);

#ifdef __cplusplus
}
#endif
#endif
