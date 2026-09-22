#include "switch_policy.h"
#include "switch_sram.h"
#include "switch_settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
static int fail_write, fail_flush, fail_close, fail_rename;
size_t SwitchTestWrite(const void *p, size_t size, size_t n, FILE *f) {
  if (fail_write) { fail_write = 0; errno = ENOSPC; return 0; }
  return fwrite(p,size,n,f);
}
int SwitchTestFlush(FILE *f) {
  if (fail_flush) { fail_flush = 0; errno = ENOSPC; return EOF; }
  return fflush(f);
}
int SwitchTestClose(FILE *f) {
  int result = fclose(f);
  if (fail_close && --fail_close == 0) { errno = EIO; return EOF; }
  return result;
}
int SwitchTestRename(const char *a, const char *b) {
  if (fail_rename && --fail_rename == 0) { errno = EACCES; return -1; }
  return rename(a,b);
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv) {
  SwitchSettings settings = SwitchSettingsDefaults(), decoded;
  uint8_t bytes[12];
  SwitchSettingsEncode(&settings, bytes);
  CHECK(SwitchSettingsDecode(bytes, &decoded) && decoded.volume == 10 && decoded.wide == 1);
  bytes[4] = 2; CHECK(!SwitchSettingsDecode(bytes, &decoded));
  bytes[4] = 1; bytes[6] = 11; CHECK(!SwitchSettingsDecode(bytes, &decoded));
  bytes[6] = 0; CHECK(SwitchSettingsDecode(bytes, &decoded) && decoded.volume == 0);
  bytes[9] = 1; CHECK(!SwitchSettingsDecode(bytes, &decoded));
  CHECK(argc == 2);
  char path[512], backup[520], temp[520];
  snprintf(path, sizeof path, "%s/synthetic.srm", argv[1]);
  snprintf(backup, sizeof backup, "%s.bak", path);
  snprintf(temp, sizeof temp, "%s.tmp", path);
  remove(path); remove(backup); remove(temp);
  unsigned char a[4] = {1,2,3,4}, b[4] = {5,6,7,8}, out[4] = {0};
  CHECK(SwitchSramRead(path, out, 4) == 0);
  CHECK(SwitchSramWrite(path, a, 4));
  CHECK(SwitchSramRead(path, out, 4) == 1 && !memcmp(a,out,4));
  fail_write = 1;
  CHECK(!SwitchSramWrite(path, b, 4));
  CHECK(SwitchSramRead(path, out, 4) == 1 && !memcmp(a,out,4));
  fail_flush = 1;
  CHECK(!SwitchSramWrite(path, b, 4));
  CHECK(SwitchSramRead(path, out, 4) == 1 && !memcmp(a,out,4));
  fail_close = 1;
  CHECK(!SwitchSramWrite(path, b, 4));
  CHECK(SwitchSramRead(path, out, 4) == 1 && !memcmp(a,out,4));
  /* Third close is the old-primary validation, after write + readback. */
  fail_close = 3;
  CHECK(!SwitchSramWrite(path, b, 4));
  CHECK(SwitchSramRead(path, out, 4) == 1 && !memcmp(a,out,4));
  fail_rename = 1;
  CHECK(!SwitchSramWrite(path, b, 4));
  CHECK(SwitchSramRead(path, out, 4) == 1 && !memcmp(a,out,4));
  fail_rename = 2;
  CHECK(!SwitchSramWrite(path, b, 4));
  CHECK(SwitchSramRead(path, out, 4) == 2 && !memcmp(a,out,4));
  CHECK(SwitchSramWrite(path, a, 4));
  CHECK(SwitchSramWrite(path, b, 4));
  FILE *f = fopen(path, "wb"); CHECK(f); fputc(0, f); fclose(f);
  CHECK(SwitchSramRead(path, out, 4) == 2 && !memcmp(a,out,4));
  CHECK(SwitchSramWrite(path, b, 4));
  remove(path);
  CHECK(SwitchSramRead(path, out, 4) == 2 && !memcmp(a,out,4));
  CHECK(!SwitchSramWrite("missing-parent/synthetic.srm", b, 4));
  CHECK(SwitchSramRead(path, out, 4) == 2 && !memcmp(a,out,4));
  remove(backup);
  memset(out, 99, 4);
  CHECK(SwitchSramRead(path, out, 4) == 0 && out[0] == 99 && out[3] == 99);
  const unsigned physical_bits[12] = {1,3,11,10,13,15,12,14,0,2,6,7};
  for (unsigned i = 0; i < 12; ++i)
    CHECK(SwitchMapNpad(UINT64_C(1) << physical_bits[i]) == (1u << i));
  CHECK(SwitchMapNpad(UINT64_MAX) == 0xfff);
  CHECK(SwitchMapNpad((1u << 8) | (1u << 9)) == 0);
  CHECK((SwitchMapNpad(1u << 1) | (SwitchMapNpad(1u << 0) << 12)) == 0x100001);
  CHECK(SwitchDirections(0, 12000, -12000) == 0);
  CHECK(SwitchDirections(0, -12001, 12001) == 0x50);
  CHECK(SwitchDirections(0xf0, 0, 0) == 0);
  CHECK(SwitchDirections(0x101, 32767, -32767) == 0x1a1);
  CHECK(SwitchDeadline(100, 120, 10) == 100);
  CHECK(SwitchDeadline(100, 131, 10) == 131);
  CHECK(SwitchDeadline(100, 90, 10) == 100);
  return 0;
}
