#!/usr/bin/env python3
"""Apply source-owned DKC2 simultaneous co-op adaptations to private generated C.

The patch rewrites the Kong action input gate so that, in "2 PLAYER TEAM"
mode with the simultaneous policy selected, both Kongs read their own
controller:

* The gate compare (LDX current_sprite / CPX $0593 / BEQ) wraps its $0593
  read with Dkc2CoopGateActiveValue, which substitutes current_sprite for
  the active-Kong address so both Kong slots take the input path. With the
  classic policy (or outside TEAM mode) the cartridge value is returned
  and behavior is byte-for-byte stock.
* The input path (LDA $050E/$0510 -> STA $0981/$0983) wraps its reads with
  Dkc2CoopSelectHeldWord/Dkc2CoopSelectPressedWord, which substitute the
  per-controller words ($0502/$0504 held, $0506/$0508 pressed) keyed by
  the Kong slot, matching the cartridge's own slot-to-controller binding
  from set_active_kong ($80:883B).

* The state read in the named Kong dispatcher wraps $2E,x with
  Dkc2CoopSelectStateWord, admitting the waiting second player and preventing
  the normal follower-history state from taking control back after landing.
* Normal follower clipping uses the existing separate hitbox path. Accepted
  stomp reactions remember the colliding Kong and select its bounce handler.
* The normal dim-follower palette offset is suppressed, leaving special
  status palettes on their existing branches.

Gate anchors are instruction-shape based, so the adapter tolerates
revision-dependent PC drift between game revisions. The active-word loads
are located first (their $0983/$0981 store context is unique across the
tree); the gate compare is then the $0593 read above them in the same unit
fed by the current_sprite load and taken by the equality branch. Other
$0593 compares in the tree share the local context but not that position.
Every adaptation is verified by exactly-one-match counts and is idempotent
across regeneration runs.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re


INCLUDE = '#include "dkc2_coop.h"'

CURRENT_SPRITE_READ = re.compile(
    r"cpu_read16\(cpu, 0x00, \(uint16\)\(cpu->D \+ 0x0064\)\)")
X16_READ = re.compile(r"uint16 \w+ = cpu_read_x16\(cpu\);")
COMPARE_READ = re.compile(
    r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x0593\)\)")
COMPARE_BRANCH = re.compile(r"if \(cpu->_flag_Z == 1\)")
PRESSED_READ = re.compile(
    r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x0510\)\)")
HELD_READ = re.compile(
    r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x050e\)\)")
PRESSED_STORE = re.compile(r"cpu_write16\(cpu, [^,]*, \(uint16\)\(0x0983\),")
HELD_STORE = re.compile(r"cpu_write16\(cpu, [^,]*, \(uint16\)\(0x0981\),")
STATE_READ = re.compile(
    r"cpu_read16\(cpu, 0x00, \(uint16\)\(cpu->D \+ 0x002e \+ cpu->X\)\)")
STATE_HANDLER = re.compile(
    r"RecompReturn kong_state_handler_M0X0\(CpuState \*cpu\) \{.*?^\}",
    re.MULTILINE | re.DOTALL)

CONTEXT_WINDOW = 800
GATE_MAX_DISTANCE_FROM_PRESSED = 8192
CLIPPING_ENTRY = "RecompReturn get_active_kong_clipping_M0X0(CpuState *cpu) {"
FOLLOWER_CLIPPING_ENTRY = "RecompReturn CODE_BCFB2C_M0X0(CpuState *cpu) {"
CLIPPING_REDIRECT = """
  /* The existing inactive-Kong clipping path keeps the two hitboxes separate. */
  if (Dkc2CoopUseFollowerClipping(cpu))
    return CODE_BCFB2C_M0X0(cpu);
"""
INTERACTION_HANDLER = re.compile(
    r"RecompReturn set_player_interaction_M0X0\(CpuState \*cpu\) \{.*?^\}",
    re.MULTILINE | re.DOTALL)
BOUNCE_HANDLER = re.compile(
    r"RecompReturn player_interaction_1B_M0X0\(CpuState \*cpu\) \{.*?^\}",
    re.MULTILINE | re.DOTALL)
BOUNCE_CALL = "work_on_active_kong_M0X0(cpu)"
BOUNCE_REDIRECT = ("(Dkc2CoopBounceUsesFollower(cpu) ? "
                   "work_on_inactive_kong_M0X0(cpu) : " + BOUNCE_CALL + ")")
ROPE_FUNCTIONS = ("player_interaction_11", "player_interaction_12")
ROPE_REDIRECT = ("(Dkc2CoopRopeUsesFollower(cpu) ? "
                 "work_on_inactive_kong_M0X0(cpu) : " + BOUNCE_CALL + ")")
PALETTE_HANDLER = re.compile(
    r"RecompReturn CODE_BB8B66_M0X0\(CpuState \*cpu\) \{.*?^\}",
    re.MULTILINE | re.DOTALL)
PALETTE_OFFSET = re.compile(r"\b0x1e\b")
RECOVERY_HANDLER = re.compile(
    r"RecompReturn CODE_B9D705_M0X0\(CpuState \*cpu\) \{.*?^\}",
    re.MULTILINE | re.DOTALL)
FOLLOWER_READ = re.compile(r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x0597\)\)")
HELD_READ_WORD = re.compile(r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x0d7a\)\)")
OWNER_READ = re.compile(r"cpu_read16\(cpu, (?:cpu->DB|0x00), \(uint16\)\(0x0593\)\)")
PICKUP_X_READ = re.compile(r"cpu_read_x16\(cpu\)")
OWNER_FUNCTIONS = {"update_held_sprite_position": 1, "animation_command_89": 1,
                   "CODE_B9D3E7": 1, "animation_command_8B": 1, "animation_command_8C": 1,
                   "CODE_B8D4AE": 5}  # release terrain sweep from the thrower's position

ANIMAL_TYPE_READ = re.compile(r"cpu_read16\(cpu, 0x00, \(uint16\)\(cpu->D \+ 0x006e\)\)")
# Kong movement/reaction and animation entry points, including emitted tail paths.
# Crate spawning keeps the global mounted-animal check.
ANIMAL_TYPE_FUNCTIONS = {
    "CODE_B8830E": 1,
    "CODE_B88936": 2,
    "CODE_B88F14": 1,
    "CODE_B88F39": 4,
    "CODE_B890E1": 1,
    "CODE_B893DF": 1,
    "CODE_B8958F": 1,
    "CODE_B898C2": 1,
    "CODE_B89AE0": 1,
    "CODE_B89B49": 1,
    "CODE_B8A98E": 1,
    "CODE_B8B5C3": 1,
    "CODE_B9D705": 1,
    "CODE_B9DEEF": 1,
    "CODE_B9DF51": 1,
    "CODE_B9DFB6": 1,
    "CODE_B9E003": 2,
    "CODE_B9E021": 1,
    "CODE_B9E0A1": 1,
    "CODE_B9E0D0": 2,
    "CODE_B9E11A": 1,
    "CODE_B9E252": 1,
    "animal_special_action": 3,
    "bank_B8_981E": 1,
    "bank_B8_B4ED": 1,
    "check_if_animal_or_holding_sprite": 1,
    "coop_take_control_start_action": 1,
    "coop_take_control_y_action": 1,
    "dismount_animal_action": 1,
    "get_player_x_move_speed": 2,
    "get_x_acceleration": 1,
    "handle_player_slope_state_and_anim": 1,
    "kong_state_09": 1,
    "kong_state_0A": 1,
    "kong_state_12": 2,
    "kong_state_16": 1,
    "land_animal_attack_action": 1,
    "player_interaction_09": 1,
    "player_interaction_0C": 1,
    "player_interaction_0E": 1,
    "player_interaction_18": 4,
    "player_interaction_19": 4,
    "player_interaction_1B": 1,
    "player_interaction_23": 2,
    "player_interaction_27": 1,
    "set_anim_handle_animal_and_dixie": 1,
    "set_player_jumping_gravity": 1,
    "set_player_normal_gravity": 1,
    "set_player_terminal_velocity": 1,
    "set_player_terminal_velocity_down": 1,
    "set_swimming_state": 1,
    "shoot_web_platform_l_action": 1,
    "shoot_web_platform_r_action": 1,
    "slow_rattly_if_on_ground": 1,
    "start_player_falling": 1,
    "start_player_jumping": 1,
}

POLICY_READS = [
    # Single/double-rope hanging, direction, speed and junction completion
    # callbacks normally skip the AI follower. Independent climbers use them.
    ("CODE_B9DAB7", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DAE0", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DB19", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DB45", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DD61", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DD7C", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DD8E", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DD9C", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DDB7", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DDC9", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DDE8", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9DE17", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("CODE_B9E013", FOLLOWER_READ, 1, "Dkc2CoopRopeAnimationFollowerValue"),
    ("team_up_action", FOLLOWER_READ, 2, "Dkc2CoopTeamPartnerValue"),
    ("team_up_action", re.compile(r"cpu_read16\(cpu, \(uint8\)\([^;\n]*?0x002e[^;\n]*?\)\)\)"), 1, "Dkc2CoopTeamStateValue"),
    ("bank_B5_F776", re.compile(r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x0d44\)\)"), 1, "Dkc2CoopBananaSecondWidth"),
    ("prevent_sprite_from_leaving_level_x", re.compile(r"(?:(?<= = )|(?<=Dkc2CoopScreenLeftValue\(cpu, ))0x10\b"), 1, "Dkc2CoopScreenLeftValue"),
    ("prevent_sprite_from_leaving_level_x", re.compile(r"(?:(?<= = )|(?<=Dkc2CoopScreenSpanValue\(cpu, ))0xe0\b"), 1, "Dkc2CoopScreenSpanValue"),
    ("kong_state_27", re.compile(r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x0d66\)\)"), 1, "Dkc2CoopHandoffXValue"),
    ("kong_state_27", re.compile(r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x0d6a\)\)"), 1, "Dkc2CoopHandoffYValue"),
    ("handle_animal_mounting", OWNER_READ, 1, "Dkc2CoopMountCandidateValue"),
    ("get_state_death_and_mounting_flags", OWNER_READ, 1, "Dkc2CoopMountCandidateValue"),
    ("snap_follower_kong_to_animal",
     re.compile(r"cpu_read16\(cpu, cpu->DB, \(uint16\)\(0x08c2\)\)"),
     1, "Dkc2CoopAnimalFollowerFlags"),
] + [(name, ANIMAL_TYPE_READ, count, "Dkc2CoopAnimalTypeValue")
     for name, count in ANIMAL_TYPE_FUNCTIONS.items()]
MOUNT_ENTRY = "RecompReturn player_interaction_17_M0X0(CpuState *cpu) {"
MOUNT_PREPARE = "\n  Dkc2CoopPrepareAnimalMount(cpu);\n"
MOUNT_COLLISION_CALL = "check_active_kong_collision_M0X0(cpu)"
MOUNT_COLLISION_REDIRECT = ("(Dkc2CoopUseBothMountColliders(cpu) ? "
                            "check_inactive_kong_collision_M0X0(cpu) : " + MOUNT_COLLISION_CALL + ")")
HANDOFF_TRACE = "cpu_trace_block(cpu, 0xB8A16F);"
HANDOFF_SKIP = "\n    if (Dkc2CoopHandoffInPlace(cpu)) goto L_A1AB_M0X0;"
BANANA_TRACE = "cpu_trace_block(cpu, 0xB5F794);"
BANANA_REPEAT = "\n    if (Dkc2CoopBananaNextPass(cpu)) goto L_F789_M0X0;"



def require_action_calls(sources):
    repeats = named_candidates(sources, "bank_B5_F776", re.compile(re.escape(BANANA_TRACE)))
    targets = named_candidates(sources, "bank_B5_F776", re.compile(r"L_F789_M0X0:"))
    if len(repeats) != 1 or len(targets) != 1 or repeats[0][0] != targets[0][0]:
        raise ValueError("expected one banana group continuation and collision label")
    count = sum(s.count("Dkc2CoopBananaNextPass(") for s in sources.values())
    if count and (count != 1 or BANANA_TRACE + BANANA_REPEAT not in sources[repeats[0][0]]):
        raise ValueError("out of place banana group adaptation")
    handoffs = named_candidates(sources, "kong_state_27", re.compile(re.escape(HANDOFF_TRACE)))
    targets = named_candidates(sources, "kong_state_27", re.compile(r"L_A1AB_M0X0:"))
    if len(handoffs) != 1 or len(targets) != 1 or handoffs[0][0] != targets[0][0]:
        raise ValueError("expected one handoff movement entry and completion label")
    count = sum(s.count("Dkc2CoopHandoffInPlace(") for s in sources.values())
    if count and (count != 1 or HANDOFF_TRACE + HANDOFF_SKIP not in sources[handoffs[0][0]]):
        raise ValueError("out of place handoff movement adaptation")
    calls = named_candidates(sources, "handle_animal_mounting", re.compile(re.escape(MOUNT_COLLISION_CALL)))
    if len(calls) != 1:
        raise ValueError("expected one animal mounting collision call")
    count = sum(s.count("Dkc2CoopUseBothMountColliders(") for s in sources.values())
    path, pos, length = calls[0]
    prefix = MOUNT_COLLISION_REDIRECT[:-len(MOUNT_COLLISION_CALL)-1]
    if count and (count != 1 or sources[path][pos-len(prefix):pos+length+1] != MOUNT_COLLISION_REDIRECT):
        raise ValueError("out of place animal mounting collision call")
    helpers = {row[3] for row in POLICY_READS}
    anchored = dict.fromkeys(helpers, 0)
    for name, pattern, expected, helper in POLICY_READS:
        candidates = named_candidates(sources, name, pattern)
        if len(candidates) != expected:
            raise ValueError(f"expected {expected} policy operand(s) in {name}; found {len(candidates)}")
        prefix = f"{helper}(cpu, "
        for path, pos, length in candidates:
            if sources[path][pos-len(prefix):pos] == prefix and sources[path][pos+length:pos+length+1] == ")":
                anchored[helper] += 1
    for helper in helpers:
        if sum(s.count(helper + "(") for s in sources.values()) != anchored[helper]:
            raise ValueError(f"out of place {helper} adaptation")
    entries = [p for p, s in sources.items() for _ in range(s.count(MOUNT_ENTRY))]
    if len(entries) != 1:
        raise ValueError("expected one accepted animal mount entry")
    count = sum(s.count("Dkc2CoopPrepareAnimalMount(") for s in sources.values())
    if count and (count != 1 or MOUNT_ENTRY + MOUNT_PREPARE not in sources[entries[0]]):
        raise ValueError("out of place animal mount preparation")


def named_candidates(sources, name, pattern):
    handler_re = re.compile(r"RecompReturn " + re.escape(name) +
                            r"_M0X0\(CpuState \*cpu\) \{.*?^\}",
                            re.MULTILINE | re.DOTALL)
    return [(path, handler.start() + match.start(), len(match.group()))
            for path, source in sources.items()
            for handler in handler_re.finditer(source)
            for match in pattern.finditer(handler.group())]


def pickup_candidates(sources):
    return [candidate for candidate in named_candidates(
                sources, "update_object_pickup", PICKUP_X_READ)
            if re.search(r"cpu_write16\(cpu, cpu->DB, \(uint16\)\(0x0d7a\),",
                         sources[candidate[0]][candidate[1]:candidate[1] + 160])]


def require_owner_calls(sources):
    # One owner read in each named animation/attachment function, rather than
    # replacing unrelated active-Kong accesses throughout the generated tree.
    total = sum(s.count("Dkc2CoopHeldOwnerValue(") for s in sources.values())
    wrapped = 0
    for name, expected in OWNER_FUNCTIONS.items():
        candidates = named_candidates(sources, name, OWNER_READ)
        if len(candidates) != expected:
            raise ValueError(f"expected exactly {expected} held owner reads in {name}")
        for path, pos, length in candidates:
            prefix = "Dkc2CoopHeldOwnerValue(cpu, "
            if sources[path][pos-len(prefix):pos] == prefix:
                if sources[path][pos+length:pos+length+1] != ")":
                    raise ValueError("misplaced held owner wrapper")
                wrapped += 1
    if total != wrapped:
        raise ValueError("ambiguous existing held owner adaptation")


def recovery_candidates(sources):
    handlers = [(path, handler) for path, source in sources.items()
                for handler in RECOVERY_HANDLER.finditer(source)]
    if len(handlers) != 1:
        raise ValueError("expected exactly one Kong recovery handler")
    path, handler = handlers[0]
    reads = list(FOLLOWER_READ.finditer(handler.group()))
    # Only the entry CPX distinguishes follower recovery. The later CMP
    # distinguishes a carried Kong from a carried object and stays stock.
    if len(reads) != 2:
        raise ValueError("expected two follower reads in Kong recovery")
    match = reads[0]
    return [(path, handler.start() + match.start(), len(match.group()))]


def palette_candidates(sources):
    return [(path, handler.start() + match.start(), len(match.group()))
            for path, source in sources.items()
            for handler in PALETTE_HANDLER.finditer(source)
            for match in PALETTE_OFFSET.finditer(handler.group())]


def interaction_candidates(sources):
    return [(path, handler.start() + match.start(), len(match.group()))
            for path, source in sources.items()
            for handler in INTERACTION_HANDLER.finditer(source)
            for match in CURRENT_SPRITE_READ.finditer(handler.group())]


def bounce_candidate(sources):
    candidates = [(path, handler) for path, source in sources.items()
                  for handler in BOUNCE_HANDLER.finditer(source)]
    if len(candidates) != 1 or candidates[0][1].group().count(BOUNCE_CALL) != 1:
        raise ValueError("expected exactly one stomp reaction active-Kong call")
    path, handler = candidates[0]
    redirects = sum(s.count("Dkc2CoopBounceUsesFollower(") for s in sources.values())
    if redirects and (redirects != 1 or BOUNCE_REDIRECT not in handler.group()):
        raise ValueError("ambiguous existing stomp reaction adaptation")
    return path, handler, redirects


def require_rope_calls(sources):
    anchored = 0
    for name in ROPE_FUNCTIONS:
        calls = named_candidates(sources, name, re.compile(re.escape(BOUNCE_CALL)))
        if len(calls) != 1:
            raise ValueError(f"expected one rope reaction active-Kong call in {name}")
        path, pos, length = calls[0]
        prefix = ROPE_REDIRECT[:-len(BOUNCE_CALL)-1]
        if sources[path][pos-len(prefix):pos+length+1] == ROPE_REDIRECT:
            anchored += 1
    if sum(s.count("Dkc2CoopRopeUsesFollower(") for s in sources.values()) != anchored:
        raise ValueError("out of place rope reaction adaptation")


def generated_units(generated_dir: Path) -> list[Path]:
    return sorted(generated_dir.glob("*.c"))


def qualifies(text: str, position: int, requirements) -> bool:
    """True when every (pattern, pre_window, post_window) requirement is
    met at `position`; a zero window skips that side."""
    for pattern, pre_window, post_window in requirements:
        pre = text[max(0, position - pre_window):position]
        post = text[position:position + post_window]
        if (pre_window and not pattern.search(pre)) or \
                (post_window and not pattern.search(post)):
            return False
    return True


def find_candidates(
        sources: dict[Path, str], read_pattern: re.Pattern[str],
        alternative_requirements: list) -> list[tuple[Path, int, int]]:
    """Every read position satisfying any single requirement set."""
    candidates: list[tuple[Path, int, int]] = []
    for path, text in sources.items():
        start = 0
        while True:
            match = read_pattern.search(text, start)
            if not match:
                break
            position = match.start()
            if any(qualifies(text, position, requirements)
                   for requirements in alternative_requirements):
                candidates.append((path, position, match.end() - position))
            start = match.end()
    return candidates


def require_single(candidates: list[tuple[Path, int, int]], what: str,
                   helper: str, sources: dict[Path, str]) -> bool:
    """Fail closed unless exactly one candidate exists. Returns True when
    the wrap must be applied; False when the one existing helper call is
    already anchored at that candidate (idempotency)."""
    already = [(path, match.start()) for path, text in sources.items()
               for match in re.finditer(r"\b" + helper + r"\(", text)]
    if already:
        if len(already) != 1 or len(candidates) != 1:
            raise ValueError(f"ambiguous existing {helper} adaptation")
        path, position, length = candidates[0]
        prefix = f"{helper}(cpu, "
        if already[0] != (path, position - len(prefix)) or \
                sources[path][position + length:position + length + 1] != ")":
            raise ValueError(f"existing {helper} call is out of place")
        return False
    if len(candidates) != 1:
        raise ValueError(
            f"expected exactly one {what} candidate across generated "
            f"sources; found {len(candidates)}")
    return True


def wrap_candidate(sources: dict[Path, str],
                   candidate: tuple[Path, int, int], helper: str) -> None:
    path, position, length = candidate
    text = sources[path]
    wrapped = f"{helper}(cpu, {text[position:position + length]})"
    sources[path] = text[:position] + wrapped + text[position + length:]


def state_candidates(sources: dict[Path, str]) -> list[tuple[Path, int, int]]:
    return [(path, handler.start() + match.start(), len(match.group()))
            for path, source in sources.items()
            for handler in STATE_HANDLER.finditer(source)
            for match in STATE_READ.finditer(handler.group())]


def apply_overrides(generated_dir: Path) -> list[Path]:
    paths = generated_units(generated_dir)
    if not paths:
        raise ValueError("no generated C units found")
    sources = {path: path.read_text(encoding="utf-8") for path in paths}
    originals = sources.copy()
    require_action_calls(sources)
    require_owner_calls(sources)
    require_single(pickup_candidates(sources), "accepted object pickup",
                   "Dkc2CoopRecordPickupValue", sources)
    require_single(named_candidates(sources, "CODE_B9D705", HELD_READ_WORD),
                   "recovery held object", "Dkc2CoopHeldObjectValue", sources)
    bounce_candidate(sources)
    require_rope_calls(sources)
    require_single(recovery_candidates(sources), "Kong recovery follower compare",
                   "Dkc2CoopRecoveryFollowerValue", sources)
    require_single(palette_candidates(sources), "follower palette offset",
                   "Dkc2CoopFollowerPaletteOffset", sources)
    require_single(interaction_candidates(sources), "interaction source read",
                   "Dkc2CoopRecordInteractionSource", sources)
    clipping = [path for path, source in sources.items()
                for _ in range(source.count(CLIPPING_ENTRY))]
    if len(clipping) != 1 or sum(s.count(FOLLOWER_CLIPPING_ENTRY)
                                 for s in sources.values()) != 1:
        raise ValueError("expected exactly one active and inactive clipping entry")
    clipping_path = clipping[0]
    redirects = sum(s.count("Dkc2CoopUseFollowerClipping(") for s in sources.values())
    if redirects and (redirects != 1 or CLIPPING_ENTRY + CLIPPING_REDIRECT
                      not in sources[clipping_path]):
        raise ValueError("ambiguous existing follower clipping adaptation")
    state = state_candidates(sources)
    require_single(state, "Kong state dispatch read",
                   "Dkc2CoopSelectStateWord", sources)

    # Input path first: the active pressed/held word loads feeding the
    # $0983/$0981 stores are gate-unique across the tree.
    pressed = find_candidates(
        sources, PRESSED_READ, [[(PRESSED_STORE, 0, CONTEXT_WINDOW)]])
    require_single(pressed, "active pressed word load",
                   "Dkc2CoopSelectPressedWord", sources)
    held = find_candidates(
        sources, HELD_READ, [[(HELD_STORE, 0, CONTEXT_WINDOW)]])
    require_single(held, "active held word load",
                   "Dkc2CoopSelectHeldWord", sources)

    # Gate compare: the $0593 read fed by current_sprite and taken by the
    # equality branch into the input path. Other $0593 compares share the
    # local context, so the discriminator is position: the real gate sits
    # above the pressed-word load it admits into (same unit, bounded
    # distance). The fall-through CLC path (screen fades) is untouched.
    pressed_path, pressed_pos, _ = pressed[0]
    gate = [candidate for candidate in find_candidates(
                sources, COMPARE_READ,
                [[(CURRENT_SPRITE_READ, CONTEXT_WINDOW, 0),
                  (COMPARE_BRANCH, 0, CONTEXT_WINDOW)],
                 [(X16_READ, CONTEXT_WINDOW, 0),
                  (COMPARE_BRANCH, 0, CONTEXT_WINDOW)]])
            if candidate[0] == pressed_path and
            candidate[1] < pressed_pos and
            pressed_pos - candidate[1] < GATE_MAX_DISTANCE_FROM_PRESSED]
    require_single(gate, "gate compare", "Dkc2CoopGateActiveValue", sources)

    # Wrap the gate first, then re-find the word loads: inserted text
    # shifts every later position in the unit. Each wrap is skipped when
    # the helper is already anchored there (idempotent regeneration).
    if require_single(gate, "gate compare", "Dkc2CoopGateActiveValue",
                      sources):
        wrap_candidate(sources, gate[0], "Dkc2CoopGateActiveValue")
    pressed = find_candidates(
        sources, PRESSED_READ, [[(PRESSED_STORE, 0, CONTEXT_WINDOW)]])
    if require_single(pressed, "active pressed word load",
                      "Dkc2CoopSelectPressedWord", sources):
        wrap_candidate(sources, pressed[0], "Dkc2CoopSelectPressedWord")
    held = find_candidates(
        sources, HELD_READ, [[(HELD_STORE, 0, CONTEXT_WINDOW)]])
    if require_single(held, "active held word load",
                      "Dkc2CoopSelectHeldWord", sources):
        wrap_candidate(sources, held[0], "Dkc2CoopSelectHeldWord")
    state = state_candidates(sources)
    if require_single(state, "Kong state dispatch read",
                      "Dkc2CoopSelectStateWord", sources):
        wrap_candidate(sources, state[0], "Dkc2CoopSelectStateWord")
    if not redirects:
        sources[clipping_path] = sources[clipping_path].replace(
            CLIPPING_ENTRY, CLIPPING_ENTRY + CLIPPING_REDIRECT, 1)
    interaction = interaction_candidates(sources)
    if require_single(interaction, "interaction source read",
                      "Dkc2CoopRecordInteractionSource", sources):
        wrap_candidate(sources, interaction[0], "Dkc2CoopRecordInteractionSource")
    bounce_path, handler, bounce_redirects = bounce_candidate(sources)
    if not bounce_redirects:
        source = sources[bounce_path]
        sources[bounce_path] = (source[:handler.start()] +
            handler.group().replace(BOUNCE_CALL, BOUNCE_REDIRECT, 1) +
            source[handler.end():])
    for name in ROPE_FUNCTIONS:
        path, pos, length = named_candidates(sources, name, re.compile(re.escape(BOUNCE_CALL)))[0]
        prefix = ROPE_REDIRECT[:-len(BOUNCE_CALL)-1]
        if sources[path][pos-len(prefix):pos+length+1] != ROPE_REDIRECT:
            sources[path] = sources[path][:pos] + ROPE_REDIRECT + sources[path][pos+length:]
        declaration = "RecompReturn work_on_inactive_kong_M0X0(CpuState *cpu);"
        if declaration not in sources[path]:
            sources[path] = sources[path].replace('#include "funcs.h"', '#include "funcs.h"\n' + declaration, 1)
    palette = palette_candidates(sources)
    if require_single(palette, "follower palette offset",
                      "Dkc2CoopFollowerPaletteOffset", sources):
        wrap_candidate(sources, palette[0], "Dkc2CoopFollowerPaletteOffset")
    recovery = recovery_candidates(sources)
    if require_single(recovery, "Kong recovery follower compare",
                      "Dkc2CoopRecoveryFollowerValue", sources):
        wrap_candidate(sources, recovery[0], "Dkc2CoopRecoveryFollowerValue")
    pickup = pickup_candidates(sources)
    if require_single(pickup, "accepted object pickup", "Dkc2CoopRecordPickupValue", sources):
        wrap_candidate(sources, pickup[0], "Dkc2CoopRecordPickupValue")
    held_object = named_candidates(sources, "CODE_B9D705", HELD_READ_WORD)
    if require_single(held_object, "recovery held object", "Dkc2CoopHeldObjectValue", sources):
        wrap_candidate(sources, held_object[0], "Dkc2CoopHeldObjectValue")
    for name in OWNER_FUNCTIONS:
        # Right-to-left leaves earlier offsets valid when a function has
        # several branches that read the owner.
        for candidate in reversed(named_candidates(sources, name, OWNER_READ)):
            path, pos, _ = candidate
            prefix = "Dkc2CoopHeldOwnerValue(cpu, "
            if sources[path][pos-len(prefix):pos] != prefix:
                wrap_candidate(sources, candidate, "Dkc2CoopHeldOwnerValue")

    for name, pattern, _, helper in POLICY_READS:
        for candidate in reversed(named_candidates(sources, name, pattern)):
            path, pos, _ = candidate
            prefix = f"{helper}(cpu, "
            if sources[path][pos-len(prefix):pos] != prefix:
                wrap_candidate(sources, candidate, helper)
    for path, source in sources.items():
        if MOUNT_ENTRY in source and MOUNT_ENTRY + MOUNT_PREPARE not in source:
            sources[path] = source.replace(MOUNT_ENTRY, MOUNT_ENTRY + MOUNT_PREPARE, 1)
    for path, pos, length in named_candidates(sources, "kong_state_27", re.compile(re.escape(HANDOFF_TRACE))):
        if HANDOFF_TRACE + HANDOFF_SKIP not in sources[path]:
            sources[path] = sources[path][:pos+length] + HANDOFF_SKIP + sources[path][pos+length:]

    for path, pos, length in named_candidates(sources, "bank_B5_F776", re.compile(re.escape(BANANA_TRACE))):
        if BANANA_TRACE + BANANA_REPEAT not in sources[path]:
            sources[path] = sources[path][:pos+length] + BANANA_REPEAT + sources[path][pos+length:]

    for path, pos, length in named_candidates(sources, "handle_animal_mounting", re.compile(re.escape(MOUNT_COLLISION_CALL))):
        if "Dkc2CoopUseBothMountColliders(" not in sources[path]:
            sources[path] = sources[path][:pos] + MOUNT_COLLISION_REDIRECT + sources[path][pos+length:]

    changed: list[Path] = []
    for path in paths:
        text = sources[path]
        if "Dkc2Coop" not in text:
            continue
        if '#include "dkc2_coop.h"' not in text:
            marker = '#include "funcs.h"'
            if text.count(marker) != 1:
                raise ValueError(
                    f"generated unit {path.name} has an unexpected funcs.h "
                    f"include")
            text = text.replace(marker, marker + "\n" + INCLUDE, 1)
            sources[path] = text
        changed.append(path)

    for path in changed:
        if sources[path] != originals[path]:
            path.write_text(sources[path], encoding="utf-8", newline="\n")
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generated-dir", required=True, type=Path)
    args = parser.parse_args()
    generated_dir = args.generated_dir.expanduser().resolve(strict=True)
    changed = apply_overrides(generated_dir)
    for path in changed:
        print(f"Applied DKC2 co-op overrides: {path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"error: {error}")
        raise SystemExit(1)
