"""Contract test: the co-op generation adapter must adapt REAL v2 emitter
output, not only hand-written fixtures.

The v2 emitter is driven on a synthetic ROM whose function mirrors the
Kong action input gate shape (LDX dp / CPX abs / BEQ / CLC / RTS /
LDA abs / STA abs / LDA abs / STA abs / RTS). The emitted C is then run
through scripts/apply_dkc2_coop_overrides.py and the three adaptations
must land exactly once each. This pins the adapter to the emitter's real
C idioms (single read temps, cpu->DB absolute banks, flag-mirror
branches) so a generator change fails this source-only test instead of
the owner's private regeneration. The adapter anchors on
instruction-shape context rather than trace PCs, so no address remapping
is needed here.
"""

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[1]
SCRIPT = REPOSITORY / "scripts" / "apply_dkc2_coop_overrides.py"
SPEC = importlib.util.spec_from_file_location(
    "apply_dkc2_coop_overrides", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)

sys.path.insert(0, str(REPOSITORY / "snesrecomp" / "recompiler"))
try:
    from v2.emit_function import emit_function
except ImportError:  # pragma: no cover - submodule not checked out
    emit_function = None


def gate_rom():
    """Gate-shaped synthetic function at LoROM bank-0 $8000, M0X0."""
    rom = bytearray(0x8000)
    code = bytes([
        0xA6, 0x64,             # $8000 LDX $64        (current_sprite)
        0xEC, 0x93, 0x05,       # $8002 CPX $0593      (active Kong gate)
        0xF0, 0x04,             # $8005 BEQ $800B      (input path)
        0x18,                   # $8007 CLC            (no input)
        0x60,                   # $8008 RTS
        0xEA, 0xEA,             # $8009 NOP padding
        0xAD, 0x10, 0x05,       # $800B LDA $0510      (active pressed)
        0x8D, 0x83, 0x09,       # $800E STA $0983
        0xAD, 0x0E, 0x05,       # $8011 LDA $050E      (active held)
        0x8D, 0x81, 0x09,       # $8014 STA $0981
        0x60,                   # $8017 RTS
    ])
    rom[0:len(code)] = code
    return bytes(rom)


@unittest.skipIf(emit_function is None, "snesrecomp v2 emitter unavailable")
class CoopEmitterContractTests(unittest.TestCase):
    def test_adapter_applies_to_real_emitted_c(self):
        src = emit_function(gate_rom(), bank=0, start=0x8000,
                            entry_m=0, entry_x=0)
        # Sanity: the synthetic function really emits the gate idioms the
        # adapter anchors on before any adaptation is attempted.
        self.assertIn("uint16 _v1 = cpu_read16(cpu, 0x00, "
                      "(uint16)(cpu->D + 0x0064));", src)
        self.assertEqual(src.count("cpu_read16(cpu, cpu->DB, "
                                   "(uint16)(0x0593))"), 1)
        self.assertEqual(src.count("cpu_read16(cpu, cpu->DB, "
                                   "(uint16)(0x0510))"), 1)
        self.assertEqual(src.count("cpu_read16(cpu, cpu->DB, "
                                   "(uint16)(0x050e))"), 1)

        # Real generated units carry the funcs.h include the adapter
        # anchors its own include behind; emit_function returns the body
        # only, so provide the unit header here.
        state_rom = bytearray(0x8000)
        state_rom[:3] = bytes([0xB5, 0x2E, 0x60])  # LDA $2E,x / RTS
        state_src = emit_function(bytes(state_rom), bank=0, start=0x8000,
                                  entry_m=0, entry_x=0,
                                  func_name="kong_state_handler")
        return_rom = bytes([0x6B]) + bytes(0x7fff)
        clipping_src = "".join(emit_function(return_rom, bank=0, start=0x8000,
                               entry_m=0, entry_x=0, func_name=name)
                               for name in ("CODE_BCFB2C", "get_active_kong_clipping"))
        interaction_rom = bytearray(0x8000)
        interaction_rom[:6] = bytes([0xA5, 0x64, 0x8D, 0x84, 0x0A, 0x60])
        interaction_src = emit_function(bytes(interaction_rom), bank=0, start=0x8000,
                                        entry_m=0, entry_x=0,
                                        func_name="set_player_interaction")
        bounce_rom = bytearray(0x8000)
        bounce_rom[:4] = bytes([0x20, 0x92, 0x80, 0x60])
        bounce_rom[0x92] = 0x60
        bounce_src = emit_function(bytes(bounce_rom), bank=0, start=0x8000,
                                   entry_m=0, entry_x=0,
                                   func_name="player_interaction_1B")
        # Give the synthetic callee the same symbol alias as the game config.
        import re
        bounce_src, count = re.subn(r"\b\w+_M0X0\(cpu\)",
                                    "work_on_active_kong_M0X0(cpu)", bounce_src)
        self.assertEqual(count, 1)
        rope_src = ''.join(bounce_src.replace('player_interaction_1B', name)
                           for name in MODULE.ROPE_FUNCTIONS)
        palette_rom = bytes([0xA9, 0x1E, 0x00, 0x60]) + bytes(0x7FFC)
        palette_src = emit_function(palette_rom, bank=0, start=0x8000,
                                    entry_m=0, entry_x=0, func_name="CODE_BB8B66")
        recovery_rom = bytes([0xA6, 0x64, 0xEC, 0x97, 0x05,
                              0xAD, 0x7A, 0x0D, 0xCD, 0x97, 0x05, 0x60]) + bytes(0x7FF4)
        recovery_src = emit_function(recovery_rom, bank=0, start=0x8000,
                                     entry_m=0, entry_x=0, func_name="CODE_B9D705")
        pickup_rom = bytes([0x8E, 0x7A, 0x0D, 0x60]) + bytes(0x7FFC)
        pickup_src = emit_function(pickup_rom, bank=0, start=0x8000,
                                   entry_m=0, entry_x=0, func_name="update_object_pickup")
        owner_src = "".join(emit_function(bytes([0xAE, 0x93, 0x05]) * count +
                                        bytes([0x60]) + bytes(0x7FFF - 3 * count),
                                        bank=0, start=0x8000,
                                        entry_m=0, entry_x=0, func_name=name)
                             for name, count in MODULE.OWNER_FUNCTIONS.items())
        animal_src = emit_function(return_rom, bank=0, start=0x8000,
                                   entry_m=0, entry_x=0, func_name="player_interaction_17")
        animal_ops = {}
        for name, _, count, helper in MODULE.POLICY_READS:
            address = {"Dkc2CoopBananaSecondWidth": [0x44, 0x0D],
                       "Dkc2CoopAnimalFollowerFlags": [0xC2, 0x08],
                       "Dkc2CoopHandoffXValue": [0x66, 0x0D],
                       "Dkc2CoopHandoffYValue": [0x6A, 0x0D]}.get(helper, [0x93, 0x05])
            op = [0xA5, 0x6E] if helper == "Dkc2CoopAnimalTypeValue" else [0xAD] + address
            op = {"Dkc2CoopScreenLeftValue": [0xA9, 0x10, 0],
                  "Dkc2CoopScreenSpanValue": [0xA9, 0xE0, 0],
                  "Dkc2CoopTeamPartnerValue": [0xAC, 0x97, 0x05],
                  "Dkc2CoopRopeAnimationFollowerValue": [0xAD, 0x97, 0x05],
                  "Dkc2CoopTeamStateValue": [0xB9, 0x2E, 0]}.get(helper, op)
            animal_ops.setdefault(name, []).extend(op * count)
        for name, ops in animal_ops.items():
            if name == "CODE_B9D705":
                recovery_rom = bytes(ops) + recovery_rom[:12] + bytes(0x8000-12-len(ops))
                recovery_src = emit_function(recovery_rom, bank=0, start=0x8000,
                                            entry_m=0, entry_x=0, func_name=name)
            elif name == "player_interaction_1B":
                synthetic = bytearray(0x8000)
                synthetic[:len(ops)+4] = bytes(ops + [0x20, 0x92, 0x80, 0x60])
                synthetic[0x92] = 0x60
                bounce_src = emit_function(bytes(synthetic), bank=0, start=0x8000,
                                          entry_m=0, entry_x=0, func_name=name)
                bounce_src = re.sub(r"\b\w+_M0X0\(cpu\)", "work_on_active_kong_M0X0(cpu)", bounce_src)
            elif name == "bank_B5_F776":
                # Synthetic group flow with the real continuation labels.
                synthetic = bytearray(0x200000)
                base = 0x1AF776
                synthetic[base:base+6] = bytes(ops + [0x4C, 0x89, 0xF7])
                synthetic[0x1AF789:0x1AF78C] = bytes([0x4C, 0x94, 0xF7])
                synthetic[0x1AF794] = 0x60
                animal_src += emit_function(bytes(synthetic), bank=0xB5, start=0xF776,
                                            entry_m=0, entry_x=0, func_name=name)
            elif name == "kong_state_27":
                # Real emitter labels for the two source-owned handoff anchors.
                synthetic = bytearray(0x200000)
                synthetic[0x1C216F:0x1C2172] = bytes([0x4C, 0xAB, 0xA1])
                synthetic[0x1C21AB:0x1C21AB+len(ops)+1] = bytes(ops + [0x60])
                animal_src += emit_function(bytes(synthetic), bank=0xB8, start=0xA16F,
                                            entry_m=0, entry_x=0, func_name=name)
            else:
                if name == "handle_animal_mounting":
                    ops += [0x20, 0x92, 0x80]
                code = bytearray(bytes(ops + [0x60]) + bytes(0x7FFF-len(ops)))
                code[0x92] = 0x60
                src_animal = emit_function(bytes(code), bank=0, start=0x8000,
                                           entry_m=0, entry_x=0, func_name=name)
                if name == "handle_animal_mounting":
                    src_animal = re.sub(r"\b\w+_M0X0\(cpu\)", "check_active_kong_collision_M0X0(cpu)", src_animal)
                animal_src += src_animal
        unit_text = ('#include "funcs.h"\n' + src + state_src + clipping_src +
                     interaction_src + bounce_src + palette_src + recovery_src +
                     pickup_src + owner_src + animal_src + rope_src)
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            unit = directory / "bank_b8.c"
            unit.write_text(unit_text, encoding="utf-8")
            changed = MODULE.apply_overrides(directory)
            self.assertEqual(changed, [unit])
            adapted = unit.read_text(encoding="utf-8")

            # A second pass must be a no-op (idempotent regeneration).
            again = MODULE.apply_overrides(directory)
            self.assertEqual(again, [unit])
            self.assertEqual(unit.read_text(encoding="utf-8"), adapted)

        self.assertEqual(adapted.count('#include "dkc2_coop.h"'), 1)
        self.assertEqual(adapted.count("Dkc2CoopSelectStateWord(cpu, "), 1)
        self.assertEqual(adapted.count("Dkc2CoopUseFollowerClipping(cpu)"), 1)
        self.assertEqual(adapted.count("Dkc2CoopBananaSecondWidth(cpu, "), 1)
        self.assertIn(MODULE.BANANA_TRACE + MODULE.BANANA_REPEAT, adapted)
        self.assertEqual(adapted.count("Dkc2CoopRecordInteractionSource(cpu, "), 1)
        self.assertEqual(adapted.count("Dkc2CoopBounceUsesFollower(cpu)"), 1)
        self.assertEqual(adapted.count("Dkc2CoopRopeUsesFollower(cpu)"), 2)
        self.assertEqual(adapted.count("Dkc2CoopFollowerPaletteOffset(cpu, 0x1e)"), 1)
        self.assertEqual(adapted.count("Dkc2CoopRecoveryFollowerValue(cpu, "), 1)
        self.assertEqual(adapted.count("Dkc2CoopRecordPickupValue(cpu, "), 1)
        self.assertEqual(adapted.count("Dkc2CoopHeldObjectValue(cpu, "), 1)
        self.assertEqual(adapted.count("Dkc2CoopHeldOwnerValue(cpu, "), 10)
        self.assertEqual(
            adapted.count("Dkc2CoopGateActiveValue(cpu, cpu_read16("
                          "cpu, cpu->DB, (uint16)(0x0593)))"),
            1)
        self.assertEqual(
            adapted.count("Dkc2CoopSelectPressedWord(cpu, cpu_read16("
                          "cpu, cpu->DB, (uint16)(0x0510)))"),
            1)
        self.assertEqual(
            adapted.count("Dkc2CoopSelectHeldWord(cpu, cpu_read16("
                          "cpu, cpu->DB, (uint16)(0x050e)))"),
            1)
        # The CLC fall-through path stays stock.
        self.assertIn("goto L_8007_M0X0; /* fall-through */", adapted)


if __name__ == "__main__":
    unittest.main()
