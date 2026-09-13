#!/usr/bin/env python3
"""Private TEAM barrel ownership/recovery acceptance using real controller inputs."""
import argparse
from pathlib import Path
import tempfile

from check_coop_combat import run_route
from check_coop_route import TEAM_START, SEPARATE_PLAYERS, check


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--kongs-pack", type=Path,
                        help="Also exercise Donkey/Kiddy replacement callbacks in TEAM mode")
    args = parser.parse_args()
    runner, rom = args.runner.resolve(strict=True), args.rom.resolve(strict=True)
    pickup = TEAM_START + SEPARATE_PLAYERS + [
        (0x80080, 90), (0x82080, 30), (0, 60), (0x82080, 60)]
    cases = {
        "dk_right": (pickup, 1, 0x1A8),
        "dk_left": (pickup + [(0x42000, 20)], -1, 0x1A8),
        "regular_right": (pickup + [(0, 80), (0x80080, 30), (0x82080, 85)], 1, 0x1BC),
    }
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-barrels-") as tmp:
        directory = Path(tmp)
        for name, (prefix, direction, object_id) in cases.items():
            release = sum(n for _, n in prefix)
            move = 0x40000  # retreat into the cleared area after either throw
            data = run_route(runner, rom, directory, name,
                             prefix + [(0, 80), (move, 80), (0, 20)],
                             kongs_pack=args.kongs_pack.resolve() if args.kongs_pack else None)
            carried = data[release - 1]
            held = carried["held_slot"]
            check(held >= 0x0E9E and (held - 0x0DE2) % 0x5E == 0, f"{name}: no held barrel")
            slot = (held - 0x0DE2) // 0x5E
            barrel = carried["sprites"][slot]
            check(barrel["id"] == object_id, f"{name}: wrong barrel type")
            check(carried["b"]["x"] - carried["a"]["x"] > 100, f"{name}: players too close")
            check(abs(barrel["x"] - carried["b"]["x"]) < 40, f"{name}: held barrel follows P1")
            thrown = next((r for r in data[release:release + 70] if r["held_slot"] == 0), None)
            check(thrown is not None, f"{name}: barrel was not released")
            shot = thrown["sprites"][slot]
            check(shot["id"] == object_id, f"{name}: released barrel missing")
            check(abs(shot["x"] - thrown["b"]["x"]) < 48, f"{name}: wrong throw origin")
            check(abs(shot["x"] - thrown["a"]["x"]) > 100, f"{name}: throw starts at P1")
            moving = data[thrown["frame"] + 3]["sprites"][slot]
            check(moving["id"] == object_id and direction * (moving["x"] - shot["x"]) > 20,
                  f"{name}: barrel went in the wrong direction")
            start, end = data[release + 79], data[release + 159]
            check(start["b"]["x"] - end["b"]["x"] > 100,
                  f"{name}: P2 locked after throwing")
            check(start["a"]["x"] == end["a"]["x"], f"{name}: P2 controls moved P1")
            check(all(r["b"]["s"] != "003e" for r in data[release:]),
                  f"{name}: DK barrel rescued an already present P2")
            print(f"{name}: P2 carried, threw from its own position, and moved afterward")


if __name__ == "__main__":
    main()
