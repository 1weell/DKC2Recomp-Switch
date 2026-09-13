#!/usr/bin/env python3
"""Convert a user-supplied Project Kongs checkout into a private sprite pack.

Reads assembly declarations as data; never assembles or executes the checkout.
No game bytes are embedded in this tool. The output belongs outside Git.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

MAGIC = b"DKC2KNG5"


def source_lines(path):
    """Select revision zero and strip comments before interpreting declarations."""
    active = [True]
    conditions = []
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.split(";", 1)[0].strip()
        if line.startswith("if "):
            expr = line[3:].strip()
            if expr not in ("!version == 0", "!version == 1"):
                raise ValueError(f"unsupported conditional in {path}: {line}")
            conditions.append(expr.endswith("0"))
            active.append(active[-1] and conditions[-1])
        elif line == "else":
            active[-1] = active[-2] and not conditions[-1]
        elif line == "endif":
            active.pop()
            conditions.pop()
        elif active[-1] and line:
            yield line
    if len(active) != 1:
        raise ValueError(f"unterminated conditional in {path}")


def decode_sprite(data):
    if len(data) < 8:
        raise ValueError("short sprite header")
    large, small, start, extra, extra_start, dma1, dma2_start, dma2 = data[:8]
    count = large + small + extra
    header = 8 + count * 2
    if not 0 < count <= 64 or len(data) != header + (dma1 + dma2) * 32:
        raise ValueError("invalid sprite length")
    pieces = []
    for i in range(count):
        x, y = data[8 + i * 2] - 128, data[9 + i * 2] - 128
        size = 16 if i < large else 8
        tile = (i // 8) * 32 + (i % 8) * 2 if i < large else (
            start + i - large if i < large + small else
            extra_start + i - large - small)
        pieces.append((x, y, size, tile))
    x0 = min(p[0] for p in pieces)
    y0 = min(p[1] for p in pieces)
    width = max(p[0] + p[2] for p in pieces) - x0
    height = max(p[1] + p[2] for p in pieces) - y0
    if not (0 < width <= 128 and 0 < height <= 128):
        raise ValueError("sprite bounds exceed 128 pixels")
    pixels = bytearray(width * height)
    # Earlier OAM entries win where compound pieces overlap.
    for ox, oy, size, base in reversed(pieces):
        for y in range(size):
            for x in range(size):
                tile = base + (y // 8) * 16 + x // 8
                if tile < dma1:
                    source = tile
                elif dma2_start <= tile < dma2_start + dma2:
                    source = dma1 + tile - dma2_start
                else:
                    raise ValueError("sprite refers to a tile outside its DMA payload")
                addr = header + source * 32 + (y % 8) * 2
                shift = 7 - x % 8
                color = sum(((data[addr + offset] >> shift) & 1) << plane
                            for plane, offset in enumerate((0, 1, 16, 17)))
                if color:
                    pixels[(oy + y - y0) * width + ox + x - x0] = color
    return x0, y0, width, height, bytes(pixels)


def graphics(root):
    folder = root / "source/kong_hack/objects/graphics"
    files = {}
    for path in sorted(folder.glob("*.asm")):
        label = None
        for line in source_lines(path):
            m = re.fullmatch(r"(\w+):", line)
            if m:
                label = m[1]
            m = re.fullmatch(r'incbin "([^"]+)"', line)
            if m and label:
                target = (path.parent / m[1]).resolve()
                if not target.is_relative_to(root.resolve()):
                    raise ValueError("asset path escapes supplied checkout")
                files[label] = target
    frames = {}
    for name, first in (("donkey_sprite_graphics_table.asm", 0x35A0),
                        ("kiddy_sprite_graphics_table.asm", 0x3C98),
                        ("kong_hack_sprite_graphics_table.asm", 0x4220)):
        index = first
        for line in source_lines(folder / name):
            m = re.fullmatch(r"dl (\w+)\s*:\s*db \$[0-9A-Fa-f]+", line)
            if not m:
                continue
            if m[1] not in files:
                raise ValueError(f"missing graphic {m[1]}")
            frames[index] = decode_sprite(files[m[1]].read_bytes())
            index += 4
    return frames


def animation_program(paths):
    instructions, labels = [], {}
    for path in paths:
        for line in source_lines(path):
            m = re.fullmatch(r"(\w+):", line)
            if m:
                labels[m[1]] = len(instructions)
            elif line.startswith("db "):
                instructions.append(line)
    return instructions, labels


def project_sequence(program, label, frames, stop_before=None):
    """Project visual frames, following local loops/subroutines, without CPU calls.

    Conditional game-code callbacks are deliberately omitted: DKC2 owns behavior.
    A stable visual loop is retained and terminal sequences hold their last pose.
    """
    instructions, labels = program
    if label not in labels:
        return [], 0
    pc, stack, visited, result = labels[label], [], {}, []
    for _ in range(4096):
        if pc == stop_before:
            return result, 0
        key = (pc, tuple(stack))
        if key in visited:
            return result, visited[key]
        visited[key] = len(result)
        if pc >= len(instructions):
            break
        line = instructions[pc]
        pc += 1
        m = re.fullmatch(r"db \$([0-9A-Fa-f]+)\s*:\s*dw \$([0-9A-Fa-f]+)", line)
        if m:
            duration, graphic = int(m[1], 16), int(m[2], 16)
            if 0 < duration < 128 and graphic in frames:
                result.append((graphic, duration))
            continue
        m = re.match(r"db !animation_command_([0-9A-Fa-f]+)", line)
        if not m:
            continue
        command = int(m[1], 16)
        operands = line.split("dw", 1)[-1].strip().split(",")
        if command in (0x85, 0x86, 0x8B):
            values = re.findall(r"\$([0-9A-Fa-f]+)", line)
            if len(values) >= 3:
                duration = int(values[0], 16)
                # Compound animal/rider frames: only the rider belongs to this pack.
                choices = [int(v, 16) for v in values[1:(2 if command == 0x8B else 3)]]
                graphic = next((v for v in reversed(choices) if v in frames), None)
                if graphic is not None and 0 < duration < 128:
                    result.append((graphic, duration))
        elif command == 0x82:
            if operands[0] not in labels:
                break
            pc = labels[operands[0]]
        elif command == 0x91:
            if operands[0] in labels and len(stack) < 8:
                stack.append(pc)
                pc = labels[operands[0]]
        elif command == 0x92:
            if not stack:
                break
            pc = stack.pop()
        elif command == 0x80:
            # The original animation engine loops zero-ended sequences.
            loop = 0 if re.search(r",\s*\$00$", line) else max(0, len(result) - 1)
            return result, loop
    return result, max(0, len(result) - 1)


def mounted_offsets(path):
    """Read five attachment points per character, without running assembly."""
    result, current = {}, None
    for line in source_lines(path):
        m = re.fullmatch(r"(donkey|kiddy)_animal_offset_a:", line)
        if m:
            current = result[m[1]] = []
        m = re.fullmatch(r"dw \$([0-9a-fA-F]{4}),\s*\$([0-9a-fA-F]{4})", line)
        if m and current is not None:
            current.append(tuple(int(v, 16) - (0x10000 if int(v, 16) & 0x8000 else 0)
                                 for v in m.groups()))
    if set(result) != {"donkey", "kiddy"} or any(len(v) != 5 for v in result.values()):
        raise ValueError("expected five animal attachment points for each Kong")
    # This revision still uses Diddy's $1F08 art for Kiddy on Rattly. Adapt
    # Kiddy's own seated $4100 pose: its frame bottom is seven pixels higher.
    x, y = result["kiddy"][1]
    result["kiddy"][1] = (x, y + 7)
    return result


def handoff_sequence(program, label, frames, component, stop_callback):
    """Extract one character from a tag's paired frames, up to control transfer."""
    instructions, labels = program
    result = []
    for line in instructions[labels[label]:]:
        if line == f"db !animation_command_81 : dw {stop_callback}":
            if not result:
                break
            return result
        if line.startswith("db !animation_command_8A,"):
            values = [int(v, 16) for v in re.findall(r"\$([0-9A-Fa-f]+)", line)]
            if len(values) != 5 or not 0 < values[0] < 128:
                raise ValueError("malformed paired handoff frame")
            graphic = values[1 + component]
            if graphic not in frames:
                raise ValueError("missing handoff graphic")
            result.append((graphic, values[0]))
        elif line.startswith("db !animation_command_80"):
            break
    raise ValueError(f"missing handoff transfer marker in {label}")


def compound_poses(program, label, frames):
    """Collect animal/rider pairs on both sides of visual branches.

    The live animal graphic selects these poses; callbacks are never executed.
    Keeping the animal key and offsets avoids independently looping a jump.
    """
    instructions, labels = program
    if label not in labels:
        return []
    todo, seen, result = [(labels[label], ())], set(), []
    while todo:
        pc, stack = todo.pop()
        while pc < len(instructions) and (pc, stack) not in seen:
            if len(seen) >= 8192:
                raise ValueError("mounted animation graph exceeds limit")
            seen.add((pc, stack))
            line = instructions[pc]
            pc += 1
            m = re.match(r"db !animation_command_([0-9A-Fa-f]+)", line)
            if not m:
                continue
            command = int(m[1], 16)
            operands = [v.strip() for v in line.split("dw", 1)[-1].split(",")]
            if command in (0x85, 0x86):
                values = [int(v, 16) for v in re.findall(r"\$([0-9A-Fa-f]+)", line)]
                expected = 5 if command == 0x86 else 3
                if len(values) != expected or not 0 < values[0] < 128:
                    raise ValueError("malformed compound rider frame")
                duration, animal, rider = values[:3]
                if rider in frames:
                    dx, dy = values[3:] if command == 0x86 else (0, 0)
                    dx = dx - 65536 if dx & 0x8000 else dx
                    dy = dy - 65536 if dy & 0x8000 else dy
                    record = (animal, rider, dx, dy, int(command == 0x86), duration)
                    if not result or result[-1] != record:
                        result.append(record)
            elif command == 0x8F:
                if len(operands) == 2 and operands[1] in labels:
                    todo.append((labels[operands[1]], stack))
            elif command == 0x82:
                if operands[0] not in labels:
                    break
                pc = labels[operands[0]]
            elif command == 0x91:
                if operands[0] in labels and len(stack) < 8:
                    stack += (pc,)
                    pc = labels[operands[0]]
            elif command == 0x92:
                if not stack:
                    break
                pc, stack = stack[-1], stack[:-1]
            elif command == 0x80:
                break
    return result


def carry_offsets(program, used):
    """Preserve signed hand attachment positions from carry-frame declarations."""
    result = {}
    for line in program[0]:
        if not line.startswith("db !animation_command_8B,"):
            continue
        values = [int(v, 16) for v in re.findall(r"\$([0-9A-Fa-f]+)", line)]
        if len(values) != 4:
            raise ValueError("malformed carry frame")
        _, graphic, x, y = values
        if graphic in used:
            x, y = (v - 65536 if v & 0x8000 else v for v in (x, y))
            if not (-128 <= x <= 127 and -128 <= y <= 127):
                raise ValueError("carry offset outside sprite bounds")
            # Repeated poses share hand placement; the first declaration is
            # the ordinary carry cycle, before contextual team-up variants.
            result.setdefault(graphic, (x, y))
    return [(g, *xy) for g, xy in sorted(result.items())]


def kiddy_team_recovery(program, frames):
    """Adapt the grounded sit-up, excluding the hit and crying/death actions.

    The source borrows Diddy's team-stunned entry. Its generic Kiddy hurt
    substitute alternates upright and horizontal hit poses every six ticks;
    that is not a waiting animation. Retain the sit-up's source timing, then
    hold the seated pose until the native follower rejoins the leader.
    """
    instructions, labels = program
    start = instructions.index("db $10 : dw $3F40", labels["kiddy_death"])
    end = instructions.index("db $04 : dw $3EE0", start) + 1
    sequence, _ = project_sequence(
        (instructions, dict(labels, team_recovery=start)), "team_recovery", frames, end)
    if not sequence or sequence[-1][0] != 0x3ee0:
        raise ValueError("missing grounded Kiddy recovery pose")
    return sequence, len(sequence) - 1


def build_pack(root):
    frames = graphics(root)
    folder = root / "source/kong_hack/objects/animations"
    paths = [folder / "donkey_animations.asm", folder / "donkey_custom_animations.asm",
             folder / "kiddy_animations.asm", folder / "teamup_animations.asm",
             folder / "swap_animations.asm", folder / "kong_special_animations.asm"]
    program = animation_program(paths)
    # Kiddy's imported idle ends with a walking cycle whose original movement
    # callback is commented out. Split that cycle into a movement-only entry.
    instructions, program_labels = program
    kiddy_move = instructions.index("db $03 : dw $4104", program_labels["kiddy_animal_idle"])
    program_labels["kiddy_animal_move"] = kiddy_move
    offsets = mounted_offsets(root / "source/kong_hack/objects/source/kong_animal_offsets.asm")
    sequences, fallbacks, mounted = [], [], []
    aliases = {"glide": "air", "celebrate": "idle",
               "honey_wall_idle": "rope_vertical_single_idle",
               "rope_vertical_single_to_double": "rope_vertical_single_idle",
               "rope_vertical_double_to_single": "rope_vertical_single_idle",
               "rope_vertical_double_idle": "rope_vertical_single_idle",
               "rope_vertical_double_up": "rope_vertical_single_up",
               "rope_vertical_double_down": "rope_vertical_single_down",
               "hook_idle": "rope_horizontal_idle", "skull_cart": "crouch_loop",
               "scared": "hurt", "wind_float": "air"}
    for character, name in enumerate(("donkey", "kiddy"), 1):
        labels = [re.match(r"dw (\w+)", line)[1]
                  for line in source_lines(folder / f"{name}_anim_table.asm")
                  if line.startswith("dw ")]
        for semantic, label in enumerate(labels):
            if 94 <= semantic <= 158:
                for pose in compound_poses(program, label, frames):
                    mounted.append((character, semantic, *pose))
                # No replacement on the animal object itself.
                continue
            requested = label
            if label.startswith(("diddy_", "dixie_")):
                suffix = label.split("_", 1)[1]
                label = name + "_" + aliases.get(suffix, suffix)
            if semantic == 11:
                label = name + "_air"  # Dixie's helicopter state.
            if semantic == 73:
                label = name + "_swap_idle"  # Paired tag poses are selected at runtime.
            if semantic == 32 and name == "donkey":
                label = "donkey_animal_idle"  # The reference borrows Diddy's top pose.
            sequence, loop = project_sequence(program, label, frames)
            if semantic == 40 and name == "kiddy":
                sequence, loop = kiddy_team_recovery(program, frames)
            if not sequence:
                fallback = name + ("_hurt" if semantic in (10, 41, 42) else "_idle")
                sequence, loop = project_sequence(program, fallback, frames)
                fallbacks.append((name, semantic, requested, fallback))
            if not sequence:
                raise ValueError(f"missing essential animation for {name}: {label}")
            sequences.append((character, semantic, sequence, min(loop, len(sequence) - 1)))
        for animal in range(5):
            idle = (name + "_rattly_idle" if animal == 1 and name == "donkey" else
                    name + "_squawks_idle" if animal == 2 else name + "_animal_idle")
            stop = kiddy_move if name == "kiddy" and animal != 2 else None
            seq, loop = project_sequence(program, idle, frames, stop)
            if animal == 1 and name == "kiddy":
                seq, loop = project_sequence(program, "kiddy_animal_mount", frames)
            sequences.append((character, 164 + animal, seq, min(loop, len(seq) - 1)))
            move = name + ("_animal_move_start" if name == "donkey" else "_animal_move")
            if animal in (0, 3):
                seq, loop = project_sequence(program, move, frames)
            sequences.append((character, 169 + animal, seq, min(loop, len(seq) - 1)))
    # Hand-slap art and timing come from the supplied DK animation. Kiddy uses
    # his existing thrown-body somersault and landing art for a native belly flop.
    slap, loop = project_sequence(program, "donkey_hand_slap_animation", frames)
    sequences.append((1, 174, slap, len(slap) - 1))
    body, loop = project_sequence(program, "kiddy_team_top_thrown", frames)
    sequences.append((2, 174, body, len(body) - 1))
    land, loop = project_sequence(program, "kiddy_land", frames)
    sequences.append((2, 175, land, len(land) - 1))
    # The reference's DK/Kiddy pair entry is unfinished. Adapt their individual
    # tag gestures to DKC2's two roles; retain the native transfer and hop.
    incoming = handoff_sequence(program, "diddy_swap_to_donkey_animation", frames, 0, "dkc1_swap")
    outgoing = handoff_sequence(program, "donkey_swap_to_diddy_animation", frames, 1, "dkc1_swap")
    kiddy = handoff_sequence(program, "dixie_swap_to_kiddy_animation", frames, 0, "dkc3_swap_b")
    for character, semantic, seq in ((1, 176, incoming), (1, 177, outgoing),
                                     (2, 176, kiddy), (2, 177, kiddy)):
        sequences.append((character, semantic, seq, len(seq) - 1))
    # The top Kong must follow the carrier's state, not its own unchanged
    # native animation ID. DK has no original team-up, so adapt his seated and
    # tumbling art; Kiddy supplies dedicated DKC3 top idle/walk/air/throw art.
    for character, labels in ((1, ("donkey_animal_idle", "donkey_animal_move_start",
                                    "donkey_animal_mount", "donkey_team_top_thrown")),
                              (2, ("kiddy_team_top_idle", "kiddy_team_top_walk",
                                    "kiddy_team_top_air", "kiddy_team_top_throw"))):
        for semantic, label in enumerate(labels, 178):
            seq, loop = project_sequence(program, label, frames)
            if not seq:
                raise ValueError(f"missing paired team pose: {label}")
            sequences.append((character, semantic, seq, min(loop, len(seq) - 1)))
    used = {graphic for _, _, seq, _ in sequences for graphic, _ in seq}
    used.update(pose[3] for pose in mounted)
    payload = bytearray(MAGIC + struct.pack("<II", len(used), len(sequences)))
    for name in ("donkey", "kiddy"):
        palette = (root / f"source/kong_hack/objects/palettes/{name}_palette.bin").read_bytes()
        if len(palette) != 240:
            raise ValueError("expected eight fifteen-color palette variants")
        payload.extend(b"\0\0" + palette[:30])
    for graphic in sorted(used):
        x, y, width, height, pixels = frames[graphic]
        payload.extend(struct.pack("<HhhHH", graphic, x, y, width, height))
        payload.extend(pixels)
    for character, semantic, seq, loop in sequences:
        payload.extend(struct.pack("<HHHH", character, semantic, len(seq), loop))
        for graphic, duration in seq:
            payload.extend(struct.pack("<HH", graphic, duration))
    for name in ("donkey", "kiddy"):
        for x, y in offsets[name]:
            payload.extend(struct.pack("<hh", x, y))
    payload.extend(struct.pack("<I", len(mounted)))
    for character, semantic, animal, graphic, x, y, explicit, duration in mounted:
        payload.extend(struct.pack("<HHHHhhHH", character, semantic, animal, graphic,
                                   x, y, explicit, duration))
    carry = carry_offsets(program, used)
    payload.extend(struct.pack("<I", len(carry)))
    for graphic, x, y in carry:
        payload.extend(struct.pack("<Hhh", graphic, x, y))
    return bytes(payload), {"frames": len(used), "animations": len(sequences),
                            "mounted_poses": len(mounted), "carry_offsets": len(carry), "fallbacks": fallbacks}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data, report = build_pack(args.project.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    report["sha256"] = hashlib.sha256(data).hexdigest()
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
