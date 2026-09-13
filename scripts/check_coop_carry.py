#!/usr/bin/env python3
"""Private TEAM partner-carry acceptance using real inputs and blank SRAM.

No guest memory is patched. ROM, temporary snapshots and traces stay outside
Git. Optionally repeat with the owner's imported Donkey/Kiddy character pack.
"""
import argparse
import hashlib
from pathlib import Path
import tempfile

from check_coop_combat import run_route
from check_coop_route import TEAM_START, check


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--kongs-pack", type=Path)
    args = parser.parse_args()
    runner, rom = args.runner.resolve(strict=True), args.rom.resolve(strict=True)
    check(hashlib.sha256(rom.read_bytes()).hexdigest() ==
          "35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633",
          "carry route requires the supported headerless USA v1.0 ROM")
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-carry-") as tmp:
        directory = Path(tmp)

        def run(name, route, *, state=None, save=None):
            return run_route(runner, rom, directory, name, route,
                             state=state, save=save, kongs_pack=args.kongs_pack)

        near = directory / "near.sav"
        # Join P2, land, then walk slightly apart: pickup must work without
        # exact coordinate overlap and with either player as the carrier.
        run("join", TEAM_START + [(0x1000, 1), (0, 70), (0x80000, 8), (0, 20)], save=near)
        for who in range(2):
            shift, other_shift = who * 12, (1 - who) * 12
            carrier, passenger = ("a", "b") if who == 0 else ("b", "a")
            slot = 0xE40 if who == 0 else 0xDE2
            name = f"p{who + 1}"
            carried = directory / f"{name}-carried.sav"
            pickup = run(name + "-pickup", [(0x180 << shift, 1), (0x80 << shift, 40),
                         (0x183 << other_shift, 20), (0, 20)], state=near, save=carried)
            check(pickup[0]["held_slot"] == slot, name + ": nearby pickup rejected")
            check(all(row["held_slot"] == slot and row[carrier]["s"] == "0017" and
                      row[passenger]["s"] == "0018" for row in pickup[25:]),
                  name + ": movement/passenger inputs broke carrying")
            check(pickup[-1][carrier]["x"] > pickup[0][carrier]["x"] + 35,
                  name + ": carrier could not walk")
            check(all(abs(row[carrier]["x"] - row[passenger]["x"]) <= 24
                      for row in pickup[25:]), name + ": passenger did not follow carrier")
            jump = run(name + "-carried-jump", [(1 << shift, 1), (0, 60)], state=carried)
            check(all(row["held_slot"] == slot for row in jump) and
                  min(row[carrier]["y"] for row in jump) < jump[0][carrier]["y"] - 20 and
                  jump[-1][carrier]["s"] == "0017" and jump[-1][passenger]["s"] == "0018",
                  name + ": jumping broke carrying")

            # Reloading a carried state must preserve ownership without a
            # host-only pickup record. Drop and throw both restore two players.
            for action, button in (("drop", 0x100), ("throw", 0x82)):
                release = run(name + "-" + action,
                              [(0, 2), (button << shift, 1), (0, 180),
                               (0x80, 20), (0, 20), (0x80000, 20), (0, 20),
                               (0x1001, 1), (0, 40)], state=carried)
                check(release[0]["held_slot"] == slot, name + ": carry lost on load")
                check(release[182]["held_slot"] == 0 and
                      release[182]["a"]["s"] == release[182]["b"]["s"] == "0000",
                      name + ": release left a player waiting")
                check(all(s["flags"] == 0x1E for s in release[182]["sprites"][:2]),
                      name + ": release did not restore player collisions")
                if action == "throw":
                    check(any(row[passenger]["s"] == "001f" for row in release),
                          name + ": native throw flight did not run")
                    check(release[182][passenger]["x"] > release[0][carrier]["x"] + 60,
                          name + ": partner throw did not travel from the carrier")
                check(release[222]["a"]["x"] > release[182]["a"]["x"] + 20 and
                      abs(release[222]["b"]["x"] - release[182]["b"]["x"]) <= 2,
                      name + ": P1 controls not independent after " + action)
                check(release[262]["b"]["x"] > release[222]["b"]["x"] + 20 and
                      abs(release[262]["a"]["x"] - release[222]["a"]["x"]) <= 2,
                      name + ": P2 controls not independent after " + action)
                for actor in ("a", "b"):
                    check(min(row[actor]["y"] for row in release[264:]) <
                          release[262][actor]["y"] - 20,
                          name + ": player cannot jump after " + action)
            print(name + ": nearby pickup, movement, passenger input, save/load, drop/throw and recovery passed")

        far = directory / "far.sav"
        separated = run("separate", [(0x80000, 50), (0, 60)], state=near, save=far)
        check(separated[-1]["b"]["x"] - separated[-1]["a"]["x"] > 90,
              "distance rejection fixture did not separate players")
        for who in range(2):
            denied = run(f"p{who + 1}-far", [(0x100 << (who * 12), 1), (0, 60)], state=far)
            check(all(row["held_slot"] == 0 for row in denied), "pickup worked from too far away")
            check(denied[-1]["active"] == separated[-1]["active"], "rejected pickup changed leader")
            for actor in ("a", "b"):
                check(denied[-1][actor]["x"] == separated[-1][actor]["x"],
                      "rejected pickup moved a player")
        # Physically walk up to that same partner, then retry the same button.
        approach = run("approach", [(0x80, 47), (0, 20), (0x100, 1), (0, 70)], state=far)
        check(approach[-1]["held_slot"] == 0xE40, "walking up did not enable pickup")
        airborne = run("vertical", [(0x1000, 1), (0, 14), (0x100, 1), (0, 50)], state=near)
        check(all(row["held_slot"] == 0 for row in airborne), "airborne partner was pulled into carry")
        simultaneous = run("both-buttons", [(0x100100, 1), (0, 90)], state=near)
        check(simultaneous[-1]["held_slot"] in (0xDE2, 0xE40) and
              {simultaneous[-1]["a"]["s"], simultaneous[-1]["b"]["s"]} == {"0017", "0018"},
              "simultaneous pickup buttons did not produce one carrier")
        print("Distance/height rejection, walking up and simultaneous buttons passed")


if __name__ == "__main__":
    main()
