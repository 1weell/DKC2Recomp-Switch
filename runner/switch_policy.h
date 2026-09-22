#ifndef DKC2_SWITCH_POLICY_H
#define DKC2_SWITCH_POLICY_H
#include <stdint.h>
/* Stable Npad bit positions, checked against libnx in switch_main.c. */
static inline uint32_t SwitchMapNpad(uint64_t keys) {
  static const unsigned source_bits[12] = {1, 3, 11, 10, 13, 15, 12, 14, 0, 2, 6, 7};
  uint32_t result = 0;
  for (unsigned i = 0; i < 12; ++i)
    if (keys & (UINT64_C(1) << source_bits[i])) result |= 1u << i;
  return result;
}
/* Neutral wins for opposing directions; diagonals are allowed. */
static inline uint32_t SwitchDirections(uint32_t input, int x, int y) {
  if (x < -12000) input |= 1u << 6;
  if (x > 12000) input |= 1u << 7;
  if (y < -12000) input |= 1u << 5;
  if (y > 12000) input |= 1u << 4;
  if ((input & 0x30) == 0x30) input &= ~0x30u;
  if ((input & 0xc0) == 0xc0) input &= ~0xc0u;
  return input;
}
static inline uint64_t SwitchDeadline(uint64_t deadline, uint64_t now,
                                      uint64_t step) {
  return now > deadline && now - deadline > step * 3 ? now : deadline;
}
#endif
