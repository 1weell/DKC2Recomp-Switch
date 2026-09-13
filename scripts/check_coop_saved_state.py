#!/usr/bin/env python3
"""Replay the reported Pirate Panic ledges regression from an external save slot.

This route is specific to the captured layout (P2 leader on the lower ledge).
It prints the state hash, never stores the owner's save or trace in the repo.
"""
import argparse
import hashlib
from pathlib import Path
import tempfile

from check_coop_combat import run_route
from check_coop_route import check


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--state", required=True, type=Path)
    args = parser.parse_args()
    runner, rom, state = (p.resolve(strict=True) for p in (args.runner, args.rom, args.state))
    print("state_sha256=" + hashlib.sha256(state.read_bytes()).hexdigest())
    cases = {
        "p2_contact": [(0x80000, 180), (0, 200), (0x41041, 60)],
        "p1_contact": [(0x80, 100), (0, 200), (0x40, 120)],
        "p2_roll": [(0x80000, 75), (0x82000, 60), (0, 100), (0x40000, 60)],
        "p1_roll": [(0x80, 112), (0x82, 40), (0, 100), (0x40, 60)],
    }
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-ledges-") as tmp:
        for name, route in cases.items():
            data = run_route(runner, rom, Path(tmp), name, route, state=state)
            check(data[0]["active"] == "0e40" and data[0]["a"]["y"] < data[0]["b"]["y"] - 50,
                  "this route requires the reported P2-leader ledges snapshot")
            player, other, slot = ("a", "b", 0) if name.startswith("p1") else ("b", "a", 1)
            if name.endswith("contact"):
                check(any(r[player]["s"] == "0024" for r in data), f"{name}: contact passed through enemy")
                check(all(r[player]["s"] == "0013" and r["sprites"][slot]["flags"] == 0
                          for r in data[220:]), f"{name}: lost player rejoined on input")
                if player == "b":
                    survivor = data[300]
                    # Keeping its original position lets the survivor hit the
                    # animal crate on this ledge and mount Rambi on landing.
                    check(survivor[other]["s"] == "0000" or
                          (survivor[other]["s"] == "0009" and survivor["animal_type"] == 0x19C),
                          "survivor is stuck waiting for a turn")
                    check(survivor["time_freeze"] != 7, "world is still frozen after handoff")
                    check(data[379][other]["x"] - data[-1][other]["x"] > 50 and
                          min(r[other]["y"] for r in data[380:]) < data[379][other]["y"] - 60,
                          "survivor cannot move after handoff")
            else:
                enemy_slot = 2 if slot == 0 else 4
                check(any(r["sprites"][enemy_slot]["id"] == 0x1EC and
                          r["sprites"][enemy_slot]["state"] == 1 for r in data),
                      f"{name}: roll did not defeat enemy on its ledge")
                check(data[-61][player]["x"] - data[-1][player]["x"] > 70,
                      f"{name}: attacker cannot move after killing enemy")
            print(f"{name}: passed from the supplied save")


if __name__ == "__main__":
    main()
