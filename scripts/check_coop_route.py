#!/usr/bin/env python3
"""Verify new-TEAM input routing using an external supported ROM.

No save seed is required. All replay/trace files live in TemporaryDirectory.
The route deliberately waits for title, file, mode and map transitions before
testing P1 alone, P2 alone (including landing), and both players together.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

TEAM_START = [(0, 900), (8, 8), (0, 188), (8, 8), (0, 190),
              (0x20, 8), (0, 42), (8, 8), (0, 900),
              (0x100, 8), (0, 300), (0x100, 8), (0, 450)]
SEPARATE_PLAYERS = [(0x81, 120), (0, 40), (0x81000, 120), (0, 40)]


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--rom", required=True, type=Path)
    args = parser.parse_args()
    runner, rom = args.runner.resolve(strict=True), args.rom.resolve(strict=True)
    route = TEAM_START + SEPARATE_PLAYERS + [(0x81081, 120), (0, 60)]
    frames = sum(duration for _, duration in route)
    with tempfile.TemporaryDirectory(prefix="dkc2-coop-") as directory:
        directory = Path(directory)
        inputs = directory / "team.input"
        inputs.write_text("\n".join(f"{value:06x}*{duration}"
                                    for value, duration in route), encoding="ascii")
        runs = {}
        for mode in ("classic", "simultaneous"):
            trace = directory / f"{mode}.jsonl"
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith(("DKC2_", "SNESRECOMP_"))}
            env.update(DKC2_COOP=mode, DKC2_COOP_TRACE=str(trace),
                       SNESRECOMP_INPUT_PLAY=str(inputs))
            result = subprocess.run([str(runner), str(rom), str(frames)],
                                    cwd=directory, env=env, capture_output=True,
                                    text=True, timeout=90)
            check(result.returncode == 0, result.stdout + result.stderr)
            data = [json.loads(line) for line in trace.read_text().splitlines()]
            check(len(data) == frames, f"{mode}: incomplete trace")
            check(all(row["mode"] == 1 and row["sub"] == "06"
                      for row in data[3017:]), f"{mode}: route did not reach TEAM gameplay")
            runs[mode] = data
        data = runs["simultaneous"]
        check(data[3137]["a"]["x"] - data[3017]["a"]["x"] > 150,
              "P1 did not move its own Kong")
        check(data[3137]["b"]["x"] == data[3017]["b"]["x"],
              "P1 unexpectedly controlled the waiting P2 Kong")
        check(data[3297]["a"]["x"] == data[3177]["a"]["x"],
              "P2 unexpectedly moved P1")
        check(data[3297]["b"]["x"] - data[3178]["b"]["x"] > 150,
              "P2 did not retain independent movement")
        check(data[3297]["b"]["x"] - data[3248]["b"]["x"] > 40,
              "P2 lost control after landing")
        for slot in ("a", "b"):
            check(data[3457][slot]["x"] - data[3337][slot]["x"] > 150,
                  f"simultaneous movement failed for {slot}")
        # Classic ignores P2 while P1 remains active; it must not produce
        # the independent P2 progress seen with the new policy.
        classic = runs["classic"]
        check(classic[3297]["b"]["x"] < data[3297]["b"]["x"] - 100,
              "classic and simultaneous P2 behavior did not differ")
        print(f"TEAM route passed: {frames} frames per policy; P1, P2, landing and simultaneous movement")


if __name__ == "__main__":
    main()
