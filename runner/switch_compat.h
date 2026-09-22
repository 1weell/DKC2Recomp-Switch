#ifndef DKC2_SWITCH_COMPAT_H
#define DKC2_SWITCH_COMPAT_H

#include <stddef.h>

/* The shared runtime uses strdup in a utility path that is not consistently
 * declared by the Switch C library under strict C11. Keep that dependency
 * local to this target instead of weakening the project-wide language mode. */
char *Dkc2SwitchStrdup(const char *source);
#define strdup Dkc2SwitchStrdup

#endif
