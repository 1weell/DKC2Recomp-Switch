# DKC2Recomp — Nintendo Switch port

This directory is the isolated Switch port workspace for DKC2Recomp. The
source game is a SNES recompilation, so the first port milestone uses the
shared `snesrecomp` runtime plus SDL2's Switch renderer. It does not reuse
Wave Race's N64/RT64/Vulkan path.

## Repository boundary

- `snesrecomp/` and `recomp-ui/` are upstream submodules.
- `generated/snesrecomp/` is generated from the user's verified ROM and stays
  ignored; no ROM or generated game C belongs in Git.
- `runner/switch_main.c` is the Switch host. It uses the existing 16:9 path,
  a 342x224 logical framebuffer, 1280x720 presentation, nearest-neighbor
  scaling, and the SDL DSP audio callback.
- `runner/switch_host.c` supplies Switch-safe logging and lifecycle glue.
- `runner/switch_main.c` opens a 32,040 Hz stereo SDL audio device and feeds it
  from the shared SNES DSP ring through `RtlRenderAudio`.
- `cmake/toolchains/switch-devkitA64.cmake` selects devkitA64/libnx.

## Required local inputs

The supported private ROM is North American v1.0, 4 MiB, SHA-256
`35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633`.
Do not copy it into this repository.

Install devkitPro with devkitA64, libnx, SDL2 for Switch, `nacptool` and
`elf2nro`. Python and Cargo are needed to generate the private recompilation
units.

## Generate private game code

```sh
python3 scripts/generate_snesrecomp.py \
  --rom /absolute/path/to/DKC2-USA-v1.0.sfc
```

The output is `generated/snesrecomp/`. The generator validates the ROM before
writing and applies the repository's DKC2 callbacks and video overrides.

If the generator reports `Unsupported ROM SHA-256`, the file is not the exact
North American v1.0 dump required by this port. Supply a matching dump instead
of changing the expected hash; the generated addresses and symbols depend on
that exact revision.

## Configure and build

Run from a devkitPro MSYS2 shell:

```sh
export DEVKITPRO=/c/devkitPro
export DEVKITA64=/c/devkitPro/devkitA64
export TMP=/c/Users/Paulo/wineNX/tmp
export TEMP=/c/Users/Paulo/wineNX/tmp
export TMPDIR=/c/Users/Paulo/wineNX/tmp

cmake -S . -B build-switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/switch-devkitA64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DDKC2_BUILD_SNESRECOMP_SWITCH=ON
cmake --build build-switch --parallel 16
```

The expected artifacts are:

```text
build-switch/DKC2RecompSwitch
build-switch/DKC2RecompSwitch.nacp
build-switch/DKC2RecompSwitch.nro
```

## SD layout

```text
sdmc:/switch/DKC2Recomp/
  DKC2RecompSwitch.nro
  DKC2-USA-v1.0.sfc
  .runtime/
    save.srm
    save.srm.bak
    settings.bin
    boot.log
    boot.previous.log
```

The NRO also accepts a ROM path argument during development, but the default
path is the one shown above.

## Milestones

1. Build and link the Switch host with the private generated sources.
2. Boot with a verified ROM and record `boot.log`.
3. Reach a 16:9 SDL presentation and first presented PPU frame.
4. Verify Npad Player 1 and Player 2 input.
5. Validate the SDL/libnx SPC audio consumer on hardware.
6. Validate SRAM, reset, shutdown and long-session stability on hardware.
7. Measure CPU frame time before attempting aggressive compiler optimization.

A successful ELF/NRO link is not proof of Switch runtime compatibility. Each
graphics, input and audio change must be tested on hardware with the prior NRO
preserved for rollback.

## Candidate verification and host tests

The menu additionally links SDL2_test from the installed switch-sdl2 package.
After a successful cross-build, record the exact candidate:

```sh
python3 scripts/check_switch_build.py --build build-switch
python3 tests/test_check_switch_build.py
python3 tests/test_switch_reporter.py
```

Keep `build-switch/switch-build-manifest.json` with the tested NRO and preserve
an older NRO separately. The manifest includes dirty state and source hashes;
run it after rebuilding every source change. It validates ELF/NRO headers,
NACP size and NRO bounds; it does not execute the game.

Public host policies/storage are tested in a separate native directory. For
the installed MSYS GCC environment used in this checkpoint:

```sh
cmake -S . -B build-roadmap-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-D_GNU_SOURCE
cmake --build build-roadmap-tests --parallel 16
ctest --test-dir build-roadmap-tests --output-on-failure
```

`-D_GNU_SOURCE` exposes the existing POSIX launcher APIs on MSYS; do not add it
to the Switch toolchain. Private ROM regressions require their own configured
fixtures. See docs/SWITCH_IMPLEMENTATION_2026-09-22.md for the hardware matrix.
