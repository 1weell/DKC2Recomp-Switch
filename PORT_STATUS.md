# DKC2Recomp Switch port status

## Current checkpoint

- Repository cloned with the upstream `snesrecomp` and `recomp-ui` submodules.
- Switch-specific CMake target added, producing an `.nro` through `nacptool`
  and `elf2nro`.
- Switch host added with SDL2 presentation, libnx `PadState` input, SRAM
  directory setup, frame pacing, and a small `boot.log` reporter.
- Host audio now opens a 32,040 Hz stereo SDL device and consumes the shared
  SNES DSP output through `RtlRenderAudio`.
- Face-button mapping was corrected after the first hardware test: Switch
  B/Y/A/X now map to the physical SNES B/Y/A/X positions.
- Switch presentation now enables the existing 16:9 path with a 342x224
  logical framebuffer scaled to a 1280x720 output.
- Desktop and headless hosts are excluded from the Switch configuration so the
  cross-build does not pull in Windows or desktop-only code.
- The installed devkitPro ARM compiler successfully parses both Switch host
  translation units.

## ROM and generated code

The verified ROM at the repository root is ignored by Git as intended. It is
4,194,304 bytes and has the exact North American v1.0 SHA-256 required by the
upstream generator:

```text
35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633
```

The generator completed successfully with 3,323 roots, 3,474 exact AOT
variants, 12 generated banks, and no LLE variants. The generated C files are
private build artifacts and remain ignored by Git.

## Build checkpoint

The Switch build completed with 16 parallel jobs and produced:

- `build-switch/DKC2RecompSwitch`
- `build-switch/DKC2RecompSwitch.nacp`
- `build-switch/DKC2RecompSwitch.nro`

Warnings remain in upstream/generated PPU code, but there were no compile or
link errors. The first pre-audio/pre-input-fix `.nro` was tested successfully
on hardware: it booted, displayed the game, and accepted controls. That test
found the ABXY mismatch, missing audio, and a small intro glitch. The current
`.nro` contains the ABXY and audio fixes; the intro glitch still needs a
reference capture for diagnosis.

## Next steps

1. Copy the `.nro` and the verified ROM to the documented SD-card layout.
2. Test video, controller input, SRAM, and frame pacing on hardware or an
   emulator.
3. Verify the SDL2/libnx audio queue and APU output on hardware.
4. Run the documented widescreen diagnostics on hardware captures and check
   the known intro/margin transitions before treating 16:9 as stable.
