#ifndef DKC2_SWITCH_SETTINGS_H
#define DKC2_SWITCH_SETTINGS_H
#include <stdint.h>
typedef struct SwitchSettings { int wide, volume, smooth, shortcut; } SwitchSettings;
static inline SwitchSettings SwitchSettingsDefaults(void) {
  SwitchSettings s = {1, 10, 0, 0}; return s;
}
static inline int SwitchSettingsDecode(const uint8_t data[12], SwitchSettings *out) {
  if (data[0] != 'D' || data[1] != 'K' || data[2] != 'C' || data[3] != 'S' ||
      data[4] != 1 || data[5] > 1 || data[6] > 10 || data[7] > 1 || data[8] > 1 ||
      data[9] || data[10] || data[11]) return 0;
  *out = (SwitchSettings){data[5], data[6], data[7], data[8]}; return 1;
}
static inline void SwitchSettingsEncode(const SwitchSettings *s, uint8_t data[12]) {
  const uint8_t encoded[12] = {'D','K','C','S',1,(uint8_t)s->wide,
    (uint8_t)s->volume,(uint8_t)s->smooth,(uint8_t)s->shortcut,0,0,0};
  for (int i = 0; i < 12; ++i) data[i] = encoded[i];
}
#endif
