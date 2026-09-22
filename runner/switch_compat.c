#include "switch_compat.h"

#include <stdlib.h>
#include <string.h>

char *Dkc2SwitchStrdup(const char *source) {
  if (!source)
    return NULL;
  size_t size = strlen(source) + 1;
  char *copy = (char *)malloc(size);
  if (copy)
    memcpy(copy, source, size);
  return copy;
}
