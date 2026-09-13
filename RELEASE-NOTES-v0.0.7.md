# DKC2Recomp v0.0.7 - Windows + macOS

The Windows build adds simultaneous two-player TEAM mode, complete CRT and
Reconstruct controls, optional Donkey/Kiddy characters, and MSU-1 replacement
music. The download also includes the latest published **macOS arm64 v0.0.6**
archive unchanged. The Mac binary predates the new co-op and MSU-1 integration.

## Windows changes

- P1 keyboard and P2 gamepad can play together in TEAM mode. Fixes cover
  attacks, barrel ownership, damage/revival, Rambi mounting, death handoff,
  widescreen movement and banana collection while the other Kong is mounted.
- Game/View/Input dropdowns and the Escape pause menu support live settings
  and input selection. GDI now supports the ImGui overlay; native OpenGL
  supports all five Reconstruct modes and three tuning sliders.
- CRT TV includes Living room, Studio monitor, Soft and Custom presets,
  four mask choices, scanlines, sharpness, mask strength, glow, halation and
  curvature. WGL and SDL share the rendering passes.
- Escape > Characters selects Donkey/Kiddy from a private Project Kongs pack.
  Both slots retain independent attack/throw state in co-op. The importer and
  setup instructions are included; character art is supplied separately.
- Escape > Settings > Audio selects an external MSU-1 PCM folder with live
  enable/disable and music volume. Original sound effects remain, missing
  tracks fall back to SNES music, and save/rewind restarts the restored song.
- Preserves v0.0.6 source changes, including SDL audio rate control, Mac
  display pacing, the Retina pause-menu fix and save unlock tools.

## Downloads

- **Windows x64:** extract the complete `DKC2Recomp-v0.0.7-Windows-x64.zip`
  and run `DKC2Recomp.exe`. `DKC2RecompSDL.exe` is the alternate SDL host.
  Keep the bundled DLLs and assets beside the executables. Windows 10/11
  supplies the Universal C Runtime used by the app-local MSVC libraries.
- **macOS arm64:** `DKC2Recomp-v0.0.6-macOS-arm64.zip` is copied byte-for-byte
  from [v0.0.6](https://github.com/elliotttate/DKC2Recomp/releases/tag/v0.0.6).
  It retains its original filename, embedded version and ad-hoc signature.
  It has not been rebuilt or run on a Mac for this release.

Supply your own headerless USA v1.0 ROM (SHA-256
`35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633`).
ROMs, saves, character art, music packs and personal settings are excluded.
See the included `docs/PROJECT_KONGS.md` and `docs/MSU1_AUDIO.md` for setup.

## Validation and limits

The feature milestone passed 80/80 configured tests. GPU comparisons covered
40 flat-display settings and 13 CRT variations. Private tests verified the
102-track restoration pack headers, changed music with identical gameplay and
video hashes, preserved sound effects, and exact missing-track fallback.
Donkey/Kiddy co-op barrel replays checked origins, direction and recovery.
Live checks confirmed Escape, music switching and Kiddy on Rambi alongside
Donkey.

The final v0.0.7 Release build passed **84/84 CTest checks** in 194.47 seconds,
including both packaging layouts and the merged audio/pacing tests. A fresh
Windows ZIP extraction completed four 180-frame runs: each desktop host with
CRT and with Reconstruct. The SDL runs loaded the private mounted scene and
selected MSU-1 track 6. Executable hashes matched the build, private files were
absent from the archive, and the clean first launch displayed the ROM picker.

Full-game co-op, all replacement poses, every music transition, physical
rumble feel and non-Windows builds of the new source remain unverified.
Replacement characters retain original DKC2 collision boxes and some movement
rules; this is not a complete DKC1/DKC3 mechanics transplant.

Carried Mac archive SHA-256:
`20947ee9ee4bc64df13b2aeae68440288811f8e6b5b263bc8c4c8258352a6094`.
Its original executable digest and embedded version were checked against the
published v0.0.6 metadata; the original release assets remain in place.
