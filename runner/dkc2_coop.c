#include "dkc2_coop.h"
#include "dkc2_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Dkc2CoopMode s_coop_mode = kDkc2CoopSimultaneous;
static uint16_t s_stomp_source;
static uint16_t s_stomp_kong;
static uint16_t s_mount_source, s_mount_kong;
static uint16_t s_held_sprite, s_held_id, s_held_kong;
static uint8_t s_lost_mask;
static uint8_t s_stomp_events;
static bool s_banana_extra_pass;
static uint16_t s_banana_second_bounds[4];

/* DKC2_COOP_DEBUG=1 prints the first evaluations of each helper so a
 * route can confirm the generated gate actually consults the policy and
 * which words it selects. Host observation only. */
static int CoopDebugEnabled(void) {
  static int state = -1;
  if (state < 0)
    state = getenv("DKC2_COOP_DEBUG") != NULL && getenv("DKC2_COOP_DEBUG")[0] == '1';
  return state;
}

static int s_debug_gate_count;
static int s_debug_held_count;
static int s_debug_pressed_count;

void Dkc2CoopSetMode(Dkc2CoopMode mode) {
  if (mode < kDkc2CoopSimultaneous || mode >= kDkc2CoopModeCount)
    mode = kDkc2CoopSimultaneous;
  s_coop_mode = mode;
  s_stomp_source = s_stomp_kong = 0;
  s_mount_source = s_mount_kong = 0;
  s_held_sprite = s_held_id = s_held_kong = 0;
  s_banana_extra_pass = false;
  s_stomp_events = 0;
}

void Dkc2CoopResetSession(void) {
  s_lost_mask = 0;
  s_banana_extra_pass = false;
  s_stomp_events = 0;
  s_stomp_source = s_stomp_kong = 0;
  s_mount_source = s_mount_kong = 0;
  s_held_sprite = s_held_id = s_held_kong = 0;
}

uint8_t Dkc2CoopSaveLostMask(void) { return s_lost_mask; }

void Dkc2CoopLoadLostMask(CpuState *cpu, uint8_t mask, bool recorded) {
  Dkc2CoopResetSession();
  s_lost_mask = recorded ? (uint8_t)(mask & 3) : 0;
  if (!cpu || recorded) return;
  /* Old snapshots have no host co-op metadata. An absent Kong that has
   * already left its initial follower order has spent its initial join.
   * Hurt states themselves are observed again by the state dispatcher. */
  if (!(cpu_read16(cpu, 0, 0x08C2) & 0x4000)) {
    uint16_t slot = cpu_read16(cpu, 0, 0x0597);
    if ((slot == kDkc2CoopSlotA || slot == kDkc2CoopSlotB) &&
        cpu_read16(cpu, 0, (uint16_t)(slot + 0x2E)) == 0x13 &&
        cpu_read16(cpu, 0, (uint16_t)(slot + 2)) != 0xD8)
      s_lost_mask |= slot == kDkc2CoopSlotA ? 1 : 2;
  }
}

Dkc2CoopMode Dkc2CoopGetMode(void) { return s_coop_mode; }

bool Dkc2CoopModeFromName(const char *text, Dkc2CoopMode *mode) {
  if (!text || !mode)
    return false;
  if (strcmp(text, "simultaneous") == 0) {
    *mode = kDkc2CoopSimultaneous;
    return true;
  }
  if (strcmp(text, "classic") == 0) {
    *mode = kDkc2CoopClassic;
    return true;
  }
  return false;
}

bool Dkc2CoopSimultaneousActive(uint16_t mode_flag, bool enabled) {
  /* Only "2 PLAYER TEAM" ($060D == 1) becomes simultaneous. 1 PLAYER and
   * 2 PLAYER CONTEST keep the cartridge input path untouched. */
  return enabled && mode_flag == 1;
}

uint16_t Dkc2CoopGateActive(uint16_t active_slot, uint16_t sprite,
                            uint16_t mode_flag, bool enabled) {
  if (!Dkc2CoopSimultaneousActive(mode_flag, enabled))
    return active_slot;
  /* Compare current_sprite against itself: every Kong that reaches the
   * gate takes the input path with its own controller's words. Only the
   * two Kong slots qualify; anything else keeps the cartridge compare
   * (the gate only runs from Kong action states, so this is defensive). */
  if (sprite == kDkc2CoopSlotA || sprite == kDkc2CoopSlotB)
    return sprite;
  return active_slot;
}

/* $13 waits for the follower script; $22 replays the leader's position
 * history ($7FA532/$7FA572), including after a jump lands. Redirect at
 * state dispatch, before that replay can teleport the second player.
 * A waiting player joins on input. Once in normal follow mode it stays
 * independently controllable even when its controller is released.
 * Hurt, carried, barrel and other action states retain their own flow. */
bool Dkc2CoopFollowerNeedsControl(uint16_t follower_state,
                                  uint16_t follower_held) {
  return follower_state == 0x0022 ||
         (follower_state == 0x0013 && follower_held != 0);
}

uint16_t Dkc2CoopSelectHeld(uint16_t original, uint16_t sprite,
                            uint16_t mode_flag, bool enabled,
                            uint16_t p1_held, uint16_t p2_held) {
  if (!Dkc2CoopSimultaneousActive(mode_flag, enabled))
    return original;
  if (sprite == kDkc2CoopSlotA)
    return p1_held;
  if (sprite == kDkc2CoopSlotB)
    return p2_held;
  return original;
}

uint16_t Dkc2CoopSelectPressed(uint16_t original, uint16_t sprite,
                               uint16_t mode_flag, bool enabled,
                               uint16_t p1_pressed, uint16_t p2_pressed) {
  if (!Dkc2CoopSimultaneousActive(mode_flag, enabled))
    return original;
  if (sprite == kDkc2CoopSlotA)
    return p1_pressed;
  if (sprite == kDkc2CoopSlotB)
    return p2_pressed;
  return original;
}

/* Read shapes mirror the generated code's own addressing so the helpers
 * observe exactly the bytes the wrapped instructions would: direct-page
 * operands resolve in bank $00 at D+offset, absolute operands at DB:offset
 * (DKC2 runs these paths with D = 0 and DB = $00/$7E, which alias the same
 * WRAM bytes for every address used here). */
static uint16_t CoopReadDp(CpuState *cpu, uint16_t offset) {
  return cpu_read16(cpu, 0x00, (uint16)(cpu->D + offset));
}

static uint16_t CoopReadAbs(CpuState *cpu, uint16_t offset) {
  return cpu_read16(cpu, cpu->DB, (uint16)(offset));
}

static bool CoopEnabled(CpuState *cpu) {
  return cpu && Dkc2CoopSimultaneousActive(cpu_read16(cpu, 0, kDkc2CoopModeFlag),
                                          s_coop_mode == kDkc2CoopSimultaneous);
}

static void CoopMakeLeader(CpuState *cpu, uint16_t kong) {
  if (kong != cpu_read16(cpu, 0, 0x0597)) return;
  uint16_t active = cpu_read16(cpu, 0, 0x0593);
  uint16_t active_work = cpu_read16(cpu, 0, 0x0595);
  cpu_write16(cpu, 0, 0x0593, kong);
  cpu_write16(cpu, 0, 0x0595, cpu_read16(cpu, 0, 0x0599));
  cpu_write16(cpu, 0, 0x0597, active);
  cpu_write16(cpu, 0, 0x0599, active_work);
  cpu_write16(cpu, 0, 0x08A4, kong == kDkc2CoopSlotB ? 1 : 0);
  cpu_write16(cpu, 0, 0x08A2, kong == kDkc2CoopSlotB ? 2 : 1);
}

uint16_t Dkc2CoopTeamPartnerValue(CpuState *cpu, uint16_t original) {
  if (!CoopEnabled(cpu)) return original;
  uint16_t kong = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  if (kong == kDkc2CoopSlotA) return kDkc2CoopSlotB;
  if (kong == kDkc2CoopSlotB) return kDkc2CoopSlotA;
  return original;
}

uint16_t Dkc2CoopTeamStateValue(CpuState *cpu, uint16_t original) {
  if (!CoopEnabled(cpu)) return original;
  uint16_t kong = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  uint16_t partner = Dkc2CoopTeamPartnerValue(cpu, 0);
  if (!partner) return original;
  /* The original action summons an AI follower, regardless of distance.
   * Independent players must be alive, on foot and within arm's reach.
   * Only substitute the eligibility operand; the native reaction still
   * performs the pickup, animation, carrying physics and release. */
  if (cpu->Y != partner || s_lost_mask || cpu_read16(cpu, 0, 0x0D7A) ||
      cpu_read16(cpu, 0, 0x006E) ||
      !(cpu_read16(cpu, 0, 0x08C2) & 0x4000) ||
      cpu_read16(cpu, 0, (uint16_t)(kong + 0x2E)) != 0 ||
      (original != 0 && original != 0x13 && original != 0x22))
    return 0xFFFF;
  int dx = (int)cpu_read16(cpu, 0, (uint16_t)(kong + 6)) -
           (int)cpu_read16(cpu, 0, (uint16_t)(partner + 6));
  int dy = (int)cpu_read16(cpu, 0, (uint16_t)(kong + 10)) -
           (int)cpu_read16(cpu, 0, (uint16_t)(partner + 10));
  if (dx < -24 || dx > 24 || dy < -16 || dy > 16) return 0xFFFF;
  /* Team reactions use the active/inactive work pair. Promote the carrier
   * without moving either actor or changing their fixed controller ports.
   * Guest selectors preserve this ownership across saves and rewind. */
  CoopMakeLeader(cpu, kong);
  return 0x22;
}

static uint16_t CoopHeldOwner(CpuState *cpu) {
  if (!cpu || !Dkc2CoopSimultaneousActive(
                  cpu_read16(cpu, 0, kDkc2CoopModeFlag),
                  s_coop_mode == kDkc2CoopSimultaneous))
    return 0;
  uint16_t object = cpu_read16(cpu, 0, 0x0D7A);
  if (object == kDkc2CoopSlotA || object == kDkc2CoopSlotB) {
    uint16_t owner = cpu_read16(cpu, 0, 0x0593);
    return object == cpu_read16(cpu, 0, 0x0597) &&
           owner == (object == kDkc2CoopSlotA ? kDkc2CoopSlotB : kDkc2CoopSlotA)
               ? owner : 0;
  }
  if (object < 0x0E9E || object >= 0x16B2 ||
      (object - 0x0DE2) % 0x5E != 0 || !cpu_read16(cpu, 0, object))
    return 0;
  /* Ground carry and throw states identify the owner in a restored guest
   * snapshot too. The pickup record covers the transition before the Kong
   * has entered its carrying state; it is not the only source of truth. */
  uint16_t candidate = 0;
  for (uint16_t slot = kDkc2CoopSlotA; slot <= kDkc2CoopSlotB; slot += 0x5E) {
    uint16_t state = cpu_read16(cpu, 0, (uint16_t)(slot + 0x2E));
    if (state == 0x0C || state == 0x0F || state == 0x3F) {
      if (candidate) { candidate = 0; break; }
      candidate = slot;
    }
  }
  if (candidate) return candidate;
  if (object != s_held_sprite || cpu_read16(cpu, 0, object) != s_held_id)
    return 0;
  return s_held_kong;
}

uint16_t Dkc2CoopRecordPickupValue(CpuState *cpu, uint16_t object) {
  s_held_sprite = s_held_id = s_held_kong = 0;
  if (cpu && Dkc2CoopSimultaneousActive(
                 cpu_read16(cpu, 0, kDkc2CoopModeFlag),
                 s_coop_mode == kDkc2CoopSimultaneous)) {
    uint16_t owner = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
    if ((owner == kDkc2CoopSlotA || owner == kDkc2CoopSlotB) &&
        object >= 0x0E9E && object < 0x16B2 &&
        (object - 0x0DE2) % 0x5E == 0) {
      uint16_t id = cpu_read16(cpu, 0, object);
      if (id) {
        s_held_sprite = object;
        s_held_id = id;
        s_held_kong = owner;
      }
    }
  }
  return object;
}

uint16_t Dkc2CoopHeldOwnerValue(CpuState *cpu, uint16_t original) {
  uint16_t owner = CoopHeldOwner(cpu);
  return owner ? owner : original;
}

uint16_t Dkc2CoopHeldObjectValue(CpuState *cpu, uint16_t original) {
  uint16_t owner = CoopHeldOwner(cpu);
  if (owner) {
    uint16_t sprite = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
    if ((sprite == kDkc2CoopSlotA || sprite == kDkc2CoopSlotB) && sprite != owner)
      return 0;
  }
  return original;
}

uint16_t Dkc2CoopGateActiveValue(CpuState *cpu, uint16_t active_slot) {
  if (!cpu)
    return active_slot;
  uint16_t sprite = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  uint16_t mode = CoopReadAbs(cpu, kDkc2CoopModeFlag);
  bool enabled = s_coop_mode == kDkc2CoopSimultaneous;
  uint16_t result = Dkc2CoopGateActive(active_slot, sprite, mode, enabled);
  if (CoopDebugEnabled() && s_debug_gate_count < 60) {
    s_debug_gate_count++;
    fprintf(stderr, "coop_gate #%d sprite=%04x active=%04x mode=%u -> %04x\n",
            s_debug_gate_count, sprite, active_slot, mode, result);
  }
  return result;
}

static void CoopResumeRopeTransition(CpuState *cpu, uint16_t sprite) {
  /* Older simultaneous snapshots can have already skipped the completion
   * callback and parked at the script's terminal $83/$D12B wait. Replay just
   * that native callback, preserving the finished pose and rope coordinates.
   * Validate its instructions instead of assuming a character-specific cursor. */
  uint16_t animation = cpu_read16(cpu, 0, (uint16_t)(sprite + 0x36));
  if (animation >= 0xA3) animation = (uint16_t)(animation - 0xA3);
  if ((animation != 0x34 && animation != 0x35) ||
      cpu_read16(cpu, 0, (uint16_t)(sprite + 0x38)) != 0 ||
      cpu_read16(cpu, 0, (uint16_t)(sprite + 0x3E)) != 0xDD63)
    return;
  uint16_t cursor = cpu_read16(cpu, 0, (uint16_t)(sprite + 0x3C));
  uint16_t completion = animation == 0x34 ? 0xDD7E : 0xDD90;
  if (cursor < 3 ||
      (cpu_read16(cpu, 0xF9, (uint16_t)(cursor - 3)) & 0xFF) != 0x81 ||
      cpu_read16(cpu, 0xF9, (uint16_t)(cursor - 2)) != completion ||
      (cpu_read16(cpu, 0xF9, cursor) & 0xFF) != 0x83 ||
      cpu_read16(cpu, 0xF9, (uint16_t)(cursor + 1)) != 0xD12B)
    return;
  cpu_write16(cpu, 0, (uint16_t)(sprite + 0x3C), (uint16_t)(cursor - 3));
}

uint16_t Dkc2CoopSelectStateWord(CpuState *cpu, uint16_t original) {
  if (!cpu || !Dkc2CoopSimultaneousActive(
                  cpu_read16(cpu, 0x00, kDkc2CoopModeFlag),
                  s_coop_mode == kDkc2CoopSimultaneous))
    return original;
  /* This wraps LDA $2E,x before X becomes the dispatch-table index.
   * The dispatcher has already set DB to $B8, so use bank-$00 WRAM. */
  uint16_t sprite = cpu->X;
  if (sprite != kDkc2CoopSlotA && sprite != kDkc2CoopSlotB)
    return original;
  if (original == 0x36)
    CoopResumeRopeTransition(cpu, sprite);
  uint16_t held = cpu_read16(cpu, 0x00, sprite == kDkc2CoopSlotA
                                          ? kDkc2CoopP1Held : kDkc2CoopP2Held);
  uint8_t lost_bit = sprite == kDkc2CoopSlotA ? 1 : 2;
  if (original == 0x24 || original == 0x25 || original == 0x05 || original == 0x59)
    s_lost_mask |= lost_bit;
  if (original == 0x3E) /* The cartridge accepted a DK-barrel rescue. */
    s_lost_mask &= (uint8_t)~lost_bit;
  if (original == 0x13 && (s_lost_mask & lost_bit))
    return original;
  /* The carrier waits in $13 while the passenger jumps into its hands.
   * Independent movement input must not cancel that native animation. */
  uint16_t object = cpu_read16(cpu, 0, 0x0D7A);
  if (original == 0x13 &&
      (object == kDkc2CoopSlotA || object == kDkc2CoopSlotB) &&
      CoopHeldOwner(cpu) == sprite)
    return original;
  uint16_t state = Dkc2CoopFollowerNeedsControl(original, held) ? 0 : original;
  if (original == 0x21 && !object && !(s_lost_mask & lost_bit) &&
      (cpu_read16(cpu, 0, (uint16_t)(sprite + 0x1E)) & 0x0101) &&
      !(cpu_read16(cpu, 0, (uint16_t)(sprite + 0x24)) & 0x8000)) {
    /* A thrown partner normally sits waiting for the leader to touch it.
     * Once grounded, give the independent player its normal interaction
     * mask and action gate. Preserve the native flight and enemy hit path. */
    state = 0;
    uint16_t flags = cpu_read16(cpu, 0, (uint16_t)(sprite + 0x30));
    cpu_write16(cpu, 0, (uint16_t)(sprite + 0x30), (uint16_t)(flags | 0x18));
  }
  if (original == 0x6F) {
    /* TEAM's turn-handoff prompt waits for the newly active controller.
     * Both controllers are already participating: resume its existing
     * post-handoff jump and invincibility, including from an old snapshot. */
    state = 6;
    if (cpu_read16(cpu, 0, 0x0A36) == 7) {
      cpu_write16(cpu, 0, 0x0A36, 0);
      cpu_write16(cpu, 0, 0x0A38, 0);
    }
    uint16_t flags = cpu_read16(cpu, 0, 0x08C2);
    cpu_write16(cpu, 0, 0x08C2, (uint16_t)(flags & ~0x0100));
    cpu_write16(cpu, 0, (uint16_t)(sprite + 0x1C), 0);
  }
  if (state != original && CoopHeldOwner(cpu) == sprite)
    state = 0x000C; /* independently controlled, still carrying its object */
  if (state != original)
    cpu_write16(cpu, 0x00, (uint16_t)(cpu->D + sprite + 0x002E), state);
  if (state == 0 && sprite == cpu_read16(cpu, 0x00, 0x0597)) {
    /* A joined co-op player is present. Otherwise a DK barrel attempts to
     * rescue that already-controlled Kong and teleports it to the barrel. */
    uint16_t kong_flags = cpu_read16(cpu, 0x00, 0x08C2);
    if (!(kong_flags & 0x4000))
      cpu_write16(cpu, 0x00, 0x08C2, (uint16_t)(kong_flags | 0x4000));
    /* The stock follower has no interaction mask and a back render order.
     * Promote only that dormant combination, preserving action-specific
     * masks, including damage invincibility, and custom render ordering. */
    uint16_t flags_address = (uint16_t)(cpu->D + sprite + 0x0030);
    uint16_t flags = cpu_read16(cpu, 0x00, flags_address);
    if (flags == 0 || flags == 6)
      cpu_write16(cpu, 0x00, flags_address, 0x001E);
    uint16_t order_address = (uint16_t)(cpu->D + sprite + 0x0002);
    if (cpu_read16(cpu, 0x00, order_address) == 0x00D8)
      cpu_write16(cpu, 0x00, order_address, 0x00E4);
  }
  return state;
}

bool Dkc2CoopUseFollowerClipping(CpuState *cpu) {
  if (!cpu || !Dkc2CoopSimultaneousActive(
                  cpu_read16(cpu, 0x00, kDkc2CoopModeFlag),
                  s_coop_mode == kDkc2CoopSimultaneous))
    return false;
  uint16_t sprite = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  return (sprite == kDkc2CoopSlotA || sprite == kDkc2CoopSlotB) &&
         sprite == cpu_read16(cpu, 0x00, 0x0597) &&
         cpu_read16(cpu, 0x00, (uint16_t)(cpu->D + sprite + 0x0030)) != 0;
}

uint16_t Dkc2CoopRecoveryFollowerValue(CpuState *cpu, uint16_t original) {
  if (!cpu || !Dkc2CoopSimultaneousActive(
                  cpu_read16(cpu, 0, kDkc2CoopModeFlag),
                  s_coop_mode == kDkc2CoopSimultaneous))
    return original;
  uint16_t sprite = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  if (sprite != kDkc2CoopSlotA && sprite != kDkc2CoopSlotB)
    return original;
  uint16_t state = CoopReadDp(cpu, (uint16_t)(sprite + 0x2E));
  /* Stock follower recovery changes the animation but deliberately leaves
   * the action state intact. A controlled Kong must finish roll/bounce
   * recovery through the normal idle transition, after landing/animation. */
  return state == 0x0004 || state == 0x0016 || state == 0x003F ? 0 : original;
}

uint16_t Dkc2CoopFollowerPaletteOffset(CpuState *cpu, uint16_t original) {
  /* The normal inactive-Kong palette is 15 colors past its bright palette.
   * The adapter wraps only that offset, leaving status-effect palettes alone. */
  return cpu && Dkc2CoopSimultaneousActive(
                    cpu_read16(cpu, 0, kDkc2CoopModeFlag),
                    s_coop_mode == kDkc2CoopSimultaneous) ? 0 : original;
}

uint16_t Dkc2CoopRecordInteractionSource(CpuState *cpu, uint16_t source) {
  /* Called only when the cartridge accepts a new highest-priority reaction.
   * $0A84 names the enemy, while $6A names the Kong that actually collided.
   * Capture that Kong now: later enemy checks can overwrite $6A. */
  s_stomp_source = s_stomp_kong = 0;
  s_mount_source = s_mount_kong = 0;
  if (cpu && Dkc2CoopSimultaneousActive(
                 cpu_read16(cpu, 0, kDkc2CoopModeFlag),
                 s_coop_mode == kDkc2CoopSimultaneous) &&
      (CoopReadAbs(cpu, 0x0A82) == 0x001B ||
       CoopReadAbs(cpu, 0x0A82) == 0x0017)) {
    uint16_t kong = CoopReadDp(cpu, 0x006A);
    if (kong == kDkc2CoopSlotA || kong == kDkc2CoopSlotB) {
      if (CoopReadAbs(cpu, 0x0A82) == 0x0017) {
        s_mount_source = source;
        s_mount_kong = kong;
      } else {
        s_stomp_source = source;
        s_stomp_kong = kong;
      }
    }
  }
  return source;
}

bool Dkc2CoopUseBothMountColliders(CpuState *cpu) {
  if (!CoopEnabled(cpu)) return false;
  uint16_t kong = cpu_read16(cpu, 0, 0x0597);
  uint16_t animal = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  if ((kong != kDkc2CoopSlotA && kong != kDkc2CoopSlotB) ||
      !(cpu_read16(cpu, 0, (uint16_t)(kong + 0x30)) & 4)) return false;
  uint16_t state = cpu_read16(cpu, 0, (uint16_t)(kong + 0x2E));
  if (state >= 0x70 || (cpu_read16(cpu, 0xB8, (uint16_t)(0x9620 + state * 4)) & 9))
    return false;
  /* A stationary Kong beside the animal must not mask the other player's
   * valid landing. The existing two-Kong collision routine tries this
   * candidate first and then the leader if their hitboxes do not overlap. */
  uint16_t type = cpu_read16(cpu, 0, animal);
  if (type == 0x198) return !(cpu_read16(cpu, 0, (uint16_t)(kong + 0x1E)) & 0x1001);
  return (type == 0x1A0 || !(cpu_read16(cpu, 0, (uint16_t)(kong + 0x24)) & 0x8000)) &&
         cpu_read16(cpu, 0, (uint16_t)(kong + 10)) < cpu_read16(cpu, 0, (uint16_t)(animal + 10)) &&
         !(cpu_read16(cpu, 0, (uint16_t)(kong + 0x1E)) & 0x100);
}

uint16_t Dkc2CoopBananaSecondWidth(CpuState *cpu, uint16_t original) {
  s_banana_extra_pass = false;
  if (!CoopEnabled(cpu)) return original;
  uint16_t follower = cpu_read16(cpu, 0, 0x0597);
  if ((follower != kDkc2CoopSlotA && follower != kDkc2CoopSlotB) ||
      !cpu_read16(cpu, 0, (uint16_t)(follower + 0x30)) ||
      (s_lost_mask & (follower == kDkc2CoopSlotA ? 1 : 2)) ||
      !cpu_read16(cpu, 0, 0x09B7) || !cpu_read16(cpu, 0, 0x09EF) ||
      !cpu_read16(cpu, 0, 0x09CF) || !original)
    return original;

  /* The banana list accepts two rectangles: leader, then animal, with the
   * follower admitted only if either is absent. Temporarily use the second
   * scratch rectangle for the independently controlled follower. The normal
   * group walker clears collected bits, so overlapping passes cannot award
   * the same banana twice. Restore the animal rectangle before its pass.
   * This starts/finishes within one group; snapshots occur between frames. */
  uint16_t left = cpu_read16(cpu, 0, 0x09CB);
  uint16_t top = cpu_read16(cpu, 0, 0x09CD);
  uint16_t width = (uint16_t)(cpu_read16(cpu, 0, 0x09CF) - left);
  uint16_t height = (uint16_t)(cpu_read16(cpu, 0, 0x09D1) - top);
  if (!width || !height) return original;
  const uint16_t bounds[] = {left, top, width, height};
  for (unsigned i = 0; i < 4; ++i) {
    uint16_t address = (uint16_t)(0x0D3C + i * 4);
    s_banana_second_bounds[i] = cpu_read16(cpu, 0, address);
    cpu_write16(cpu, 0, address, bounds[i]);
  }
  s_banana_extra_pass = true;
  return width;
}

bool Dkc2CoopBananaNextPass(CpuState *cpu) {
  if (!cpu || !s_banana_extra_pass) return false;
  s_banana_extra_pass = false;
  for (unsigned i = 0; i < 4; ++i)
    cpu_write16(cpu, 0, (uint16_t)(0x0D3C + i * 4), s_banana_second_bounds[i]);
  return true;
}

static uint16_t CoopHandoffPosition(CpuState *cpu, uint16_t original, uint16_t offset) {
  if (CoopEnabled(cpu)) {
    uint16_t kong = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
    if (kong == kDkc2CoopSlotA || kong == kDkc2CoopSlotB)
      return cpu_read16(cpu, 0, (uint16_t)(kong + offset));
  }
  return original;
}

static bool CoopControlsCurrentKong(CpuState *cpu) {
  if (!CoopEnabled(cpu)) return false;
  uint16_t kong = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  return kong == kDkc2CoopSlotA || kong == kDkc2CoopSlotB;
}

bool Dkc2CoopHandoffInPlace(CpuState *cpu) { return CoopControlsCurrentKong(cpu); }

static void CoopScreenSlack(CpuState *cpu, int *left, int *right) {
  *left = *right = 0;
  if (!CoopControlsCurrentKong(cpu) || !Dkc2VideoTerrainReady()) return;
  int extra = Dkc2VideoExtra(), bias = Dkc2VideoPresentationBias();
  int camera = cpu_read16(cpu, 0, 0x17BA);
  int maximum = cpu_read16(cpu, 0, 0x0AFC);
  int west = camera > 0x100 ? camera - 0x100 : 0;
  int east = maximum > camera ? maximum - camera : 0;
  *left = extra - bias;
  *right = extra + bias;
  if (*left < 0) *left = 0;
  if (*right < 0) *right = 0;
  if (*left > west) *left = west;
  if (*right > east) *right = east;
}

uint16_t Dkc2CoopScreenLeftValue(CpuState *cpu, uint16_t original) {
  int left, right;
  CoopScreenSlack(cpu, &left, &right);
  /* This operand is subtracted before the camera. Signed wrapping is the
   * guest's normal 16-bit arithmetic for a left margin beyond screen X=0. */
  return (uint16_t)(original - left);
}

uint16_t Dkc2CoopScreenSpanValue(CpuState *cpu, uint16_t original) {
  int left, right;
  CoopScreenSlack(cpu, &left, &right);
  return (uint16_t)(original + left + right);
}

uint16_t Dkc2CoopHandoffXValue(CpuState *cpu, uint16_t original) {
  return CoopHandoffPosition(cpu, original, 6);
}

uint16_t Dkc2CoopHandoffYValue(CpuState *cpu, uint16_t original) {
  return CoopHandoffPosition(cpu, original, 10);
}

uint16_t Dkc2CoopMountCandidateValue(CpuState *cpu, uint16_t original) {
  if (!CoopEnabled(cpu)) return original;
  /* The state-flags helper is also called by barrel logic. Scope the
   * alternate collider to an unmounted animal's own update. */
  uint16_t type = cpu_read16(cpu, 0, CoopReadDp(cpu, kDkc2CoopCurrentSprite));
  if (cpu_read16(cpu, 0, 0x006E) || type < 0x190 || type > 0x1A0 || (type & 3))
    return original;
  uint16_t kong = CoopReadDp(cpu, 0x6A);
  if ((kong == kDkc2CoopSlotA || kong == kDkc2CoopSlotB) &&
      (cpu_read16(cpu, 0, (uint16_t)(kong + 0x30)) & 4))
    return kong;
  return original;
}

void Dkc2CoopPrepareAnimalMount(CpuState *cpu) {
  uint16_t source = s_mount_source, kong = s_mount_kong;
  s_mount_source = s_mount_kong = 0;
  if (!CoopEnabled(cpu) || !source ||
      source != cpu_read16(cpu, 0, 0x0A84) ||
      (kong != kDkc2CoopSlotA && kong != kDkc2CoopSlotB) ||
      cpu_read16(cpu, 0, 0x006E) || kong != cpu_read16(cpu, 0, 0x0597))
    return;
  /* The cartridge has one animal/rider context. Make its actual rider the
   * camera leader before the accepted mount runs. Swap selector metadata,
   * preserving both players' positions, action states and collision masks.
   * Controller routing remains bound to the two fixed Kong slots. These
   * guest selectors also make ownership survive saves and rewind. */
  CoopMakeLeader(cpu, kong);
}

uint16_t Dkc2CoopAnimalFollowerFlags(CpuState *cpu, uint16_t original) {
  return CoopEnabled(cpu) ? (uint16_t)(original & ~0x4000) : original;
}

uint16_t Dkc2CoopAnimalTypeValue(CpuState *cpu, uint16_t original) {
  if (CoopEnabled(cpu)) {
    uint16_t kong = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
    if ((kong == kDkc2CoopSlotA || kong == kDkc2CoopSlotB) &&
        kong == cpu_read16(cpu, 0, 0x0597)) return 0;
  }
  return original;
}

uint8_t Dkc2CoopTakeStompEvents(void) {
  uint8_t events = s_stomp_events;
  s_stomp_events = 0;
  return events;
}

bool Dkc2CoopRopeUsesFollower(CpuState *cpu) {
  if (!CoopEnabled(cpu)) return false;
  /* Terrain contact queues the actual Kong in $0A84. Unlike an enemy
   * reaction, its source is the player itself. This guest-owned value
   * survives a snapshot taken between contact and reaction dispatch. */
  uint16_t source = cpu_read16(cpu, 0, 0x0A84);
  return (source == kDkc2CoopSlotA || source == kDkc2CoopSlotB) &&
         source == cpu_read16(cpu, 0, 0x0597);
}

uint16_t Dkc2CoopRopeAnimationFollowerValue(CpuState *cpu, uint16_t original) {
  if (!CoopEnabled(cpu)) return original;
  uint16_t kong = cpu->X;
  uint16_t state = cpu_read16(cpu, 0, (uint16_t)(kong + 0x2E));
  if ((kong == kDkc2CoopSlotA || kong == kDkc2CoopSlotB) &&
      state >= 0x35 && state <= 0x38)
    return 0xFFFF;
  return original;
}

bool Dkc2CoopBounceUsesFollower(CpuState *cpu) {
  uint16_t source = s_stomp_source, kong = s_stomp_kong;
  s_stomp_source = s_stomp_kong = 0;
  bool follower = cpu && Dkc2CoopSimultaneousActive(
      cpu_read16(cpu, 0, kDkc2CoopModeFlag), s_coop_mode == kDkc2CoopSimultaneous) &&
      (kong == kDkc2CoopSlotA || kong == kDkc2CoopSlotB) &&
      source == CoopReadAbs(cpu, 0x0A84) && kong == cpu_read16(cpu, 0, 0x0597);
  /* This hook runs only in player_interaction_1B, after the cartridge has
   * accepted the stomp. Observe its owner without changing guest state. */
  if (cpu) {
    uint16_t mode = cpu_read16(cpu, 0, kDkc2CoopModeFlag);
    uint16_t owner = cpu_read16(cpu, 0, follower ? 0x0597 : 0x0593);
    uint16_t controller = mode == 0 ? 1 : cpu_read16(cpu, 0, 0x08A2);
    if (Dkc2CoopSimultaneousActive(mode, s_coop_mode == kDkc2CoopSimultaneous))
      controller = owner == kDkc2CoopSlotA ? 1 : 2;
    if ((owner == kDkc2CoopSlotA || owner == kDkc2CoopSlotB) &&
        controller >= 1 && controller <= 2)
      s_stomp_events |= (uint8_t)(1u << (controller - 1));
  }
  return follower;
}

uint16_t Dkc2CoopSelectHeldWord(CpuState *cpu, uint16_t original) {
  if (!cpu)
    return original;
  uint16_t sprite = CoopReadDp(cpu, kDkc2CoopCurrentSprite);
  uint16_t mode = CoopReadAbs(cpu, kDkc2CoopModeFlag);
  uint16_t result = Dkc2CoopSelectHeld(
      original, sprite, mode, s_coop_mode == kDkc2CoopSimultaneous,
      CoopReadAbs(cpu, kDkc2CoopP1Held), CoopReadAbs(cpu, kDkc2CoopP2Held));
  if (CoopDebugEnabled() && s_debug_held_count < 40) {
    s_debug_held_count++;
    fprintf(stderr, "coop_held #%d sprite=%04x mode=%u orig=%04x -> %04x\n",
            s_debug_held_count, sprite, mode, original, result);
  }
  return result;
}

uint16_t Dkc2CoopSelectPressedWord(CpuState *cpu, uint16_t original) {
  if (!cpu)
    return original;
  return Dkc2CoopSelectPressed(original,
                               CoopReadDp(cpu, kDkc2CoopCurrentSprite),
                               CoopReadAbs(cpu, kDkc2CoopModeFlag),
                               s_coop_mode == kDkc2CoopSimultaneous,
                               CoopReadAbs(cpu, kDkc2CoopP1Pressed),
                               CoopReadAbs(cpu, kDkc2CoopP2Pressed));
}
