"""Create synthetic PCM only, then exercise the public native music interfaces."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="DKC2-msu-test-") as name:
    folder = Path(name)
    for track, loop, values in ((1, 1, [1000, 2000, 3000]),
                                (17, 0, [7000, 8000]), (41, 0, [4000, 5000])):
        data = b"MSU1" + struct.pack("<I", loop)
        data += b"".join(struct.pack("<hh", value, -value) for value in values)
        (folder / f"dkc2_msu1-{track}.pcm").write_bytes(data)
    (folder / "dkc2_msu1-2.pcm").write_bytes(b"bad file")
    subprocess.run([str(Path(sys.argv[1]).resolve()), name], cwd=name, check=True)
