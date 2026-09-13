#include "dkc2_coop.h"
#include "dkc2_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Synthetic guest memory for the wrapper tests: one 64 KiB WRAM bank.
 * WRAM addresses in banks $00 and $7E alias the same bytes. The synthetic
 * animal eligibility table uses separate, otherwise unused high addresses,
 * so the stub ignores the bank argument; the
 * direct-page read resolves at D+offset exactly like the 65816 contract.
 */
static uint8_t s_fake_wram[0x10000];

uint16 cpu_read16(CpuState *cpu, uint8 bank, uint16 addr) {
  (void)cpu;
  (void)bank;
  return (uint16)(s_fake_wram[addr] |
                  (uint16)((uint16)s_fake_wram[(uint16)(addr + 1)] << 8));
}

void cpu_write16(CpuState *cpu, uint8 bank, uint16 addr, uint16 value) {
  (void)cpu;
  (void)bank;
  s_fake_wram[addr] = (uint8_t)value;
  s_fake_wram[(uint16_t)(addr + 1u)] = (uint8_t)(value >> 8);
}

static void WriteWord(uint16_t address, uint16_t value) {
  s_fake_wram[address] = (uint8_t)value;
  s_fake_wram[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8);
}

static int s_failures = 0;

static void ExpectU16(const char *what, uint16_t actual, uint16_t expected) {
  if (actual != expected) {
    fprintf(stderr, "%s: expected 0x%04x, got 0x%04x\n", what, expected,
            actual);
    s_failures++;
  }
}

static void ExpectBool(const char *what, int actual, int expected) {
  if ((actual != 0) != (expected != 0)) {
    fprintf(stderr, "%s: expected %d, got %d\n", what, expected,
            actual != 0);
    s_failures++;
  }
}

static void TestModeAccessors(void) {
  Dkc2CoopSetMode(kDkc2CoopClassic);
  ExpectBool("mode round-trips classic", Dkc2CoopGetMode() == kDkc2CoopClassic,
             1);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  ExpectBool("mode round-trips simultaneous",
             Dkc2CoopGetMode() == kDkc2CoopSimultaneous, 1);
  Dkc2CoopSetMode((Dkc2CoopMode)-1);
  ExpectBool("invalid mode clamps to simultaneous",
             Dkc2CoopGetMode() == kDkc2CoopSimultaneous, 1);
  Dkc2CoopSetMode(kDkc2CoopModeCount);
  ExpectBool("out-of-range mode clamps to simultaneous",
             Dkc2CoopGetMode() == kDkc2CoopSimultaneous, 1);
}

static void TestModeFromName(void) {
  Dkc2CoopMode mode = kDkc2CoopClassic;
  ExpectBool("parses simultaneous",
             Dkc2CoopModeFromName("simultaneous", &mode), 1);
  ExpectBool("simultaneous value", mode == kDkc2CoopSimultaneous, 1);
  ExpectBool("parses classic", Dkc2CoopModeFromName("classic", &mode), 1);
  ExpectBool("classic value", mode == kDkc2CoopClassic, 1);
  ExpectBool("rejects invalid name",
             Dkc2CoopModeFromName("alternating", &mode), 0);
  ExpectBool("rejects empty name", Dkc2CoopModeFromName("", &mode), 0);
  ExpectBool("rejects null text", Dkc2CoopModeFromName(NULL, &mode), 0);
  ExpectBool("rejects null out", Dkc2CoopModeFromName("classic", NULL), 0);
}

static void TestSimultaneousActive(void) {
  ExpectBool("team mode activates", Dkc2CoopSimultaneousActive(1, 1), 1);
  ExpectBool("disabled policy never activates",
             Dkc2CoopSimultaneousActive(1, 0), 0);
  ExpectBool("1 player mode untouched", Dkc2CoopSimultaneousActive(0, 1), 0);
  ExpectBool("contest mode untouched", Dkc2CoopSimultaneousActive(2, 1), 0);
  ExpectBool("out-of-range mode untouched",
             Dkc2CoopSimultaneousActive(3, 1), 0);
}

static void TestGateActive(void) {
  const uint16_t active = 0x0DE2;
  /* Classic: the cartridge compare decides, unchanged. */
  ExpectU16("classic keeps active slot",
            Dkc2CoopGateActive(active, 0x0E40, 1, 0), active);
  /* Simultaneous: both Kong slots compare equal and take the input path. */
  ExpectU16("slot A gate opens",
            Dkc2CoopGateActive(active, 0x0DE2, 1, 1), 0x0DE2);
  ExpectU16("slot B gate opens",
            Dkc2CoopGateActive(active, 0x0E40, 1, 1), 0x0E40);
  /* Simultaneous but not a Kong slot: fall back to the cartridge compare. */
  ExpectU16("non-kong sprite keeps cartridge compare",
            Dkc2CoopGateActive(active, 0x1234, 1, 1), active);
  /* Non-TEAM modes never open the gate. */
  ExpectU16("1 player keeps cartridge compare",
            Dkc2CoopGateActive(active, 0x0DE2, 0, 1), active);
  ExpectU16("contest keeps cartridge compare",
            Dkc2CoopGateActive(active, 0x0E40, 2, 1), active);
}

static void TestSelectWords(void) {
  /* Classic: the active words pass through unchanged. */
  ExpectU16("classic keeps held word",
            Dkc2CoopSelectHeld(0xAAAA, 0x0E40, 1, 0, 0x1111, 0x2222),
            0xAAAA);
  ExpectU16("classic keeps pressed word",
            Dkc2CoopSelectPressed(0xBBBB, 0x0DE2, 1, 0, 0x3333, 0x4444),
            0xBBBB);
  /* Simultaneous: each Kong slot reads its own controller's words. */
  ExpectU16("slot A held comes from controller 1",
            Dkc2CoopSelectHeld(0xAAAA, 0x0DE2, 1, 1, 0x1111, 0x2222),
            0x1111);
  ExpectU16("slot B held comes from controller 2",
            Dkc2CoopSelectHeld(0xAAAA, 0x0E40, 1, 1, 0x1111, 0x2222),
            0x2222);
  ExpectU16("slot A pressed comes from controller 1",
            Dkc2CoopSelectPressed(0xBBBB, 0x0DE2, 1, 1, 0x3333, 0x4444),
            0x3333);
  ExpectU16("slot B pressed comes from controller 2",
            Dkc2CoopSelectPressed(0xBBBB, 0x0E40, 1, 1, 0x3333, 0x4444),
            0x4444);
  /* Simultaneous but not a Kong slot: keep the active words. */
  ExpectU16("non-kong sprite keeps held word",
            Dkc2CoopSelectHeld(0xAAAA, 0x1234, 1, 1, 0x1111, 0x2222),
            0xAAAA);
  /* Non-TEAM modes keep the active words. */
  ExpectU16("1 player keeps held word",
            Dkc2CoopSelectHeld(0xAAAA, 0x0DE2, 0, 1, 0x1111, 0x2222),
            0xAAAA);
  ExpectU16("contest keeps pressed word",
            Dkc2CoopSelectPressed(0xBBBB, 0x0E40, 2, 1, 0x3333, 0x4444),
            0xBBBB);
}

static void TestFollowerNeedsControl(void) {
  ExpectBool("passive wait plus input takes control",
             Dkc2CoopFollowerNeedsControl(0x0013, 0x0400), 1);
  ExpectBool("passive wait without input stays passive",
             Dkc2CoopFollowerNeedsControl(0x0013, 0x0000), 0);
  ExpectBool("scripted states are never redirected",
             Dkc2CoopFollowerNeedsControl(0x003E, 0x0400), 0);
  ExpectBool("input states are never redirected",
             Dkc2CoopFollowerNeedsControl(0x0000, 0x0400), 0);
  ExpectBool("follow-history state returns to player control",
             Dkc2CoopFollowerNeedsControl(0x0022, 0x0400), 1);
  ExpectBool("released controller does not return to follow-history",
             Dkc2CoopFollowerNeedsControl(0x0022, 0x0000), 1);
}

static void TestWrappersAgainstFakeWram(void) {
  CpuState cpu;
  memset(&cpu, 0, sizeof cpu);
  cpu.D = 0x0000;
  cpu.DB = 0x00;
  memset(s_fake_wram, 0, sizeof s_fake_wram);

  WriteWord(0x0064, 0x0E40); /* current_sprite: slot B */
  WriteWord(0x060D, 1);      /* 2 PLAYER TEAM */
  WriteWord(0x0502, 0x0101); /* P1 held */
  WriteWord(0x0504, 0x0202); /* P2 held */
  WriteWord(0x0506, 0x0303); /* P1 pressed */
  WriteWord(0x0508, 0x0404); /* P2 pressed */

  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  Dkc2CoopResetSession();
  cpu.X = kDkc2CoopSlotB;
  cpu.DB = 0xB8; /* PHK/PLB precedes the dispatch read. */
  WriteWord(kDkc2CoopSlotB + 0x2E, 0x13);
  WriteWord(kDkc2CoopSlotB + 0x02, 0xD8);
  WriteWord(0x0597, kDkc2CoopSlotB);
  ExpectU16("waiting follower joins before its script executes",
            Dkc2CoopSelectStateWord(&cpu, 0x13), 0);
  ExpectU16("state word matches selected handler",
            cpu_read16(&cpu, 0, kDkc2CoopSlotB + 0x2E), 0);
  ExpectU16("joined player can interact",
            cpu_read16(&cpu, 0, kDkc2CoopSlotB + 0x30), 0x1E);
  ExpectU16("joined player is present for DK barrels",
            cpu_read16(&cpu, 0, 0x08C2), 0x4000);
  WriteWord(kDkc2CoopSlotB + 0x30, 6);
  (void)Dkc2CoopSelectStateWord(&cpu, 0);
  ExpectU16("follower mask after a leader swap regains enemy collisions",
            cpu_read16(&cpu, 0, kDkc2CoopSlotB + 0x30), 0x1E);
  ExpectU16("joined player uses normal render order",
            cpu_read16(&cpu, 0, kDkc2CoopSlotB + 0x02), 0xE4);
  ExpectBool("joined follower refreshes its own hitbox",
             Dkc2CoopUseFollowerClipping(&cpu), 1);
  WriteWord(kDkc2CoopSlotB + 0x30, 0x9E);
  (void)Dkc2CoopSelectStateWord(&cpu, 0);
  ExpectU16("hurt invincibility mask retained",
            cpu_read16(&cpu, 0, kDkc2CoopSlotB + 0x30), 0x9E);
  WriteWord(kDkc2CoopSlotB + 0x30, 0);
  (void)Dkc2CoopSelectStateWord(&cpu, 0x59);
  ExpectU16("scripted hurt state not promoted",
            cpu_read16(&cpu, 0, kDkc2CoopSlotB + 0x30), 0);
  ExpectBool("disabled follower does not create a hitbox",
             Dkc2CoopUseFollowerClipping(&cpu), 0);
  ExpectBool("null clipping context fails safe",
             Dkc2CoopUseFollowerClipping(NULL), 0);
  WriteWord(0x0504, 0);
  ExpectU16("neutral follower remains independent after landing",
            Dkc2CoopSelectStateWord(&cpu, 0x22), 0);
  for (uint16_t state = 0; state < 0x80; ++state) {
    if (state == 0x22 || state == 0x6F) continue;
    ExpectU16("other states preserved with neutral input",
              Dkc2CoopSelectStateWord(&cpu, state), state);
  }
  Dkc2CoopSetMode(kDkc2CoopClassic);
  ExpectU16("classic follow script preserved",
            Dkc2CoopSelectStateWord(&cpu, 0x22), 0x22);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  for (uint16_t mode = 0; mode <= 2; mode += 2) {
    WriteWord(0x060D, mode);
    ExpectU16("other game modes preserve follower state",
              Dkc2CoopSelectStateWord(&cpu, 0x22), 0x22);
  }
  WriteWord(0x060D, 1);
  ExpectU16("simultaneous follower uses bright palette",
            Dkc2CoopFollowerPaletteOffset(&cpu, 0x1E), 0);
  cpu.X = 0x1234;
  ExpectU16("non-Kong state stays untouched",
            Dkc2CoopSelectStateWord(&cpu, 0x22), 0x22);
  ExpectU16("null state wrapper fails safe",
            Dkc2CoopSelectStateWord(NULL, 0x22), 0x22);
  cpu.DB = 0;
  WriteWord(0x0504, 0x0202);

  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  ExpectU16("wrapper gate opens for slot B",
            Dkc2CoopGateActiveValue(&cpu, 0x0DE2), 0x0E40);
  ExpectU16("wrapper selects controller 2 held",
            Dkc2CoopSelectHeldWord(&cpu, 0x050E), 0x0202);
  ExpectU16("wrapper selects controller 2 pressed",
            Dkc2CoopSelectPressedWord(&cpu, 0x0510), 0x0404);

  WriteWord(0x0064, 0x0DE2); /* current_sprite: slot A */
  ExpectU16("wrapper gate opens for slot A",
            Dkc2CoopGateActiveValue(&cpu, 0x0E40), 0x0DE2);
  ExpectU16("wrapper selects controller 1 held",
            Dkc2CoopSelectHeldWord(&cpu, 0x050E), 0x0101);
  ExpectU16("wrapper selects controller 1 pressed",
            Dkc2CoopSelectPressedWord(&cpu, 0x0510), 0x0303);

  Dkc2CoopSetMode(kDkc2CoopClassic);
  ExpectU16("classic wrapper gate keeps active slot",
            Dkc2CoopGateActiveValue(&cpu, 0x0E40), 0x0E40);
  ExpectU16("classic wrapper keeps held word",
            Dkc2CoopSelectHeldWord(&cpu, 0x050E), 0x050E);
  ExpectU16("classic wrapper keeps pressed word",
            Dkc2CoopSelectPressedWord(&cpu, 0x0510), 0x0510);

  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  WriteWord(0x060D, 0); /* 1 PLAYER */
  ExpectU16("1 player wrapper gate keeps active slot",
            Dkc2CoopGateActiveValue(&cpu, 0x0E40), 0x0E40);
  ExpectU16("1 player wrapper keeps held word",
            Dkc2CoopSelectHeldWord(&cpu, 0x050E), 0x050E);

  /* A non-zero direct page still resolves current_sprite like hardware. */
  WriteWord(0x060D, 1);
  cpu.D = 0x0100;
  WriteWord(0x0164, 0x0E40);
  ExpectU16("wrapper honors direct page for current_sprite",
            Dkc2CoopGateActiveValue(&cpu, 0x0DE2), 0x0E40);

  /* NULL cpu fails safe to the cartridge values. */
  ExpectU16("null cpu gate fails safe", Dkc2CoopGateActiveValue(NULL, 0x1234),
            0x1234);
  ExpectU16("null cpu held fails safe", Dkc2CoopSelectHeldWord(NULL, 0x050E),
            0x050E);
  ExpectU16("null cpu pressed fails safe",
            Dkc2CoopSelectPressedWord(NULL, 0x0510), 0x0510);

  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
}

static void TestStompOwnership(void) {
  CpuState cpu;
  memset(&cpu, 0, sizeof cpu);
  memset(s_fake_wram, 0, sizeof s_fake_wram);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  WriteWord(0x060D, 1);
  WriteWord(0x0597, kDkc2CoopSlotB);
  WriteWord(0x006A, kDkc2CoopSlotB);
  WriteWord(0x0A82, 0x1B);
  WriteWord(0x0A84, 0x0E9E);
  ExpectU16("reaction enemy preserved",
            Dkc2CoopRecordInteractionSource(&cpu, 0x0E9E), 0x0E9E);
  WriteWord(0x006A, kDkc2CoopSlotA); /* later enemy check */
  ExpectBool("bounce belongs to the Kong at queue time",
             Dkc2CoopBounceUsesFollower(&cpu), 1);
  ExpectU16("P2 stomp routes rumble to P2", Dkc2CoopTakeStompEvents(), 2);
  ExpectU16("stomp events consumed once", Dkc2CoopTakeStompEvents(), 0);
  ExpectBool("reaction ownership consumed once",
             Dkc2CoopBounceUsesFollower(&cpu), 0);
  (void)Dkc2CoopRecordInteractionSource(&cpu, 0x0E9E);
  ExpectBool("active Kong keeps stock bounce", Dkc2CoopBounceUsesFollower(&cpu), 0);
  WriteWord(0x006A, kDkc2CoopSlotB);
  (void)Dkc2CoopRecordInteractionSource(&cpu, 0x0E9E);
  WriteWord(0x0A82, 0x20);
  (void)Dkc2CoopRecordInteractionSource(&cpu, 0x0EFC);
  ExpectBool("higher priority reaction replaces ownership",
             Dkc2CoopBounceUsesFollower(&cpu), 0);
  WriteWord(0x0A82, 0x1B);
  (void)Dkc2CoopRecordInteractionSource(&cpu, 0x0EFC);
  ExpectBool("different reaction source fails safe", Dkc2CoopBounceUsesFollower(&cpu), 0);
  for (uint16_t mode = 0; mode <= 2; ++mode) {
    WriteWord(0x060D, mode);
    Dkc2CoopSetMode(mode == 1 ? kDkc2CoopClassic : kDkc2CoopSimultaneous);
    ExpectU16("classic, solo and contest keep follower palette",
              Dkc2CoopFollowerPaletteOffset(&cpu, 0x1E), 0x1E);
    (void)Dkc2CoopRecordInteractionSource(&cpu, 0x0E9E);
    ExpectBool("classic, solo and contest keep stock bounce",
               Dkc2CoopBounceUsesFollower(&cpu), 0);
  }
  for (uint16_t mode = 0; mode <= 2; ++mode) {
    WriteWord(0x060D, mode);
    Dkc2CoopSetMode(kDkc2CoopClassic);
    WriteWord(0x0593, kDkc2CoopSlotB);
    WriteWord(0x08A2, 2);
    (void)Dkc2CoopBounceUsesFollower(&cpu);
    ExpectU16("solo uses P1; alternating modes use active controller",
              Dkc2CoopTakeStompEvents(), mode == 0 ? 1 : 2);
    (void)Dkc2CoopBounceUsesFollower(&cpu);
    Dkc2CoopResetSession();
    ExpectU16("load/reset clears pending rumble", Dkc2CoopTakeStompEvents(), 0);
  }
  ExpectU16("null queue preserves source", Dkc2CoopRecordInteractionSource(NULL, 7), 7);
  ExpectBool("null bounce fails safe", Dkc2CoopBounceUsesFollower(NULL), 0);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
}

static void TestRecovery(void) {
  CpuState cpu;
  memset(&cpu, 0, sizeof cpu);
  memset(s_fake_wram, 0, sizeof s_fake_wram);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  WriteWord(0x060D, 1);
  WriteWord(0x0064, kDkc2CoopSlotB);
  for (uint16_t state = 0; state < 0x80; ++state) {
    WriteWord(kDkc2CoopSlotB + 0x2E, state);
    ExpectU16("only roll, bounce and throw recovery use the active transition",
              Dkc2CoopRecoveryFollowerValue(&cpu, kDkc2CoopSlotB),
              state == 4 || state == 0x16 || state == 0x3F ? 0 : kDkc2CoopSlotB);
    ExpectU16("recovery gate does not prematurely change the action state",
              cpu_read16(&cpu, 0, kDkc2CoopSlotB + 0x2E), state);
  }
  WriteWord(kDkc2CoopSlotB + 0x2E, 0x16);
  for (uint16_t mode = 0; mode <= 2; ++mode) {
    WriteWord(0x060D, mode);
    Dkc2CoopSetMode(mode == 1 ? kDkc2CoopClassic : kDkc2CoopSimultaneous);
    ExpectU16("stock recovery modes unaffected",
              Dkc2CoopRecoveryFollowerValue(&cpu, kDkc2CoopSlotB), kDkc2CoopSlotB);
  }
  ExpectU16("null recovery is unchanged", Dkc2CoopRecoveryFollowerValue(NULL, 7), 7);
}

static void TestHeldObjectOwnership(void) {
  CpuState cpu;
  const uint16_t object = 0x0F5A;
  memset(&cpu, 0, sizeof cpu);
  memset(s_fake_wram, 0, sizeof s_fake_wram);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  WriteWord(0x060D, 1);
  WriteWord(0x0597, kDkc2CoopSlotB);
  WriteWord(0x0064, kDkc2CoopSlotB);
  WriteWord(object, 0x1A8);
  ExpectU16("accepted pickup keeps the object's slot",
            Dkc2CoopRecordPickupValue(&cpu, object), object);
  WriteWord(0x0D7A, object);
  ExpectU16("P2 owns its picked-up object",
            Dkc2CoopHeldOwnerValue(&cpu, kDkc2CoopSlotA), kDkc2CoopSlotB);
  ExpectU16("P2 still sees its held object", Dkc2CoopHeldObjectValue(&cpu, object), object);
  WriteWord(0x0064, kDkc2CoopSlotA);
  ExpectU16("P1 recovery does not adopt P2's barrel",
            Dkc2CoopHeldObjectValue(&cpu, object), 0);
  WriteWord(0x0064, object);
  ExpectU16("barrel position uses its owner",
            Dkc2CoopHeldOwnerValue(&cpu, kDkc2CoopSlotA), kDkc2CoopSlotB);
  cpu.X = kDkc2CoopSlotB;
  ExpectU16("follower transition preserves independent carry",
            Dkc2CoopSelectStateWord(&cpu, 0x22), 0x0C);
  WriteWord(0x0D7A, 0);
  ExpectU16("released object no longer overrides owner",
            Dkc2CoopHeldOwnerValue(&cpu, kDkc2CoopSlotA), kDkc2CoopSlotA);
  WriteWord(0x0D7A, object);
  WriteWord(object, 0x1BC);
  WriteWord(kDkc2CoopSlotB + 0x2E, 0);
  ExpectU16("recycled sprite does not inherit stale ownership",
            Dkc2CoopHeldOwnerValue(&cpu, kDkc2CoopSlotA), kDkc2CoopSlotA);
  WriteWord(0x0064, kDkc2CoopSlotA);
  (void)Dkc2CoopRecordPickupValue(&cpu, object);
  ExpectU16("P1 pickup retains P1 ownership",
            Dkc2CoopHeldOwnerValue(&cpu, kDkc2CoopSlotB), kDkc2CoopSlotA);
  Dkc2CoopLoadLostMask(&cpu, 0, true); /* drop all transient pickup history */
  WriteWord(kDkc2CoopSlotB + 0x2E, 0x0F);
  ExpectU16("loaded throw reconstructs P2 ownership from guest state",
            Dkc2CoopHeldOwnerValue(&cpu, kDkc2CoopSlotA), kDkc2CoopSlotB);
  for (uint16_t mode = 0; mode <= 2; ++mode) {
    WriteWord(0x060D, mode);
    Dkc2CoopSetMode(mode == 1 ? kDkc2CoopClassic : kDkc2CoopSimultaneous);
    (void)Dkc2CoopRecordPickupValue(&cpu, object);
    ExpectU16("classic, solo and contest preserve held owner",
              Dkc2CoopHeldOwnerValue(&cpu, 123), 123);
    ExpectU16("classic, solo and contest preserve held object",
              Dkc2CoopHeldObjectValue(&cpu, object), object);
  }
  ExpectU16("null pickup is unchanged", Dkc2CoopRecordPickupValue(NULL, object), object);
  ExpectU16("null held owner is unchanged", Dkc2CoopHeldOwnerValue(NULL, 7), 7);
  ExpectU16("null held object is unchanged", Dkc2CoopHeldObjectValue(NULL, 7), 7);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
}

static void TestHurtLifecycle(void) {
  CpuState cpu;
  memset(&cpu, 0, sizeof cpu);
  memset(s_fake_wram, 0, sizeof s_fake_wram);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  Dkc2CoopResetSession();
  WriteWord(0x060D, 1);
  WriteWord(0x0597, kDkc2CoopSlotB);
  for (uint16_t slot = kDkc2CoopSlotA; slot <= kDkc2CoopSlotB; slot += 0x5E) {
    cpu.X = slot;
    WriteWord(slot == kDkc2CoopSlotA ? 0x0502 : 0x0504, 0xFFFF);
    ExpectU16("hurt animation stays in charge", Dkc2CoopSelectStateWord(&cpu, 0x24), 0x24);
    ExpectU16("runaway animation stays in charge", Dkc2CoopSelectStateWord(&cpu, 0x25), 0x25);
    ExpectU16("held input cannot revive a lost Kong", Dkc2CoopSelectStateWord(&cpu, 0x13), 0x13);
    uint8_t saved = Dkc2CoopSaveLostMask();
    Dkc2CoopLoadLostMask(&cpu, saved, true);
    ExpectU16("saved loss survives reload", Dkc2CoopSelectStateWord(&cpu, 0x13), 0x13);
    Dkc2CoopSetMode(kDkc2CoopClassic);
    Dkc2CoopSetMode(kDkc2CoopSimultaneous);
    ExpectU16("changing policy does not revive a lost Kong", Dkc2CoopSelectStateWord(&cpu, 0x13), 0x13);
    ExpectU16("accepted barrel rescue preserves its animation", Dkc2CoopSelectStateWord(&cpu, 0x3E), 0x3E);
    ExpectU16("rescued Kong becomes independently controllable", Dkc2CoopSelectStateWord(&cpu, 0x22), 0);
  }
  WriteWord(0x08C2, 0x100);
  WriteWord(cpu.X + 0x1C, 0xC000);
  WriteWord(cpu.X + 0x30, 0x9E);
  ExpectU16("handoff continues existing jump", Dkc2CoopSelectStateWord(&cpu, 0x6F), 6);
  ExpectU16("handoff clears turn pause", cpu_read16(&cpu, 0, 0x08C2), 0);
  ExpectU16("handoff clears wait blink", cpu_read16(&cpu, 0, cpu.X + 0x1C), 0);
  ExpectU16("handoff retains damage grace period", cpu_read16(&cpu, 0, cpu.X + 0x30), 0x9E);
  WriteWord(kDkc2CoopSlotB + 0x2E, 0x13);
  WriteWord(kDkc2CoopSlotB + 2, 0xE4);
  Dkc2CoopLoadLostMask(&cpu, 0, false);
  ExpectU16("legacy lost Kong cannot rejoin", Dkc2CoopSelectStateWord(&cpu, 0x13), 0x13);
  Dkc2CoopResetSession();
  ExpectU16("fresh session permits initial join", Dkc2CoopSelectStateWord(&cpu, 0x13), 0);
}

static void TestAnimalsAndHandoff(void) {
  CpuState cpu;
  memset(&cpu, 0, sizeof cpu);
  memset(s_fake_wram, 0, sizeof s_fake_wram);
  Dkc2CoopResetSession();
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  WriteWord(0x060D, 1);
  const uint16_t animal = 0x0FB8;
  for (uint16_t kong = kDkc2CoopSlotA; kong <= kDkc2CoopSlotB; kong += 0x5E) {
    uint16_t other = kong == kDkc2CoopSlotA ? kDkc2CoopSlotB : kDkc2CoopSlotA;
    WriteWord(0x0593, other);
    WriteWord(0x0595, 0x16B2);
    WriteWord(0x0597, kong);
    WriteWord(0x0599, 0x16D8);
    WriteWord(0x0064, animal);
    WriteWord(0x006A, kong);
    WriteWord(0x006E, 0);
    WriteWord(animal, 0x19C);
    WriteWord(animal + 10, 200);
    WriteWord(kong + 6, 400);
    WriteWord(kong + 10, 180);
    WriteWord(kong + 0x24, 0x100);
    WriteWord(kong + 0x2E, 6);
    WriteWord(kong + 0x30, 0x1E);
    ExpectBool("either follower can land on animal", Dkc2CoopUseBothMountColliders(&cpu), true);
    WriteWord(kong + 10, 200);
    ExpectBool("standing Kong cannot mask rider landing", Dkc2CoopUseBothMountColliders(&cpu), false);
    WriteWord(kong + 10, 180);
    WriteWord(kong + 0x24, 0xFF00);
    ExpectBool("rising Kong does not mask another landing", Dkc2CoopUseBothMountColliders(&cpu), false);
    WriteWord(kong + 0x24, 0x100);
    WriteWord(0x9620 + 6 * 4, 9);
    ExpectBool("cartridge ineligible state is respected", Dkc2CoopUseBothMountColliders(&cpu), false);
    WriteWord(0x9620 + 6 * 4, 0);
    ExpectU16("mount gate uses collider", Dkc2CoopMountCandidateValue(&cpu, other), kong);
    WriteWord(animal, 0x1A8);
    ExpectU16("barrel state lookup retains leader", Dkc2CoopMountCandidateValue(&cpu, other), other);
    WriteWord(animal, 0x19C);
    WriteWord(0x006E, 0x19C);
    ExpectU16("mounted animal state lookup retains rider", Dkc2CoopMountCandidateValue(&cpu, other), other);
    WriteWord(0x006E, 0);
    WriteWord(0x0A82, 0x17);
    WriteWord(0x0A84, animal);
    Dkc2CoopRecordInteractionSource(&cpu, animal);
    WriteWord(0x006A, other); /* a later collision cannot steal the accepted mount */
    Dkc2CoopPrepareAnimalMount(&cpu);
    ExpectU16("rider becomes leader", cpu_read16(&cpu, 0, 0x0593), kong);
    ExpectU16("old leader becomes independent partner", cpu_read16(&cpu, 0, 0x0597), other);
    ExpectU16("rider work selector follows owner", cpu_read16(&cpu, 0, 0x0595), 0x16D8);
    ExpectU16("partner work selector follows owner", cpu_read16(&cpu, 0, 0x0599), 0x16B2);
    ExpectU16("rider controller remains fixed", cpu_read16(&cpu, 0, 0x08A2), kong == kDkc2CoopSlotA ? 1 : 2);
    ExpectU16("mount preparation preserves position", cpu_read16(&cpu, 0, kong + 6), 400);
    ExpectU16("mount preparation preserves collision", cpu_read16(&cpu, 0, kong + 0x30), 0x1E);
    WriteWord(0x0064, other);
    ExpectU16("partner keeps on-foot physics", Dkc2CoopAnimalTypeValue(&cpu, 0x19C), 0);
    WriteWord(0x0064, kong);
    ExpectU16("rider keeps animal physics", Dkc2CoopAnimalTypeValue(&cpu, 0x19C), 0x19C);
    ExpectU16("handoff retains survivor x", Dkc2CoopHandoffXValue(&cpu, 999), 400);
    ExpectU16("handoff retains survivor y", Dkc2CoopHandoffYValue(&cpu, 999), 180);
    WriteWord(0x0064, animal);
    ExpectU16("animal sprite sees mounted type", Dkc2CoopAnimalTypeValue(&cpu, 0x19C), 0x19C);
  }
  ExpectU16("partner not snapped to animal", Dkc2CoopAnimalFollowerFlags(&cpu, 0x4100), 0x100);
  cpu.X = kDkc2CoopSlotB;
  WriteWord(0x0A36, 7);
  WriteWord(0x0A38, 55);
  Dkc2CoopSelectStateWord(&cpu, 0x6F);
  ExpectU16("turn freeze is cleared", cpu_read16(&cpu, 0, 0x0A36), 0);
  ExpectU16("turn freeze timer is cleared", cpu_read16(&cpu, 0, 0x0A38), 0);
  WriteWord(0x0A36, 3);
  Dkc2CoopSelectStateWord(&cpu, 0x6F);
  ExpectU16("unrelated freeze retained", cpu_read16(&cpu, 0, 0x0A36), 3);
  for (uint16_t mode = 0; mode < 3; ++mode) {
    WriteWord(0x060D, mode);
    Dkc2CoopSetMode(mode == 1 ? kDkc2CoopClassic : kDkc2CoopSimultaneous);
    ExpectBool("stock animal collider retained", Dkc2CoopUseBothMountColliders(&cpu), false);
    ExpectU16("stock animal flags retained", Dkc2CoopAnimalFollowerFlags(&cpu, 0x4100), 0x4100);
    ExpectU16("stock handoff position retained", Dkc2CoopHandoffXValue(&cpu, 999), 999);
    ExpectU16("stock mount candidate retained", Dkc2CoopMountCandidateValue(&cpu, 123), 123);
    ExpectU16("stock animal type retained", Dkc2CoopAnimalTypeValue(&cpu, 0x19C), 0x19C);
  }
  ExpectBool("null mounting safe", Dkc2CoopUseBothMountColliders(NULL), false);
  ExpectU16("null animal type safe", Dkc2CoopAnimalTypeValue(NULL, 3), 3);
  Dkc2CoopPrepareAnimalMount(NULL);
}

static void TestWideMovementBounds(void) {
  CpuState cpu;
  memset(&cpu, 0, sizeof cpu);
  memset(s_fake_wram, 0, sizeof s_fake_wram);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  WriteWord(0x060D, 1);
  WriteWord(0x17BA, 1000);
  WriteWord(0x0AFC, 6000);
  for (uint16_t kong = kDkc2CoopSlotA; kong <= kDkc2CoopSlotB; kong += 0x5E) {
    WriteWord(0x0064, kong);
    for (int aspect = kDkc2VideoAspectNative; aspect < kDkc2VideoAspectCount; ++aspect) {
      Dkc2VideoSetAspect((Dkc2VideoAspect)aspect);
      Dkc2VideoSetTerrainReady(true);
      Dkc2VideoSetPresentationBias(0);
      int extra = aspect == kDkc2VideoAspectNative ? 0 : aspect == kDkc2VideoAspect16x10 ? 26 : aspect == kDkc2VideoAspect16x9 ? 43 : 95;
      ExpectU16("wide movement left follows aspect", Dkc2CoopScreenLeftValue(&cpu, 16), (uint16_t)(16-extra));
      ExpectU16("wide movement span follows aspect", Dkc2CoopScreenSpanValue(&cpu, 224), (uint16_t)(224+2*extra));
    }
  }
  Dkc2VideoSetAspect(kDkc2VideoAspect16x9);
  Dkc2VideoSetTerrainReady(true);
  WriteWord(0x17BA, 0x100);
  Dkc2VideoSetPresentationBias(43);
  ExpectU16("west wall remains bounded", Dkc2CoopScreenLeftValue(&cpu, 16), 16);
  ExpectU16("west shift opens east margin", Dkc2CoopScreenSpanValue(&cpu, 224), 310);
  WriteWord(0x17BA, 6000);
  Dkc2VideoSetPresentationBias(-43);
  ExpectU16("east shift opens west margin", Dkc2CoopScreenLeftValue(&cpu, 16), (uint16_t)-70);
  ExpectU16("east wall remains bounded", (uint16_t)(Dkc2CoopScreenLeftValue(&cpu, 16) +
             Dkc2CoopScreenSpanValue(&cpu, 224)), 240);
  Dkc2VideoSetPresentationBias(0);
  ExpectU16("reflected art cannot extend east level limit",
            (uint16_t)(Dkc2CoopScreenLeftValue(&cpu, 16) + Dkc2CoopScreenSpanValue(&cpu, 224)), 240);
  WriteWord(0x17BA, 0x100);
  WriteWord(0x0AFC, 0x100);
  ExpectU16("single-screen room stays bounded", Dkc2CoopScreenSpanValue(&cpu, 224), 224);
  WriteWord(0x17BA, 1000);
  WriteWord(0x0AFC, 6000);
  Dkc2VideoSetTerrainReady(false);
  ExpectU16("unproven margins are not playable", Dkc2CoopScreenSpanValue(&cpu, 224), 224);
  Dkc2VideoSetTerrainReady(true);
  WriteWord(0x0064, 0x0FB8);
  ExpectU16("non-Kong keeps native limits", Dkc2CoopScreenSpanValue(&cpu, 224), 224);
  WriteWord(0x0064, kDkc2CoopSlotB);
  WriteWord(0x060D, 0);
  ExpectU16("solo keeps native limits", Dkc2CoopScreenSpanValue(&cpu, 224), 224);
  WriteWord(0x060D, 2);
  ExpectU16("contest keeps native limits", Dkc2CoopScreenSpanValue(&cpu, 224), 224);
  WriteWord(0x060D, 1);
  Dkc2CoopSetMode(kDkc2CoopClassic);
  ExpectU16("classic TEAM keeps native limits", Dkc2CoopScreenSpanValue(&cpu, 224), 224);
  ExpectU16("null width query safe", Dkc2CoopScreenSpanValue(NULL, 224), 224);
  Dkc2VideoSetAspect(kDkc2VideoAspectNative);
}

static void TestBananaCollectionPass(void) {
  CpuState cpu = {0};
  memset(s_fake_wram, 0, sizeof s_fake_wram);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  Dkc2CoopResetSession();
  WriteWord(0x060D, 1);
  WriteWord(0x09B7, 100);  /* leader hitbox present */
  WriteWord(0x09EF, 110);  /* animal hitbox present */
  WriteWord(0x09CB, 300);  /* distant follower rectangle */
  WriteWord(0x09CD, 400);
  WriteWord(0x09CF, 320);
  WriteWord(0x09D1, 435);
  const uint16_t original[] = {90, 200, 20, 30};
  const uint16_t follower[] = {300, 400, 20, 35};
  for (unsigned player = 0; player < 2; ++player) {
    uint16_t slot = player ? kDkc2CoopSlotB : kDkc2CoopSlotA;
    WriteWord(0x0597, slot);
    WriteWord((uint16_t)(slot + 0x30), 30);
    for (unsigned i = 0; i < 4; ++i) WriteWord((uint16_t)(0x0D3C + i * 4), original[i]);
    ExpectU16("include on-foot banana collider", Dkc2CoopBananaSecondWidth(&cpu, 20), 20);
    for (unsigned i = 0; i < 4; ++i)
      ExpectU16("follower gets separate bounds", cpu_read16(&cpu, 0, (uint16_t)(0x0D3C + i * 4)), follower[i]);
    ExpectBool("animal still receives a collection pass", Dkc2CoopBananaNextPass(&cpu), 1);
    for (unsigned i = 0; i < 4; ++i)
      ExpectU16("original scratch bounds restored", cpu_read16(&cpu, 0, (uint16_t)(0x0D3C + i * 4)), original[i]);
    ExpectBool("repeat only once per group", Dkc2CoopBananaNextPass(&cpu), 0);
    WriteWord((uint16_t)(slot + 0x30), 0);
    Dkc2CoopBananaSecondWidth(&cpu, 20);
    ExpectBool("absent follower cannot collect", Dkc2CoopBananaNextPass(&cpu), 0);
    WriteWord((uint16_t)(slot + 0x30), 30);
  }
  const uint16_t required[] = {0x09B7, 0x09EF, 0x09CF};
  for (unsigned i = 0; i < 3; ++i) {
    uint16_t value = cpu_read16(&cpu, 0, required[i]);
    WriteWord(required[i], 0);
    Dkc2CoopBananaSecondWidth(&cpu, 20);
    ExpectBool("two or fewer colliders use stock path", Dkc2CoopBananaNextPass(&cpu), 0);
    WriteWord(required[i], value);
  }
  for (unsigned mode = 0; mode < 3; mode += 2) {
    WriteWord(0x060D, (uint16_t)mode);
    Dkc2CoopBananaSecondWidth(&cpu, 20);
    ExpectBool("solo/contest unchanged", Dkc2CoopBananaNextPass(&cpu), 0);
  }
  WriteWord(0x060D, 1);
  Dkc2CoopSetMode(kDkc2CoopClassic);
  Dkc2CoopBananaSecondWidth(&cpu, 20);
  ExpectBool("classic unchanged", Dkc2CoopBananaNextPass(&cpu), 0);
  ExpectU16("null banana query", Dkc2CoopBananaSecondWidth(NULL, 20), 20);
  ExpectBool("null banana continuation", Dkc2CoopBananaNextPass(NULL), 0);
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  Dkc2CoopLoadLostMask(&cpu, 2, true);
  Dkc2CoopBananaSecondWidth(&cpu, 20);
  ExpectBool("saved lost follower excluded", Dkc2CoopBananaNextPass(&cpu), 0);
  Dkc2CoopResetSession();
}

static void TestTeamCarry(void) {
  CpuState cpu = {0};
  memset(s_fake_wram, 0, sizeof(s_fake_wram));
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  Dkc2CoopResetSession();
  WriteWord(0x060D, 1);
  WriteWord(0x08C2, 0x4000);
  WriteWord(0x0593, kDkc2CoopSlotA);
  WriteWord(0x0595, 0x100);
  WriteWord(0x0597, kDkc2CoopSlotB);
  WriteWord(0x0599, 0x200);
  WriteWord(0x0064, kDkc2CoopSlotB);
  WriteWord(kDkc2CoopSlotA + 6, 100);
  WriteWord(kDkc2CoopSlotB + 6, 125);
  cpu.Y = kDkc2CoopSlotA;
  ExpectU16("P2 targets P1", Dkc2CoopTeamPartnerValue(&cpu, kDkc2CoopSlotB), kDkc2CoopSlotA);
  ExpectU16("distant pickup rejected", Dkc2CoopTeamStateValue(&cpu, 0), 0xFFFF);
  ExpectU16("rejected pickup leaves leader", cpu_read16(&cpu, 0, 0x0593), kDkc2CoopSlotA);
  WriteWord(kDkc2CoopSlotB + 6, 124);
  WriteWord(kDkc2CoopSlotB + 10, 17);
  ExpectU16("vertical separation rejected", Dkc2CoopTeamStateValue(&cpu, 0), 0xFFFF);
  WriteWord(kDkc2CoopSlotB + 10, 16);
  ExpectU16("airborne partner rejected", Dkc2CoopTeamStateValue(&cpu, 6), 0xFFFF);
  WriteWord(0x0D7A, 0x0E9E);
  ExpectU16("occupied hands rejected", Dkc2CoopTeamStateValue(&cpu, 0), 0xFFFF);
  WriteWord(0x0D7A, 0);
  WriteWord(0x006E, 1);
  ExpectU16("mounted pair rejected", Dkc2CoopTeamStateValue(&cpu, 0), 0xFFFF);
  WriteWord(0x006E, 0);
  Dkc2CoopLoadLostMask(&cpu, 1, true);
  ExpectU16("lost partner rejected", Dkc2CoopTeamStateValue(&cpu, 0x13), 0xFFFF);
  Dkc2CoopResetSession();
  ExpectU16("nearby independent pickup accepted", Dkc2CoopTeamStateValue(&cpu, 0), 0x22);
  ExpectU16("P2 carrier becomes leader", cpu_read16(&cpu, 0, 0x0593), kDkc2CoopSlotB);
  ExpectU16("carrier work swapped", cpu_read16(&cpu, 0, 0x0595), 0x200);
  ExpectU16("passenger work swapped", cpu_read16(&cpu, 0, 0x0599), 0x100);
  ExpectU16("carrier controller unchanged", cpu_read16(&cpu, 0, 0x08A2), 2);
  ExpectU16("pickup does not teleport", cpu_read16(&cpu, 0, kDkc2CoopSlotA + 6), 100);
  WriteWord(0x0D7A, kDkc2CoopSlotA);
  WriteWord(kDkc2CoopSlotB + 0x2E, 0x13);
  WriteWord(0x0504, 0x0100);
  cpu.X = kDkc2CoopSlotB;
  ExpectU16("movement preserves pickup animation", Dkc2CoopSelectStateWord(&cpu, 0x13), 0x13);
  ExpectU16("carrier owns passenger", Dkc2CoopHeldOwnerValue(&cpu, 0), kDkc2CoopSlotB);
  Dkc2CoopResetSession();
  ExpectU16("carry survives host reset", Dkc2CoopHeldOwnerValue(&cpu, 0), kDkc2CoopSlotB);
  WriteWord(0x0064, kDkc2CoopSlotA);
  ExpectU16("passenger does not own itself", Dkc2CoopHeldObjectValue(&cpu, kDkc2CoopSlotA), 0);
  WriteWord(0x0D7A, 0);
  cpu.X = kDkc2CoopSlotA;
  WriteWord(kDkc2CoopSlotA + 0x30, 0x16);
  ExpectU16("airborne thrown player keeps physics", Dkc2CoopSelectStateWord(&cpu, 0x21), 0x21);
  WriteWord(kDkc2CoopSlotA + 0x1E, 1);
  WriteWord(kDkc2CoopSlotA + 0x24, 0xFA00);
  ExpectU16("bouncing thrown player keeps physics", Dkc2CoopSelectStateWord(&cpu, 0x21), 0x21);
  WriteWord(kDkc2CoopSlotA + 0x24, 0);
  ExpectU16("landed thrown player regains control", Dkc2CoopSelectStateWord(&cpu, 0x21), 0);
  ExpectU16("landed thrown player regains collisions", cpu_read16(&cpu, 0, kDkc2CoopSlotA + 0x30), 0x1E);
  WriteWord(kDkc2CoopSlotB + 0x2E, 0);
  cpu.Y = kDkc2CoopSlotB;
  ExpectU16("P1 can carry in reverse", Dkc2CoopTeamStateValue(&cpu, 0), 0x22);
  ExpectU16("P1 carrier becomes leader", cpu_read16(&cpu, 0, 0x0593), kDkc2CoopSlotA);
  for (uint16_t mode = 0; mode <= 2; mode += 2) {
    WriteWord(0x060D, mode);
    ExpectU16("solo/contest partner unchanged", Dkc2CoopTeamPartnerValue(&cpu, 123), 123);
    ExpectU16("solo/contest eligibility unchanged", Dkc2CoopTeamStateValue(&cpu, 0x2A), 0x2A);
  }
  WriteWord(0x060D, 1);
  Dkc2CoopSetMode(kDkc2CoopClassic);
  ExpectU16("classic eligibility unchanged", Dkc2CoopTeamStateValue(&cpu, 0), 0);
  ExpectU16("null eligibility unchanged", Dkc2CoopTeamStateValue(NULL, 0), 0);
}

static void TestRopeTransitionReload(void) {
  CpuState cpu = {0};
  for (uint16_t slot = kDkc2CoopSlotA; slot <= kDkc2CoopSlotB; slot += 0x5E) {
    for (uint16_t direction = 0; direction <= 1; ++direction) {
      memset(s_fake_wram, 0, sizeof(s_fake_wram));
      Dkc2CoopSetMode(kDkc2CoopSimultaneous);
      cpu.X = slot;
      WriteWord(0x060D, 1);
      WriteWord(slot + 0x2E, 0x36);
      WriteWord(slot + 0x36, (uint16_t)((slot == kDkc2CoopSlotA ? 0x34 : 0xD7) + direction));
      WriteWord(slot + 0x3C, 0x8003);
      WriteWord(slot + 0x3E, 0xDD63);
      /* Synthetic script: native completion followed by terminal wait. */
      WriteWord(0x8000, 0x0081);
      WriteWord(0x8001, direction ? 0xDD90 : 0xDD7E);
      WriteWord(0x8003, 0x0083);
      WriteWord(0x8004, 0xD12B);
      WriteWord(slot + 0x38, 0x100);
      Dkc2CoopSelectStateWord(&cpu, 0x36);
      ExpectU16("unfinished junction is not rewound", cpu_read16(&cpu, 0, slot + 0x3C), 0x8003);
      WriteWord(slot + 0x38, 0);
      WriteWord(0x8004, 0xD100);
      Dkc2CoopSelectStateWord(&cpu, 0x36);
      ExpectU16("foreign wait is not rewound", cpu_read16(&cpu, 0, slot + 0x3C), 0x8003);
      WriteWord(0x8004, 0xD12B);
      for (uint16_t mode = 0; mode <= 2; mode += 2) {
        WriteWord(0x060D, mode);
        Dkc2CoopSelectStateWord(&cpu, 0x36);
        ExpectU16("solo/contest junction is untouched", cpu_read16(&cpu, 0, slot + 0x3C), 0x8003);
      }
      WriteWord(0x060D, 1);
      Dkc2CoopSetMode(kDkc2CoopClassic);
      Dkc2CoopSelectStateWord(&cpu, 0x36);
      ExpectU16("classic junction is untouched", cpu_read16(&cpu, 0, slot + 0x3C), 0x8003);
      Dkc2CoopSetMode(kDkc2CoopSimultaneous);
      Dkc2CoopResetSession();
      ExpectU16("saved junction keeps native transition state", Dkc2CoopSelectStateWord(&cpu, 0x36), 0x36);
      ExpectU16("saved junction replays completion", cpu_read16(&cpu, 0, slot + 0x3C), 0x8000);
      Dkc2CoopSelectStateWord(&cpu, 0x36);
      ExpectU16("completion rewinds once", cpu_read16(&cpu, 0, slot + 0x3C), 0x8000);
    }
  }
}

static void TestRopeOwnership(void) {
  CpuState cpu = {0};
  memset(s_fake_wram, 0, sizeof(s_fake_wram));
  Dkc2CoopSetMode(kDkc2CoopSimultaneous);
  WriteWord(0x060D, 1);
  for (uint16_t follower = kDkc2CoopSlotA; follower <= kDkc2CoopSlotB; follower += 0x5E) {
    uint16_t leader = follower == kDkc2CoopSlotA ? kDkc2CoopSlotB : kDkc2CoopSlotA;
    WriteWord(0x0593, leader);
    WriteWord(0x0597, follower);
    WriteWord(0x0064, leader); /* Last-updated sprite need not be the grabber. */
    WriteWord(0x0A84, follower);
    cpu.X = follower;
    for (uint16_t state = 0x35; state <= 0x38; ++state) {
      WriteWord(follower + 0x2E, state);
      ExpectU16("independent climber advances animation and junction", Dkc2CoopRopeAnimationFollowerValue(&cpu, follower), 0xFFFF);
    }
    WriteWord(follower + 0x2E, 0x22);
    ExpectU16("other animation states unchanged", Dkc2CoopRopeAnimationFollowerValue(&cpu, follower), follower);
    ExpectBool("rope grab belongs to contact source", Dkc2CoopRopeUsesFollower(&cpu), 1);
    Dkc2CoopResetSession();
    ExpectBool("queued rope grab survives snapshot reset", Dkc2CoopRopeUsesFollower(&cpu), 1);
    ExpectU16("rope grab keeps camera leader", cpu_read16(&cpu, 0, 0x0593), leader);
    WriteWord(0x0A84, leader);
    ExpectBool("leader rope grab keeps native work", Dkc2CoopRopeUsesFollower(&cpu), 0);
  }
  WriteWord(0x0597, 0x0E9E);
  cpu.X = 0x0E9E;
  WriteWord(cpu.X + 0x2E, 0x35);
  ExpectU16("non-Kong animation unchanged", Dkc2CoopRopeAnimationFollowerValue(&cpu, cpu.X), cpu.X);
  WriteWord(0x0A84, 0x0E9E);
  ExpectBool("non-Kong rope source rejected", Dkc2CoopRopeUsesFollower(&cpu), 0);
  WriteWord(0x0597, kDkc2CoopSlotB);
  WriteWord(0x0A84, kDkc2CoopSlotB);
  cpu.X = kDkc2CoopSlotB;
  WriteWord(cpu.X + 0x2E, 0x35);
  for (uint16_t mode = 0; mode <= 2; mode += 2) {
    WriteWord(0x060D, mode);
    ExpectBool("solo/contest rope work unchanged", Dkc2CoopRopeUsesFollower(&cpu), 0);
    ExpectU16("solo/contest rope animation unchanged", Dkc2CoopRopeAnimationFollowerValue(&cpu, cpu.X), cpu.X);
  }
  WriteWord(0x060D, 1);
  Dkc2CoopSetMode(kDkc2CoopClassic);
  ExpectBool("classic rope work unchanged", Dkc2CoopRopeUsesFollower(&cpu), 0);
  ExpectU16("classic rope animation unchanged", Dkc2CoopRopeAnimationFollowerValue(&cpu, cpu.X), cpu.X);
  ExpectU16("null rope animation unchanged", Dkc2CoopRopeAnimationFollowerValue(NULL, 123), 123);
  ExpectBool("null rope work unchanged", Dkc2CoopRopeUsesFollower(NULL), 0);
}

int main(void) {
  TestModeAccessors();
  TestModeFromName();
  TestSimultaneousActive();
  TestGateActive();
  TestSelectWords();
  TestFollowerNeedsControl();
  TestWrappersAgainstFakeWram();
  TestStompOwnership();
  TestRecovery();
  TestHeldObjectOwnership();
  TestHurtLifecycle();
  TestAnimalsAndHandoff();
  TestWideMovementBounds();
  TestBananaCollectionPass();
  TestTeamCarry();
  TestRopeOwnership();
  TestRopeTransitionReload();
  if (s_failures > 0) {
    fprintf(stderr, "%d co-op test(s) failed\n", s_failures);
    return EXIT_FAILURE;
  }
  puts("co-op policy tests passed");
  return EXIT_SUCCESS;
}
