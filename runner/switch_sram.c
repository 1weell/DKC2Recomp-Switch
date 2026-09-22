#include "switch_sram.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef SWITCH_SRAM_TEST_IO
extern size_t SwitchTestWrite(const void *, size_t, size_t, FILE *);
extern int SwitchTestFlush(FILE *);
extern int SwitchTestClose(FILE *);
extern int SwitchTestRename(const char *, const char *);
#define fwrite SwitchTestWrite
#define fflush SwitchTestFlush
#define fclose SwitchTestClose
#define rename SwitchTestRename
#endif

static int ReadExact(const char *path, void *data, size_t size) {
  FILE *f = fopen(path, "rb");
  if (!f) return errno == ENOENT ? 0 : -1;
  int ok = fread(data, 1, size, f) == size && fgetc(f) == EOF;
  if (ferror(f)) ok = -1;
  if (fclose(f)) ok = -1;
  return ok;
}
int SwitchSramRead(const char *path, void *data, size_t size) {
  char backup[512];
  if (!size || snprintf(backup, sizeof backup, "%s.bak", path) >= (int)sizeof backup) return 0;
  void *copy = malloc(size);
  if (!copy) return 0;
  int result = ReadExact(path, copy, size) == 1 ? 1 :
               ReadExact(backup, copy, size) == 1 ? 2 : 0;
  if (result) memcpy(data, copy, size);
  free(copy);
  return result;
}
int SwitchSramWrite(const char *path, const void *data, size_t size) {
  char temp[512], backup[512];
  if (!size || snprintf(temp, sizeof temp, "%s.tmp", path) >= (int)sizeof temp ||
      snprintf(backup, sizeof backup, "%s.bak", path) >= (int)sizeof backup) return 0;
  FILE *f = fopen(temp, "wb");
  if (!f) return 0;
  int ok = fwrite(data, 1, size, f) == size;
  if (fflush(f)) ok = 0;
  if (fclose(f)) ok = 0;
  if (!ok) { remove(temp); return 0; }
  void *copy = malloc(size);
  if (!copy) { remove(temp); return 0; }
  ok = ReadExact(temp, copy, size) == 1 && memcmp(data, copy, size) == 0;
  int valid_old = ReadExact(path, copy, size);
  free(copy);
  if (!ok || valid_old < 0) { remove(temp); return 0; }
  /* Never rotate a truncated primary over a recoverable backup. */
  if (valid_old) {
    if (remove(backup) && errno != ENOENT) return 0;
    if (rename(path, backup)) return 0;
  } else if (remove(path) && errno != ENOENT) return 0;
  /* If promotion fails, leave the valid backup available to the reader. */
  return rename(temp, path) == 0;
}
