#ifndef DKC2_COOP_H
#define DKC2_COOP_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simultaneous two-player co-op for DKC2's "2 PLAYER TEAM" mode.
 *
 * The cartridge alternates control in TEAM mode: every frame the input
 * dispatch ($80:8A00) fills one global "active" input word from exactly
 * one controller chosen by the active-Kong selector $08A2, and the Kong
 * action gate ($B8:B9C7 inside process_player_action) lets only the
 * active Kong ($0593) read it; the other Kong follows by AI. Control
 * passes when the active Kong is hurt and at level boundaries.
 *
 * With the simultaneous policy selected, both Kongs read their own
 * controller every frame instead. The two Kong slots are fixed by the
 * cartridge's set_active_kong ($80:883B): slot A is always controller
 * 1's Kong and slot B always controller 2's. Hurt, barrel-respawn, and
 * swap handlers remain in place. Full-game hurt/respawn interactions require
 * separate acceptance testing. Normal follower collisions and colors are
 * promoted, and queued stomps bounce the colliding Kong. First-level attack
 * and contact-damage routes cover this boundary. 1 PLAYER and 2 PLAYER CONTEST are never
 * altered, and with the classic policy selected the helpers return
 * cartridge values exactly, byte-for-byte.
 */
typedef enum Dkc2CoopMode {
  kDkc2CoopSimultaneous = 0,
  kDkc2CoopClassic,
  kDkc2CoopModeCount,
} Dkc2CoopMode;

/* The two Kong slots from set_active_kong ($80:883B). Slot A is bound to
 * controller 1 ($08A2 = 1) and slot B to controller 2 ($08A2 = 2). */
enum {
  kDkc2CoopSlotA = 0x0DE2,
  kDkc2CoopSlotB = 0x0E40,
};

/* WRAM words the policy reads (bank $00, direct-page/absolute). */
enum {
  kDkc2CoopCurrentSprite = 0x0064, /* current_sprite */
  kDkc2CoopModeFlag = 0x060D,      /* 0 = 1P, 1 = 2P TEAM, 2 = 2P CONTEST */
  kDkc2CoopP1Held = 0x0502,
  kDkc2CoopP2Held = 0x0504,
  kDkc2CoopP1Pressed = 0x0506,
  kDkc2CoopP2Pressed = 0x0508,
};

void Dkc2CoopSetMode(Dkc2CoopMode mode);
Dkc2CoopMode Dkc2CoopGetMode(void);
void Dkc2CoopResetSession(void);
uint8_t Dkc2CoopSaveLostMask(void);
void Dkc2CoopLoadLostMask(CpuState *cpu, uint8_t mask, bool recorded);
bool Dkc2CoopModeFromName(const char *text, Dkc2CoopMode *mode);

/* Host-neutral decision core, free of CpuState so the policy is
 * unit-testable without the runtime. `mode_flag` is the raw $060D value
 * and `sprite` the current player sprite address. */
bool Dkc2CoopSimultaneousActive(uint16_t mode_flag, bool enabled);
uint16_t Dkc2CoopGateActive(uint16_t active_slot, uint16_t sprite,
                            uint16_t mode_flag, bool enabled);
bool Dkc2CoopFollowerNeedsControl(uint16_t follower_state,
                                  uint16_t follower_held);
uint16_t Dkc2CoopSelectHeld(uint16_t original, uint16_t sprite,
                            uint16_t mode_flag, bool enabled,
                            uint16_t p1_held, uint16_t p2_held);
uint16_t Dkc2CoopSelectPressed(uint16_t original, uint16_t sprite,
                               uint16_t mode_flag, bool enabled,
                               uint16_t p1_pressed, uint16_t p2_pressed);

/* Generated-code entry points. The generation adapter wraps the gate
 * compare's $0593 read and the input path's $050E/$0510 reads with these;
 * the state wrapper also precedes follower dispatch. The clipping, palette
 * and stomp hooks are narrow adaptations at their named game functions.
 * Value wrappers return the
 * cartridge value unchanged unless simultaneous co-op is
 * active for the current frame and sprite. */
uint16_t Dkc2CoopGateActiveValue(CpuState *cpu, uint16_t active_slot);
uint16_t Dkc2CoopSelectStateWord(CpuState *cpu, uint16_t original);
bool Dkc2CoopUseFollowerClipping(CpuState *cpu);
uint16_t Dkc2CoopRecoveryFollowerValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopRecordPickupValue(CpuState *cpu, uint16_t object);
uint16_t Dkc2CoopHeldOwnerValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopHeldObjectValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopFollowerPaletteOffset(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopRecordInteractionSource(CpuState *cpu, uint16_t source);
/* Consume host-only stomp notifications: bit 0=P1, bit 1=P2. */
uint8_t Dkc2CoopTakeStompEvents(void);
bool Dkc2CoopBounceUsesFollower(CpuState *cpu);
bool Dkc2CoopUseBothMountColliders(CpuState *cpu);
uint16_t Dkc2CoopBananaSecondWidth(CpuState *cpu, uint16_t original);
bool Dkc2CoopBananaNextPass(CpuState *cpu);
bool Dkc2CoopHandoffInPlace(CpuState *cpu);
uint16_t Dkc2CoopScreenLeftValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopScreenSpanValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopHandoffXValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopHandoffYValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopMountCandidateValue(CpuState *cpu, uint16_t original);
void Dkc2CoopPrepareAnimalMount(CpuState *cpu);
uint16_t Dkc2CoopAnimalFollowerFlags(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopAnimalTypeValue(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopSelectHeldWord(CpuState *cpu, uint16_t original);
uint16_t Dkc2CoopSelectPressedWord(CpuState *cpu, uint16_t original);

#ifdef __cplusplus
}
#endif

#endif
