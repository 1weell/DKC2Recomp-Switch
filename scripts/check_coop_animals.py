#!/usr/bin/env python3
"""Private animal mounting regression from the reported Pirate Panic save.

The external snapshot has both Kongs present, P1 above P2 on the ledges,
and an unmounted animal beside P1. No guest memory is patched or bundled.
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
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-animals-") as tmp:
        directory = Path(tmp)
        for player, prefix in ((0, [(1, 30), (0, 70)]),
                               (1, [(0x81000, 45), (0, 100)])):
            name = f"p{player + 1}"
            rider, partner = ("a", "b") if player == 0 else ("b", "a")
            owner = "0de2" if player == 0 else "0e40"
            mounted = directory / f"{name}-mounted.sav"
            data = run_route(runner, rom, directory, name, prefix, state=state, save=mounted)
            check(data[0]["animal_type"] == 0 and data[0]["active"] == "0e40" and
                  abs(data[0]["a"]["x"] - 4477) < 3 and abs(data[0]["b"]["x"] - 4388) < 3,
                  "route requires the reported animal ledges snapshot")
            check(data[-1]["animal_type"] == 0x19C and data[-1]["active"] == owner and
                  data[-1][rider]["s"] == "0009", f"{name}: could not mount")
            shift, partner_shift = player * 12, (1-player) * 12
            # Reload ownership, then jump the on-foot player and try its dismount button.
            independent = run_route(runner, rom, directory, name + "-partner",
                [(1 << partner_shift, 30), (0, 60), (0x200 << partner_shift, 1), (0, 10)], state=mounted)
            check(any(r[partner]["s"] == "0006" for r in independent[:60]) and
                  all(r[partner]["s"] not in ("0009", "000a", "000b") for r in independent),
                  f"{name}: on-foot player inherited animal movement")
            check(all(r["animal_type"] == 0x19C and r["active"] == owner for r in independent),
                  f"{name}: partner input stole/dismounted the rider")
            check(abs(independent[-1][rider]["x"] - independent[0][rider]["x"]) <= 2,
                  f"{name}: partner input moved the animal")
            motion = run_route(runner, rom, directory, name + "-ride",
                [(0x40 << shift, 25), (0, 50), (1 << shift, 30), (0, 60),
                 (0x200 << shift, 1), (0, 80), (1 << shift, 30), (0, 70)], state=mounted)
            check(motion[0][rider]["x"] - motion[24][rider]["x"] > 15,
                  f"{name}: rider cannot move animal")
            check(any(r[rider]["s"] == "000b" for r in motion[75:165]),
                  f"{name}: rider cannot jump")
            check(all(r["animal_type"] == 0 for r in motion[165:246]),
                  f"{name}: rider cannot dismount")
            check(motion[-1]["animal_type"] == 0x19C and motion[-1][rider]["s"] == "0009",
                  f"{name}: rider cannot remount")
            print(f"{name}: mounting, save/load, separate controls, riding, jumping, dismount/remount passed")
        exchange = run_route(runner, rom, directory, "exchange",
            [(0x240, 30), (0, 80), (0x81000, 30), (0, 80)],
            state=directory / "p1-mounted.sav")
        check(exchange[109]["animal_type"] == 0 and exchange[109]["active"] == "0de2" and
              exchange[-1]["animal_type"] == 0x19C and exchange[-1]["active"] == "0e40" and
              exchange[-1]["b"]["s"] == "0009" and exchange[-1]["a"]["s"] == "0000",
              "P2 cannot take the animal after P1 dismounts")
        print("P1-to-P2 animal exchange passed")


if __name__ == "__main__":
    main()
