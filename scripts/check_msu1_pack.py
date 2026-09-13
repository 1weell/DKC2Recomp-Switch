"""Private MSU-1 acceptance using an external ROM, scene and extracted pack."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("runner", "rom", "state", "pack", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("DKC2_", "SNESRECOMP_"))}
    paths = sorted(args.pack.glob("dkc2_msu1-*.pcm"))
    assert paths, "no DKC2 MSU-1 tracks"
    for path in paths:
        with path.open("rb") as f:
            header = f.read(8)
        assert len(header) == 8 and header[:4] == b"MSU1", path
        assert path.stat().st_size >= 12 and (path.stat().st_size - 8) % 4 == 0, path
    rows = {}
    with tempfile.TemporaryDirectory(prefix="DKC2-msu-fallback-") as name:
        missing = Path(name)
        (missing / "dkc2_msu1-1.pcm").write_bytes(b"MSU1" + struct.pack("<Ihh", 0, 10, -10))
        inputs = args.output / "music.input"
        inputs.write_text("000000*30\n001000*1\n000000*50\n000080*90\n000081*1\n000000*68\n")
        for label, pack, gain in (("snes", "", 100), ("msu", str(args.pack.resolve()), 100),
                                  ("sfx-only", str(args.pack.resolve()), 0),
                                  ("fallback", str(missing), 100)):
            run_env = env | {"DKC2_MSU1_PATH": pack, "DKC2_MSU1_VOLUME": str(gain),
                "DKC2_MSU1_TRACE": "1", "DKC2_SAVESTATE_INPUT": str(args.state.resolve()),
                "SNESRECOMP_INPUT_PLAY": str(inputs.resolve())}
            run = subprocess.run([str(args.runner.resolve()), str(args.rom.resolve()), "240"],
                                 cwd=missing, env=run_env, text=True, capture_output=True, timeout=60)
            output = run.stdout + run.stderr
            (args.output / (label + ".log")).write_text(output)
            assert run.returncode == 0, output
            rows[label] = dict(re.findall(r"(\w+)=([a-zA-Z0-9]+)(?:\s|$)", output))
            if label == "msu": assert "music: MSU-1 track " in output
        for label in rows:
            for key in ("frame_sha256", "wram_sha256", "vram_sha256", "cgram_sha256", "oam_sha256"):
                assert rows[label][key] == rows["snes"][key], (label, key)
        assert rows["snes"]["audio_fnv1a"] == rows["fallback"]["audio_fnv1a"]
        assert rows["msu"]["audio_fnv1a"] != rows["snes"]["audio_fnv1a"]
        assert int(rows["sfx-only"]["audio_nonzero_samples"]) > 0, "no stock effects heard during route"
    report = {"tracks": len(paths), "state_sha256": hashlib.sha256(args.state.read_bytes()).hexdigest(),
              "results": rows}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{len(paths)} valid tracks; gameplay unchanged, replacement music, stock SFX and missing-track fallback passed")


if __name__ == "__main__":
    main()
