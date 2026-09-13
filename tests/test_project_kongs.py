#!/usr/bin/env python3
"""Synthetic tests; no reference checkout or game data required."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "import_kongs", Path(__file__).resolve().parents[1] / "scripts/import_project_kongs.py")
kongs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kongs)


class ProjectKongsTests(unittest.TestCase):
    def test_small_tile_and_transparency(self):
        tile = bytearray(32)
        tile[0] = 0x80
        data = bytes([0, 1, 0, 0, 0, 1, 0, 0, 124, 120]) + tile
        x, y, w, h, pixels = kongs.decode_sprite(data)
        self.assertEqual((x, y, w, h), (-4, -8, 8, 8))
        self.assertEqual(pixels, bytes([1]) + bytes(63))

    def test_split_dma_large_piece(self):
        data = bytes([1, 0, 0, 0, 0, 2, 16, 2, 120, 112])
        tiles = bytearray(128)
        tiles[64 + 1] = 0x80
        _, _, w, _, pixels = kongs.decode_sprite(data + tiles)
        self.assertEqual(pixels[8 * w], 2)
        self.assertEqual(sum(pixels), 2)

    def test_truncated_and_unmapped_tile_rejected(self):
        for data in [b"", bytes(8), bytes([0, 1, 8, 0, 0, 1, 0, 0, 128, 128]) + bytes(32)]:
            with self.assertRaises(ValueError):
                kongs.decode_sprite(data)

    def test_visual_subroutine_and_loop(self):
        program = (["db !animation_command_91, $4E : dw sub",
                    "db !animation_command_82 : dw main",
                    "db $03 : dw $1234", "db !animation_command_92, $4E"],
                   {"main": 0, "sub": 2})
        self.assertEqual(kongs.project_sequence(program, "main", {0x1234: None}),
                         ([(0x1234, 3)], 0))

    def test_carry_and_compound_rider_select_correct_graphic(self):
        program = (["db !animation_command_8B, $02 : dw $1234, $0000, $0000",
                    "db !animation_command_86, $03 : dw $1111, $5678, $0000, $0000",
                    "db !animation_command_80, $00"], {"carry": 0})
        frames = {0x1234: None, 0x5678: None}
        self.assertEqual(kongs.project_sequence(program, "carry", frames),
                         ([(0x1234, 2), (0x5678, 3)], 0))

    def test_mount_branches_keep_animal_keys_and_offsets(self):
        program = (["db !animation_command_8F : dw callback, air",
                    "db !animation_command_86, $03 : dw $1110, $5678, $FFFE, $0004",
                    "db !animation_command_80, $00",
                    "db !animation_command_85, $02 : dw $1114, $567C",
                    "db !animation_command_82 : dw air"], {"mount": 0, "air": 3})
        self.assertEqual(kongs.compound_poses(program, "mount", {0x5678: None, 0x567c: None}),
                         [(0x1110, 0x5678, -2, 4, 1, 3), (0x1114, 0x567c, 0, 0, 0, 2)])

    def test_idle_does_not_fall_through_into_moving_cycle(self):
        program = (["db $10 : dw $1234", "db $03 : dw $1238",
                    "db !animation_command_80, $00"], {"idle": 0, "move": 1})
        self.assertEqual(kongs.project_sequence(program, "idle", {0x1234: None, 0x1238: None}, 1),
                         ([(0x1234, 16)], 0))

    def test_mounted_offsets_are_signed_and_bounded_by_count(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "offsets.asm"
            p.write_text("donkey_animal_offset_a:\n" + "dw $FFFE, $0004\n" * 5 +
                         "kiddy_animal_offset_a:\n" + "dw $FFF8, $FFFD\n" * 5)
            offsets = kongs.mounted_offsets(p)
            self.assertEqual(offsets["donkey"][0], (-2, 4))
            self.assertEqual(offsets["kiddy"][0], (-8, -3))
            self.assertEqual(offsets["kiddy"][1], (-8, 4))
            p.write_text("donkey_animal_offset_a:\ndw $0000, $0000\n")
            with self.assertRaises(ValueError):
                kongs.mounted_offsets(p)

    def test_carry_offsets_keep_hand_position(self):
        program = (["db !animation_command_8B, $02 : dw $1234, $FFF9, $FFCA",
                    "db !animation_command_8B, $02 : dw $1238, $001F, $0003"], {})
        self.assertEqual(kongs.carry_offsets(program, {0x1234, 0x1238}),
                         [(0x1234, -7, -54), (0x1238, 31, 3)])
        self.assertEqual(kongs.carry_offsets(program, {0x1238}), [(0x1238, 31, 3)])

    def test_handoff_separates_both_characters_and_stops_at_transfer(self):
        program = (["db !animation_command_8A, $03 : dw $1234, $5678, $001C, $0000",
                    "db !animation_command_81 : dw transfer",
                    "db !animation_command_8A, $02 : dw $9999, $9998, $0000, $0000"],
                   {"tag": 0})
        self.assertEqual(kongs.handoff_sequence(program, "tag", {0x1234: None}, 0, "transfer"),
                         [(0x1234, 3)])
        self.assertEqual(kongs.handoff_sequence(program, "tag", {0x5678: None}, 1, "transfer"),
                         [(0x5678, 3)])
        with self.assertRaises(ValueError):
            kongs.handoff_sequence(program, "tag", {}, 0, "transfer")
        with self.assertRaises(ValueError):
            kongs.handoff_sequence(program, "tag", {0x1234: None}, 0, "absent")

    def test_team_recovery_sits_once_without_replaying_hurt_or_crying(self):
        program = (["db $02 : dw $3F68", "db $02 : dw $3F7C",
                    "db $10 : dw $3F40", "db $04 : dw $3F44",
                    "db $04 : dw $3F48", "db $04 : dw $3EE0",
                    "db $06 : dw $3F4C", "db !animation_command_80, $00"],
                   {"kiddy_death": 0})
        frames = dict.fromkeys([0x3f68, 0x3f7c, 0x3f40, 0x3f44, 0x3f48, 0x3ee0, 0x3f4c])
        seq, loop = kongs.kiddy_team_recovery(program, frames)
        self.assertEqual(seq, [(0x3f40, 16), (0x3f44, 4), (0x3f48, 4), (0x3ee0, 4)])
        self.assertEqual(seq[loop:], [(0x3ee0, 4)])
        del frames[0x3ee0]
        with self.assertRaises(ValueError):
            kongs.kiddy_team_recovery(program, frames)

    def test_generated_callback_wrapper_is_checked_and_idempotent(self):
        spec = importlib.util.spec_from_file_location(
            "kongs_overrides", Path(__file__).resolve().parents[1] / "scripts/apply_dkc2_kongs_overrides.py")
        overrides = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(overrides)
        symbol = "CODE_B9D965_M0X0"
        source = ('#include "funcs.h"\nRecompReturn ' + symbol + '(CpuState *cpu) {\n' +
                  'cpu_trace_func_entry(cpu, 0xB9D967, "' + symbol + '");\nreturn 0;\n}\n')
        result = overrides.adapt(source, symbol, 0xb9d967)
        self.assertEqual(result, overrides.adapt(result, symbol, 0xb9d967))
        self.assertEqual(result.count('Dkc2KongsUseCallbacks'), 1)
        self.assertIn('0xb9d967u, 2, NULL', result)
        with self.assertRaises(ValueError):
            overrides.adapt(source, symbol, 0xb9d965)
        with self.assertRaises(ValueError):
            overrides.adapt(source + source, symbol, 0xb9d967)

    def test_comments_are_not_instructions(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "test.asm"
            p.write_text('; incbin "secret"\nif !version == 0\ndb $01\nelse\ndb $02\nendif\n')
            self.assertEqual(list(kongs.source_lines(p)), ["db $01"])


if __name__ == "__main__":
    unittest.main()
