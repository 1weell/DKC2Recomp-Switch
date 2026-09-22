# Switch implementation checkpoint — 2026-09-22

This is an implementation and local verification checkpoint, not acceptance
of packages A/B on Nintendo Switch. No console was connected for this work.
The intro glitch, handheld/dock comparison, audio listening tests and the
60-minute hardware session remain pending. Existing README/PORT_STATUS edits
and the user's roadmap were retained.

## Baseline and tools

- Port revision: `5f93d93ada7123ea64c00efbd99799db059b7349`, with working-tree edits.
- snesrecomp: `3a929cd30336f2b3a077df17912719be63142299`.
- recomp-ui: `ad2f3e293c6641c93ee69963dd669661f3e40290`.
- devkitA64 r30-1, libnx 4.12.0-1, switch-sdl2 2.28.5-4.
- Baseline NRO SHA-256: `6c53803b35571eaf62d47c07c21856f13b400be709e117d99690c0610e61b2ae`.
- Baseline preserved privately at `tmp/roadmap-baseline/DKC2RecompSwitch.nro`.
- Candidate: `build-switch/DKC2RecompSwitch.nro`. Generate/check its manifest
  with `python3 scripts/check_switch_build.py --build build-switch` after building.
  The manifest records artifact hashes, source hashes, dirty state, compiler,
  packages and relevant CMake options. Keep it with the candidate being tested.
- Existing NACP application version is unchanged; distinguish candidates using
  the NRO hash and manifest, not the displayed application version alone.

Final local cross-build completed after linking SDL2_test for the menu:

- `build-switch/DKC2RecompSwitch` (32,120,848 bytes)
- `build-switch/DKC2RecompSwitch.nacp` (16,384 bytes)
- `build-switch/DKC2RecompSwitch.nro` (22,683,704 bytes)
- Candidate NRO SHA-256: `2ed394cbea444600469649ce1be71c0a48e64ce8137b0df201f951737a088327`.
- `build-switch/switch-build-manifest.json` records the candidate and source
  hashes. The build emitted existing generated PPU warnings only; no compile
  or link errors remained.

## Changes and boundaries

### Diagnostics

Log destination is `.runtime/boot.log`, independent of the working directory.
One previous boot is retained as `boot.previous.log`. Rotation errors fall back
to stderr without truncating the current log. Maximum 4096 normal log lines per
boot; fatal messages still pass the limit. Fatal status is stored and reported
in JSON. Directory creation verifies that EEXIST really names a directory.
Console fallback displays early errors and allows Plus to exit. Runtime logs
include build/compiler, renderer/fallback/output dimensions and obtained audio
format, channels, samples and rate. Visual error-path tests remain pending.

### Audio ownership audit

The main thread drives guest execution, port writes, SPC uploads, frame sync
and DSP sample production. The SDL callback consumes/resamples the DSP ring
via RtlRenderAudio, including hold/fade state and the no-op MSU mixer. Shared
common_rtl operations already call RtlApuLock/Unlock; these now use one static
libnx recursive mutex. It exists before SnesInit and remains alive until after
SDL_CloseAudioDevice. The callback samples ring occupancy under that same lock
and may recursively enter RtlRenderAudio. There is no SD I/O, logging or
allocation in the new callback code.

Lock order: pause/close the SDL device without holding the APU mutex; only then
reset the runtime or alter volume. Metrics copy audio counters under the APU
mutex, release it, and only then write logs. Reset uses RtlReset(1) with the
callback paused, preserving SRAM. This matters because not every operation in
RtlReset is internally locked. No whole-frame lock is added.

The device remains paused until the first guest frame; menu and focus loss
pause both simulation and audio. Volume zero continues consuming audio to
avoid building a backlog. Audio subsystem failure allows muted gameplay.
`empty_at_entry` counts callbacks seeing an empty ring; it is NOT a count of
all resampler underruns or dropped samples. Full overflow/underflow accounting,
buffer-size tuning and measured audible continuity remain pending.

### SRAM and lifecycle

The Switch-only adapter avoids changing the shared runtime SRAM ABI or the
pinned submodule. It reads exact-size files into scratch memory, then copies
them to SRAM only on success. A missing/truncated primary falls back to .bak.
Writing checks fwrite, fflush, fclose and a complete readback, then rotates a
valid-size primary and promotes .tmp. A truncated primary never replaces a
valid-size backup. Failure is reported and dirty state is retained for retry.
Size/readback checks do not detect every same-size semantic corruption.

SRAM is compared against the last loaded/successfully saved snapshot at a
provisional 30-second interval, on menu entry, focus loss, reset and normal
exit. Unchanged contents cause no SD write. This preserves game SRAM progress,
not arbitrary gameplay: it does not add save states or bypass in-game saves.
After an abrupt close, up to roughly 30 seconds of SRAM changes may be lost
under normal foreground operation; failed writes, stalls or OS suspension can
extend that window. No guarantee is made for power loss, SD-controller caches,
rename atomicity, or events without a scheduling opportunity. No player's real
SRAM was used in the synthetic fault tests.

libnx focus/resume hooks pause audio before notified suspension and request
clock resynchronization; Application mode requests SuspendHomeSleepNotify.
SDL window events provide an additional foreground guard. Applet mode keeps
its platform focus policy. HOME/sleep timing and event delivery need hardware
validation. Autosave on notification is best-effort, not a power-loss guarantee.

### Timing, controls and menu

Simulation stays at the existing 60 Hz; VSync plus software waiting is retained
until comparative hardware measurements exist. Deadlines more than three frame
periods behind are discarded instead of causing catch-up bursts. Menu/focus
resume also resets the deadline. Every 600 gameplay frames, logs record mean,
p95, p99 and maximum for simulation, PPU, texture upload and RenderPresent,
plus late frames and audio occupancy. Sorting uses 19,200 bytes of static
samples, without per-frame SD writes. No FPS or power improvement is claimed.

Handheld belongs only to P1; P2 uses No2. Enabled styles: handheld, paired
Joy-Con and full controller/Pro. Single Joy-Con is deliberately not enabled.
Physical B/Y/A/X is preserved. Left stick uses axis dead zone 12000/32767;
diagonals are allowed, opposite directions neutralize. Connection changes are
logged and input is suppressed until neutral after reconnect/menu exit.
P1 controls the host menu; P2 remains the game's original second player.
Remapping, rumble and simultaneous TEAM integration are still pending.

ZL+ZR opens/closes the menu. D-pad selects; A activates; Left/Right changes
options; B returns/cancels. Options: resume, 4:3/16:9, volume 0–10, nearest/linear
scaling, alternative L+R+Minus shortcut, confirmed reset and save/exit. The
alternative chord consumes Select only while invoking the menu. Aspect applies
on the next app launch, before PPU/buffer creation; reset is not app relaunch.
4:3 is displayed at 960x720 centered in the 1280x720 logical output. The previous
16:9 default is retained provisionally, without claiming full widescreen coverage.

Settings are a fixed 12-byte versioned record (`DKCS`, version 1, aspect,
volume, filtering, shortcut, three reserved zero bytes) at settings.bin. Invalid
values/version use a valid backup or defaults. Settings writes are checked and
failures stay visible. Font rendering uses the existing SDL stack's SDL2_test
helper, with 24-pixel text at 720p. Legibility and controller-only navigation
must still be exercised on hardware.

## Local verification

The pre-change build-host suite was attempted: 17/49 passed; 32 were not runnable
because cached paths/executables belonged to another installation. See
`tmp/roadmap-baseline-tests.log`; these are not asserted to be gameplay failures.
A fresh build-roadmap-tests was configured with MSYS GCC and `-D_GNU_SOURCE`
to expose POSIX declarations required by the existing launcher model. No shared
launcher source or global warning suppression was changed. All 50 configured
checks passed. The public suite has no private ROM/corpus fixtures configured.

New tests cover all 12 button bits, player packing, dead-zone edges, opposite
directions, deadline resync, settings version/limits/reserved bytes, missing and
truncated saves, backup recovery, and injected write/flush/close/rename failures.
Separate Python tests check artifact hashes/header acceptance and rejection
of truncated/missing NROs, plus logger path independence, rotation, rotation
failure, line limits, fatal JSON and nested lock-hook wiring. The logger test
uses a small libnx shim; it does not prove libnx threading behavior. A failed
read/close while validating the old SRAM now aborts promotion, preserving it. Logs are in tmp/roadmap-tests-final.log and
roadmap-host-build-final.log. Cross-build and final hash are recorded below.

## Next hardware pass

Use copied SRAM, preserving both the baseline NRO and its save. Exercise boot
from different folders; missing/invalid ROM; inaccessible/full SD; each menu
option and reset confirmation; initial/muted/unavailable audio; HOME/sleep;
reconnect; original two-player mode; 4:3/16:9; handheld/dock; and a 60-minute
session. Record NRO hash, console/CFW, clocks, routes, logs and outcomes. Do not
close roadmap acceptance criteria merely from these local tests.
