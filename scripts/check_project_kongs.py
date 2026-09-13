#!/usr/bin/env python3
"""Compare optional Kong rendering against original gameplay using private states."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for arg in ("runner", "rom", "pack", "states", "output"):
        p.add_argument("--" + arg, type=Path, required=True)
    p.add_argument("--frames", type=int, default=120)
    p.add_argument("--input", type=Path)
    p.add_argument("--aspect", choices=("4:3", "16:10", "16:9"), default="16:9")
    p.add_argument("--choices", default="1,2")
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    def compare(state):
        runs = []
        for name, choice in (("original", "0,0"), ("kongs", args.choices)):
            prefix = args.output / (state.stem + "-" + name)
            env = os.environ | {"DKC2_KONGS_PACK": str(args.pack.resolve()),
                "DKC2_KONGS": choice, "DKC2_KONGS_TRACE": "1", "DKC2_ASPECT": args.aspect,
                "DKC2_SAVESTATE_INPUT": str(state.resolve()),
                "DKC2_FRAME_PPM": str(prefix.with_suffix(".ppm").resolve())}
            if args.input:
                env["SNESRECOMP_INPUT_PLAY"] = str(args.input.resolve())
            result = subprocess.run([str(args.runner.resolve()), str(args.rom.resolve()),
                                     str(args.frames)], env=env, text=True, capture_output=True)
            text = result.stdout + result.stderr
            prefix.with_suffix(".log").write_text(text)
            values = dict(re.findall(r"(\w+)=(\w+)", text))
            runs.append((result.returncode, values, text))
        original, modded = runs
        keys = ("wram_sha256", "vram_sha256", "cgram_sha256", "oam_sha256", "audio_fnv1a")
        changed = [key for key in keys if key not in original[1] or
                   original[1][key] != modded[1].get(key)]
        traces = re.findall(r"animation=(\d+).*?matched=(\d+)/(\d+) active=(\d)", modded[2])
        misses = sorted(set((int(a), int(n)) for a, _, n, active in traces
                            if active == "0" and int(n) > 0))
        return {"state": state.name, "return_codes": [original[0], modded[0]],
                "changed_machine_fields": changed,
                "rendered_actor_frames": sum(int(t[3]) for t in traces),
                "unmatched_visible_layouts": misses,
                "picture_changed": original[1].get("frame_sha256") != modded[1].get("frame_sha256")}

    states = sorted(args.states.glob("*.sav"))
    if not states:
        p.error("no private save states found")
    with ThreadPoolExecutor(max_workers=4) as pool:
        rows = list(pool.map(compare, states))
    report = {"frames_per_run": args.frames, "states": len(rows), "results": rows}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    failed = [r for r in rows if any(r["return_codes"]) or r["changed_machine_fields"]]
    unmatched = [r for r in rows if r["unmatched_visible_layouts"]]
    print(f"states={len(rows)} machine_failures={len(failed)} unmatched_states={len(unmatched)}")
    for row in failed + unmatched:
        print(json.dumps(row))
    raise SystemExit(bool(failed or unmatched or
                          not sum(r["rendered_actor_frames"] for r in rows)))


if __name__ == "__main__":
    main()
