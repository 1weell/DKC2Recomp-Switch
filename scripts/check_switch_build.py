#!/usr/bin/env python3
"""Check Switch artifacts and record a source/build manifest (no ROM included)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import datetime


def command(args, root):
    try:
        result = subprocess.run(args, cwd=root, text=True, capture_output=True, timeout=60)
        return result.stdout.strip() if result.returncode == 0 else "unavailable: " + result.stderr.strip()
    except (OSError, subprocess.TimeoutExpired) as error:
        return "unavailable: " + str(error)


def inspect_artifacts(build):
    result = {}
    for name in ("DKC2RecompSwitch", "DKC2RecompSwitch.nacp", "DKC2RecompSwitch.nro"):
        data = (build / name).read_bytes()
        if name.endswith(".nro"):
            if len(data) < 128 or data[16:20] != b"NRO0":
                raise ValueError("Invalid NRO header")
            code_size = int.from_bytes(data[24:28], "little")
            if code_size < 128 or code_size > len(data):
                raise ValueError("Truncated NRO")
        elif name.endswith(".nacp"):
            if len(data) != 0x4000:
                raise ValueError("Invalid NACP size")
        elif data[:4] != b"\x7fELF":
            raise ValueError("Invalid ELF header")
        result[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build-switch"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    artifacts = inspect_artifacts(build)
    cache = (build / "CMakeCache.txt").read_text()
    options = {}
    for line in cache.splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key.startswith(("CMAKE_BUILD_TYPE:", "CMAKE_C_COMPILER:", "CMAKE_C_FLAGS", "CMAKE_TOOLCHAIN_FILE:", "DKC2_BUILD_", "SNESRECOMP_")):
            options[key] = value
    sources = {}
    for pattern in ("runner/switch*", "CMakeLists.txt", "cmake/toolchains/switch*", "tests/test_switch_host.c"):
        for path in sorted(root.glob(pattern)):
            if path.is_file():
                sources[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    compiler = next((v for k, v in options.items() if k.startswith("CMAKE_C_COMPILER:")), "aarch64-none-elf-gcc")
    manifest = {
        "recorded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "revision": command(["git", "rev-parse", "HEAD"], root),
        "submodules": command(["git", "submodule", "status"], root),
        "working_tree": command(["git", "status", "--short", "--untracked-files=normal"], root),
        "compiler": command([compiler, "--version"], root),
        "packages": command(["pacman", "-Q", "devkitA64", "libnx", "switch-sdl2"], root),
        "cmake_options": options, "source_sha256": sources, "artifacts": artifacts,
        "hardware_validation": "not performed; compilation is not console acceptance",
    }
    output = build / "switch-build-manifest.json"
    output.write_text(json.dumps(manifest, indent=2) + "\n")
    print(output)
    print("NRO SHA-256: " + artifacts["DKC2RecompSwitch.nro"]["sha256"])


if __name__ == "__main__":
    main()
