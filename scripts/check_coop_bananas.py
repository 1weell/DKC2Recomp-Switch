#!/usr/bin/env python3
"""Private banana collection regression from the reported mounted ledges save.

Uses real controller inputs and the shared BCD banana counter. The owner's
external snapshot and all generated traces remain outside the source tree.
"""
import argparse
import hashlib
from pathlib import Path
import tempfile

from check_coop_combat import run_route
from check_coop_route import check


def bananas(row):
    value = row["bananas_bcd"]
    check(value <= 0x99 and (value & 15) < 10, "invalid BCD banana count")
    return (value >> 4) * 10 + (value & 15)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--state", required=True, type=Path)
    args = parser.parse_args()
    runner, rom, state = (p.resolve(strict=True) for p in (args.runner, args.rom, args.state))
    print("state_sha256=" + hashlib.sha256(state.read_bytes()).hexdigest())
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-bananas-") as tmp:
        directory = Path(tmp)

        def replay(name, route, start=state, save=None):
            return run_route(runner, rom, directory, name, route,
                             state=start, save=save, aspect="16:9")

        idle = replay("idle", [(0, 150)])
        check(idle[0]["active"] == "0e40" and idle[0]["animal_type"] == 0x19C and
              idle[0]["a"]["x"] == 4581 and idle[0]["b"]["x"] == 4476 and
              bananas(idle[0]) == 30, "requires the reported mounted banana-trail snapshot")
        check(all(bananas(r) == 30 for r in idle), "distant bananas collected without contact")

        foot_save = directory / "p1-collected.sav"
        p1 = replay("p1-on-foot", [(0x81, 35), (0, 115)], save=foot_save)
        check(bananas(p1[-1]) == 38, "P1 could not collect the eight on-foot bananas")
        check(all(r["b"]["x"] == 4476 and r["b"]["s"] == "0009" for r in p1),
              "on-foot collection moved or dismounted the distant rider")
        print("P1 on foot: 30 -> 38 bananas while P2 remains mounted")

        reversed_save = directory / "p1-rider.sav"
        swapped = replay("exchange", [(0x200000, 1), (0x41, 45), (0, 80), (1, 30), (0, 80)],
                         save=reversed_save)
        check(swapped[-1]["active"] == "0de2" and swapped[-1]["a"]["s"] == "0009" and
              swapped[-1]["b"]["s"] == "0000" and bananas(swapped[-1]) == 30,
              "controller-driven exchange failed or collected the test trail")
        p2 = replay("p2-on-foot", [(0x80000, 70), (0, 80)], start=reversed_save)
        check(bananas(p2[-1]) == 35, "P2 could not collect the five on-foot bananas")
        check(all(r["a"]["x"] == 4475 and r["a"]["s"] == "0009" for r in p2),
              "P2 collection moved or dismounted P1's animal")
        print("P2 on foot after mounted save/load: 30 -> 35 bananas")

        rider = replay("rider", [(0x80000, 70), (0, 80)])
        overlap = replay("overlap", [(0x80000, 70), (0, 80)], start=foot_save)
        check(bananas(rider[-1]) == 43, "rider/animal banana collection regressed")
        check(bananas(overlap[0]) == 38 and bananas(overlap[-1]) == bananas(rider[-1]),
              "revisiting P1's collected bananas awarded them twice or lost remaining pickups")
        check(all(bananas(r) <= 43 for r in overlap), "duplicate banana award")
        print("Rider still collects; overlapping and saved pickups count only once")

        dismounted = replay("dismounted", [(0x200000, 1), (0x81, 35), (0, 115)])
        check(bananas(dismounted[-1]) == 38 and dismounted[-1]["animal_type"] == 0,
              "ordinary two-Kong collection regressed after dismount")
        print("Ordinary unmounted collection passed")


if __name__ == "__main__":
    main()
