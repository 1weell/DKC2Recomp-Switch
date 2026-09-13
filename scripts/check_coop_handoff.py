#!/usr/bin/env python3
"""Private blank-SRAM P1 death, P2 position and world-resumption acceptance."""
import argparse
from pathlib import Path
import tempfile

from check_coop_combat import run_route
from check_coop_route import TEAM_START, check


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    args = parser.parse_args()
    runner, rom = args.runner.resolve(strict=True), args.rom.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-handoff-") as tmp:
        directory = Path(tmp)
        data = run_route(runner, rom, directory, "handoff",
            TEAM_START + [(0x40000, 1), (0, 1), (0x80, 360), (0, 180), (0x80000, 50)],
            save=directory / "survivor.sav")
        hurt = next(r for r in data[3020:] if r["a"]["s"] == "0024")
        switched = next(r for r in data[hurt["frame"]:] if r["active"] == "0e40")
        check(abs(switched["b"]["x"] - hurt["b"]["x"]) <= 2 and
              abs(switched["b"]["y"] - hurt["b"]["y"]) <= 2,
              "surviving P2 moved to dying P1's position")
        resumed = data[switched["frame"]+1:]
        check(all(r["time_freeze"] != 7 for r in resumed), "turn freeze persists after handoff")
        enemy_x = {s["x"] for r in resumed[:30] for s in r["sprites"][2:]
                   if s["id"] == 0x1E4 and s["state"] == 0}
        check(len(enemy_x) >= 15, "enemy simulation did not resume")
        check(data[-1]["b"]["x"] - data[-51]["b"]["x"] > 60,
              "P2 cannot move after P1 dies")
        check(data[-1]["a"]["s"] == "0013" and data[-1]["sprites"][0]["flags"] == 0,
              "dead P1 rejoined or remained interactive")
        restored = run_route(runner, rom, directory, "reload", [(0x41000, 45)],
                             state=directory / "survivor.sav")
        check(all(r["time_freeze"] != 7 and r["a"]["s"] == "0013" for r in restored),
              "save/load restored a frozen world or revived P1")
        print("P1 death passed: P2 stays in place, enemies resume, P2 moves, loss survives save/load")


if __name__ == "__main__":
    main()
