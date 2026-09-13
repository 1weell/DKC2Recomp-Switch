#!/usr/bin/env python3
"""Private co-op movement limits in 4:3, 16:10, 16:9 and 21:9, using external saves."""
import argparse
from pathlib import Path
import tempfile

from check_coop_combat import run_route
from check_coop_route import TEAM_START, check


def check_edge(row, player, side, extra, label):
    check(row["wide_extra"] == extra and row["wide_ready"] == bool(extra),
          label + ": presentation geometry unavailable")
    camera, bias = row["camera_x"], row["wide_bias"]
    left = min(max(extra-bias, 0), max(camera-256, 0))
    right = min(max(extra+bias, 0), max(row["camera_max"]-camera, 0))
    expected = camera+16-left if side == "left" else camera+240+right
    check(abs(row[player]["x"] - expected) <= 1,
          f"{label}: stopped at {row[player]['x']}, expected visible edge {expected}")
    check(row[player]["s"] == "0000", label + ": player cannot stand at the edge")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--state", required=True, type=Path)
    args = parser.parse_args()
    runner, rom, state = (p.resolve(strict=True) for p in (args.runner, args.rom, args.state))
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-wide-") as tmp:
        directory = Path(tmp)
        # Use real mounting/dismounting to select P1 as leader, keeping P2 on foot.
        p1_leader = directory / "p1-leader.sav"
        run_route(runner, rom, directory, "select-p1",
                  [(1, 30), (0, 70), (0x200, 1), (0, 100)], state=state, save=p1_leader)
        for slot, snapshot in ((0, state), (1, p1_leader)):
            player, leader = ("a", "b") if slot == 0 else ("b", "a")
            for aspect, extra in (("4:3", 0), ("16:10", 26), ("16:9", 43), ("21:9", 95)):
                for side, word in (("left", 0x40), ("right", 0x82)):
                    name = f"p{slot+1}-{aspect.replace(':', '-')}-{side}"
                    data = run_route(runner, rom, directory, name,
                                     [(word << (12*slot), 170)], state=snapshot, aspect=aspect)
                    check(data[-1][leader]["x"] == data[0][leader]["x"], name + ": camera leader moved")
                    check(data[-1]["wide_bias"] == 0, name + ": expected a centered viewport")
                    check_edge(data[-1], player, side, extra, name)
            print(f"P{slot+1}: left/right movement reaches each aspect's visible edge")
        entrance = directory / "entrance.sav"
        run_route(runner, rom, directory, "new-team", TEAM_START, save=entrance)
        for policy in ("glide", "shift", "bars", "reflect"):
            east = directory / f"{policy}-east.sav"
            data = run_route(runner, rom, directory, policy + "-east", [(0x80000, 220)],
                             state=entrance, save=east, aspect="16:9", edge=policy)
            check(data[-1]["camera_x"] == 256, policy + ": expected level's west edge")
            check_edge(data[-1], "b", "right", 43, policy + " east")
            data = run_route(runner, rom, directory, policy + "-west", [(0x40000, 240)],
                             state=east, aspect="16:9", edge=policy)
            check_edge(data[-1], "b", "left", 43, policy + " west")
            check(data[-1]["b"]["x"] >= 272, policy + ": player crossed the level's west limit")
        print("Glide/shift/bars/reflect: visible side area is playable and west level limit is preserved")


if __name__ == "__main__":
    main()
