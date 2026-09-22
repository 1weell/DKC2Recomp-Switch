#ifndef DKC2_SWITCH_SRAM_H
#define DKC2_SWITCH_SRAM_H
#include <stddef.h>
/* Exact-length reads never partially overwrite the caller's SRAM. */
int SwitchSramRead(const char *path, void *data, size_t size);
int SwitchSramWrite(const char *path, const void *data, size_t size);
#endif
