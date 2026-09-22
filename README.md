# DKC2Recomp — Nintendo Switch port

Nintendo Switch port of [DKC2Recomp](https://github.com/elliotttate/DKC2Recomp), based on the original project's static recompiler and SNES runtime.

This repository contains the Switch host, build configuration, and documentation required to run the game on Nintendo Switch.

## Current status

The build has been tested on real hardware and provides:

- game boot and execution;
- working Switch controls;
- correct physical B/Y/A/X button mapping;
- SNES audio through SDL/libnx;
- 16:9 presentation at 1280×720;
- persistent SRAM data under .runtime/.

The widescreen route is still being validated screen by screen. Small transition artifacts may remain in some intro scenes.

## Requirements

Install the following to build the port:

- [devkitPro](https://devkitpro.org/);
- devkitA64;
- libnx;
- SDL2 for Switch;
- CMake, Ninja, Python, and Rust/Cargo;
- devkitPro MSYS2;
- your own legally obtained North American v1.0 ROM of *Donkey Kong Country 2: Diddy's Kong Quest*.

The project expects:

~~~
export DEVKITPRO=/c/devkitPro
export DEVKITA64=/c/devkitPro/devkitA64
~~~

## Supported ROM

The ROM is not distributed in this repository. Generation verifies the North American v1.0 revision using this SHA-256:

~~~
35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633
~~~

Place the ROM in the project root or pass its path directly to the generator.

## Building

Run these commands from the devkitPro MSYS2 shell:

~~~
export DEVKITPRO=/c/devkitPro
export DEVKITA64=/c/devkitPro/devkitA64

python3 scripts/generate_snesrecomp.py --rom /c/path/to/DKC2-USA-v1.0.sfc

cmake -S . -B build-switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/switch-devkitA64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DDKC2_BUILD_SNESRECOMP_SWITCH=ON

cmake --build build-switch --parallel 16
~~~

The main artifact is generated at:

~~~
build-switch/DKC2RecompSwitch.nro
~~~

The build uses 16 parallel jobs, matching the development environment used for this port.

## Installing on Switch

Copy the following files to the SD card:

~~~
/switch/DKC2Recomp/DKC2RecompSwitch.nro
/switch/DKC2Recomp/DKC2-USA-v1.0.sfc
~~~

Launch the .nro through the Homebrew Menu.

The game creates and uses persistent data under:

~~~
/switch/DKC2Recomp/.runtime/
~~~

Do not publish the ROM or personal save files with the port.

## Controls

The mapping follows the physical position of the Switch controller buttons:

| Switch control | SNES button |
| --- | --- |
| B | B |
| Y | Y |
| A | A |
| X | X |
| + | Start |
| − | Select |
| L / R | L / R |
| D-pad | D-pad |

## Widescreen and video

The port enables the widescreen route already present in the original project. Rendering uses:

- a 342×224 logical framebuffer;
- 16:9 expansion with the game's additional side margins;
- 1280×720 output presentation;
- nearest-neighbor scaling to preserve the original pixel artwork.

Diagnostics and validation for BG1, BG2, BG3, and OBJ layers are documented in [docs/WIDESCREEN_DIAGNOSTICS.md](docs/WIDESCREEN_DIAGNOSTICS.md).

## Audio

Audio uses the SNES runtime DSP callback and the Switch SDL output. Initialization requests stereo S16 audio at 32,040 Hz, the game's native rate.

## Port-specific files

- runner/switch_main.c — main host, video, input, and game loop;
- runner/switch_host.c / runner/switch_compat.c — host compatibility;
- runner/switch_music.c — host music support;
- cmake/toolchains/switch-devkitA64.cmake — devkitA64 toolchain;
- BUILDING_SWITCH.md — detailed build and installation workflow;
- PORT_STATUS.md — current status and remaining work;
- PORTING_WORKLOG.md — technical porting history.

Generated files, local builds, ROMs, and logs are ignored by Git and are not part of the public repository.

## Credits and acknowledgements

- [Elliott Tate — DKC2Recomp](https://github.com/elliotttate/DKC2Recomp), the original project and foundation for this port;
- [snesrecomp](https://github.com/mstan/snesrecomp), the static recompiler and shared SNES runtime;
- [recomp-ui](https://github.com/mstan/recomp-ui), shared components from the original project;
- [H4v0c21 — DKC2 disassembly](https://github.com/H4v0c21/DKC2-disassembly), reverse-engineering and symbol reference;
- devkitPro, devkitA64, libnx, and SDL2, the tools and libraries used by the port;
- the game-recompilation and game-preservation communities.

The original *Donkey Kong Country 2: Diddy's Kong Quest* content belongs to its respective rights holders. The ROM must be provided by the user and is not distributed with this project.

See the license files included with each submodule and dependency for the applicable terms.
