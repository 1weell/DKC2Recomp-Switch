#!/usr/bin/env python3
"""Private Topsail Trouble rope regression from the reported external snapshot.

All movement uses real controller inputs. No guest memory is patched, and no
ROM, snapshot, trace or character assets are stored in the repository.
"""
import argparse
import hashlib
from pathlib import Path
import tempfile

from check_coop_combat import run_route
from check_coop_route import check


def check_junction(run, state, directory):
    check(hashlib.sha256(state.read_bytes()).hexdigest() ==
          "d3fa341c257601fab83ef8ba1ec2adc53516b2766b83585f4a92bb0e9e65f245",
          "junction route requires the reported stuck rope-transition snapshot")
    raised, below, transition = (directory / name for name in
                                 ("raised.sav", "below.sav", "transition.sav"))
    idle = run("junction-resume", [(0, 30)], state)
    check(all(row["b"] == {"x": 947, "y": 2480, "s": "0037"} for row in idle),
          "saved junction did not complete in place")
    up = run("junction-up", [(0x10000, 40), (0, 40)], state, raised)
    check(up[35]["b"]["y"] < up[0]["b"]["y"] - 25 and
          up[-1]["b"]["s"] == "0037", "P2 cannot climb the double ropes")
    check(all(row["sprites"][1]["animation"] == 0xDA for row in up[10:35]) and
          len({row["sprites"][1]["graphic"] for row in up[10:35]}) >= 5 and
          up[-1]["sprites"][1]["animation"] == 0xD9,
          "double-rope Up does not animate or settle to its hanging pose")
    down = run("junction-down", [(0x20000, 70), (0, 40)], raised, below)
    check(all(row["sprites"][1]["animation"] == 0xDB for row in down[:20]) and
          len({row["sprites"][1]["graphic"] for row in down[:25]}) >= 4,
          "double-rope Down does not animate")
    check(any(row["b"]["s"] == "0036" for row in down) and
          down[-1]["b"]["s"] == "0035" and
          down[-1]["b"]["y"] > 2520, "P2 cannot descend through the junction")
    # Save/load in an unfinished transition must retain its remaining frames.
    partial = run("junction-save", [(0x20000, 8)], state, transition)
    check(partial[-1]["b"]["s"] == "0036", "route missed the transition")
    restored = run("junction-reload", [(0x20000, 30), (0, 20)], transition)
    check(restored[-1]["b"]["s"] == "0035" and
          restored[-1]["b"]["y"] > 2490, "unfinished saved transition got stuck")
    routes = [idle, up, down, partial, restored]
    for name, button, x in (("left", 0x40000, 947), ("right", 0x80000, 979)):
        sideways = run("junction-" + name, [(button, 25), (0, 30)], state)
        check(any(row["b"]["s"] == "0036" for row in sideways) and
              sideways[-1]["b"] == {"x": x, "y": 2480, "s": "0035"},
              "P2 cannot leave the double ropes to the " + name)
        routes.append(sideways)
    # Hold across the entire net, reverse and reach its other outer rope.
    # One-column motion is insufficient: a skipped facing callback used to
    # leave P2 looping the turn animation at the middle column ($03D3).
    edges = run("junction-edges", [(0x80000, 150), (0, 10),
                                   (0x40000, 210), (0, 30)], state)
    check(edges[140]["b"] == {"x": 1011, "y": 2480, "s": "0035"} and
          edges[-1]["b"] == {"x": 947, "y": 2480, "s": "0035"},
          "P2 cannot cross to both outer ropes after turning")
    check(edges[140]["sprites"][1]["animation"] == 0xD2 and
          edges[-1]["sprites"][1]["animation"] == 0xD2,
          "P2 loops its turn instead of hanging at the outer ropes")
    routes.append(edges)
    check(all(row["a"] == {"x": 947, "y": 2508, "s": "0035"} and
              row["active"] == "0de2" for rows in routes for row in rows),
          "P2 junction movement affected P1 or camera ownership")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--state", required=True, type=Path)
    parser.add_argument("--junction-state", type=Path)
    parser.add_argument("--kongs-pack", type=Path)
    args = parser.parse_args()
    runner, rom, state = (p.resolve(strict=True) for p in
                          (args.runner, args.rom, args.state))
    check(hashlib.sha256(rom.read_bytes()).hexdigest() ==
          "35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633",
          "rope route requires the supported headerless USA v1.0 ROM")
    check(hashlib.sha256(state.read_bytes()).hexdigest() ==
          "725807dad843865b979cc6f8c14a98eccd0adfd3549c63897ca257c38c3497d2",
          "rope route requires the reported Topsail Trouble snapshot")
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-ropes-") as tmp:
        directory = Path(tmp)

        def run(name, route, source, save=None):
            return run_route(runner, rom, directory, name, route, state=source,
                             save=save, kongs_pack=args.kongs_pack, submode="08")

        resumed, grabbed = directory / "resumed.sav", directory / "grabbed.sav"
        # The reported checkpoint is paused. Vertical stages use submode $08
        # during gameplay too; the actual pause bit is $0040 in kong_flags.
        ready = run("resume", [(0, 5), (8, 8), (0, 80)], state, resumed)
        check(ready[0]["kong_flags"] & 0x40 and not ready[-1]["kong_flags"] & 0x40,
              "snapshot did not resume")
        check(ready[-1]["a"] == {"x": 947, "y": 2645, "s": "0035"} and
              ready[-1]["b"]["x"] == 983, "unexpected starting rope positions")

        # P2 jumps left onto P1's vertical rope, releases Left and climbs Up.
        climb = run("p2-grab", [(0x51000, 20), (0x10000, 100), (0, 30)], resumed, grabbed)
        attachments = [i for i, row in enumerate(climb) if row["b"]["s"] == "0035"]
        check(attachments and attachments[0] < 40, "P2 did not grab the rope")
        check(all(row["b"]["s"] == "0035" for row in climb[attachments[0]:]),
              "P2 lost the rope during climbing or idle")
        check(climb[-1]["b"]["y"] < climb[attachments[0]]["b"]["y"] - 80,
              "P2 attached but could not climb")
        check(all(row["sprites"][1]["animation"] == 0xD3 for row in climb[40:120]) and
              len({row["sprites"][1]["graphic"] for row in climb[40:120]}) >= 6,
              "P2 floats upward without cycling its climbing frames")
        check(all(row["a"] == ready[-1]["a"] and row["active"] == "0de2" for row in climb),
              "P2 contact moved P1 or changed camera ownership")

        # Reload a save with both players on the rope, then use opposing inputs.
        opposite = run("opposite", [(0x20010, 40), (0, 20)], grabbed)
        check(all(row[actor]["s"] == "0035" for row in opposite for actor in ("a", "b")),
              "rope attachment lost after save/load")
        check(opposite[40]["a"]["y"] < opposite[0]["a"]["y"] - 40 and
              opposite[40]["b"]["y"] > opposite[0]["b"]["y"] + 50,
              "players could not climb in opposite directions")
        for who, animation in ((0, 0x30), (1, 0xD4)):
            check(all(row["sprites"][who]["animation"] == animation for row in opposite[10:40]) and
                  len({row["sprites"][who]["graphic"] for row in opposite[10:40]}) >= 6,
                  f"P{who + 1} does not animate in its climbing direction")

        # Jump right to the next rope. This checks native release, flight,
        # terrain contact and reattachment without disturbing P1's rope state.
        jump = run("neighbor-rope", [(0x81000, 12), (0, 50)], grabbed)
        check(any(row["b"]["s"] == "0006" for row in jump), "P2 could not jump off")
        check(jump[-1]["b"]["s"] == "0035" and
              jump[-1]["b"]["x"] > jump[0]["b"]["x"] + 20 and
              jump[-1]["b"]["y"] < jump[0]["b"]["y"] - 40,
              "P2 could not attach to the neighboring rope")
        check(all(row["a"] == ready[-1]["a"] for row in jump),
              "P2 rope jump affected P1")
        if args.junction_state:
            check_junction(run, args.junction_state.resolve(strict=True), directory)
    print("Rope regression passed: grab/climb, animated Up/Down, independent controls, save/load, jump and regrab")
    if args.junction_state:
        print("Junction regression passed: stuck-save recovery, animated double-rope Up/Down, full net traversal, both exits and transition save/load")


if __name__ == "__main__":
    main()
