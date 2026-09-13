# Interactive desktop test builds

## Scope

`dkc2_snesrecomp_desktop` is the accepted Windows presentation host for
the native `snesrecomp` path. It uses the same generated code, interpreter
fallback, PPU, SPC700, S-DSP, frame adapter, and verified-ROM loader as the
headless regression runner. The repository contains no ROM or generated game
code.

`dkc2_snesrecomp_sdl` is the parallel Windows/Linux/macOS gameplay host. It
uses SDL2 for video, audio, input, controllers, and timing while retaining the
same shared launcher and game/runtime behavior. Its lifecycle is automated on
Windows. The native Apple-silicon `.app`, menu, icon, Application Support
persistence, and private-ROM integration tests are accepted locally; remaining
Mac distribution and Linux acceptance work is tracked in `CROSS_PLATFORM.md`.

The host currently provides:

- a resizable Windows window that presents the complete 256x224 BGRX frame at
  a conventional 4:3 display aspect;
- real-time pacing at 60.098811862 video frames per second;
- signed 16-bit stereo output at the SNES DSP rate of 32,040 Hz, queued in
  fixed 2,048-frame Windows wave-output blocks;
- keyboard input while the game window has focus; and
- two independently routed SNES controller ports using focused keyboard input
  or up to two hot-pluggable XInput gamepads;
- fixed 3x fast-forward and approximately 15 seconds of fixed 3x rewind; and
- load-on-start/clean-exit persistence for DKC2's 2 KiB battery SRAM;
- a once-per-second FPS readout in the game-window title; and
- opt-in main-thread phase telemetry for diagnosing slowdown.

## Build

From PowerShell at the repository root:

```powershell
git submodule update --init --recursive
.\scripts\generate_snesrecomp.ps1 -Rom "C:\private\dkc2.smc"
cmake -S . -B build-snesrecomp `
    -DDKC2_BUILD_SNESRECOMP=ON `
    -DDKC2_ROM="C:\private\dkc2.smc"
cmake --build build-snesrecomp --config Release `
    --target dkc2_snesrecomp_desktop
```

Build the portable host on any platform with target
`dkc2_snesrecomp_sdl`. It produces `DKC2RecompSDL.exe` on Windows and
`DKC2Recomp` on Linux or `DKC2Recomp.app` on macOS. The portable private
generator is:

```sh
python3 scripts/generate_snesrecomp.py --rom /private/path/dkc2.sfc
```

On macOS, the supported local bundle workflow is:

```sh
./build_macos.sh "/private/path/dkc2.sfc"
open build/macos/DKC2Recomp.app
```

The script bundles SDL2, fixes the dylib install name, ad-hoc signs/verifies
the app, and registers its icon. Mutable data lives in
`~/Library/Application Support/Flat2VR/DKC2Recomp` instead of the read-only
bundle. This is a local test signature, not a notarized distribution.

Release builds use `-O3` with GCC/Clang. MSVC uses `/O2`, its highest
supported speed preset. A private icon can be embedded without entering the
source tree by adding
`-DDKC2_DESKTOP_ICON="C:\private\dkc2.ico"` to the configure command.

This option enables both the diagnostic headless target and the desktop
target. The deprecated `DKC2_BUILD_SNESRECOMP_HEADLESS` name remains an alias
for existing build directories. Generated C stays under ignored
`generated/snesrecomp/`, and all build products stay under the ignored build
directory.

## Run

For the simplest launch, double-click:

```text
build-snesrecomp\Release\DKC2Recomp.exe
```

The application opens without a command-prompt window and asks you to select
your private DKC2 `.smc` or `.sfc` file. Cancelling the picker exits normally.
The verified ROM is read from its existing location and is never copied into
the repository.

The source-only launcher remains useful for repeatable command-line runs:

```powershell
.\scripts\run_snesrecomp_desktop.ps1 -Rom "C:\private\dkc2.smc"
```

Or invoke the executable directly:

```powershell
.\build-snesrecomp\Release\DKC2Recomp.exe `
    "C:\private\dkc2.smc"
```

The loader accepts only a payload whose headerless SHA-256 is
`35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633`.
It removes an optional 512-byte copier header in memory; it never changes the
private file.

## Default controls

| SNES control | Keyboard | XInput controller |
| --- | --- | --- |
| D-pad | Arrow keys | D-pad or left stick |
| B | `Z` | A |
| A | `X` | B |
| Y | `A` | X |
| X | `S` | Y |
| Start | Enter | Menu/Start |
| Select | Shift | View/Back |
| L | `Q` | Left shoulder |
| R | `W` | Right shoulder |
| Rewind (3x) | `1` | Left trigger |
| Fast-forward (3x) | `2` | Right trigger |
| Save state (selected slot) | `F5` | — |
| Load state (selected slot) | `F9` | — |
| Toggle performance log | `F` | — |
| Overlay | Escape | Guide, or Start+Back |
| Quit | Overlay button | — |

Input is intentionally ignored when the game window is not focused. The ImGui
launcher exposes Player 1 and Player 2 source selectors. Keyboard is Player 1
and Gamepad is Player 2 by default; Gamepad players receive connected XInput
devices in player order. Selecting two Gamepad sources assigns the first two
connected devices to SNES ports 1 and 2. Source, deadzone, keyboard bindings,
and standard-controller bindings persist in `launcher.cfg`. Each player's
Configure page exposes the complete SNES layout. Its compact Assist Shortcuts
row exposes Rewind and Fast-forward; the top-level Assist page additionally
exposes Save State and Load State. Select a chip, press a key/button/axis, then
press Play to commit it. Pads can be attached or removed while the program is
running. Native DirectInput and PlayStation-controller APIs are not implemented
yet; pads translated to XInput by their driver or a launcher are expected to
work.

## In-game overlay and Assist Tools

Escape opens the Dear ImGui overlay in the Windows OpenGL and SDL/OpenGL
hosts. When the SDL/Mac window is fullscreen and the overlay is closed, Escape
first returns to windowed mode and consumes that keypress; a later Escape opens
the overlay normally. Emulation stops at the completed host-frame boundary,
controller input is suppressed, and queued audio is cleared/paused until
Resume or Escape closes the menu. The pages are Main, Settings, Assist Tools /
Cheats, Controls, and Credits.

Assist Tools default off. Enabling them permits the existing 3x rewind,
3x fast-forward, five-slot overlay controls, and configurable state shortcuts;
disabling the gate makes those bindings inert. The native Mac Game menu's
fixed Quick Save/Load commands remain available and use Slot 1 regardless of
the Assist gate. Previous/Next wraps through Slots 1–5, and overlay Save/Load
acts on the selected slot. Files are `dkc2s0.sav` through `dkc2s4.sav`; the
legacy `dkc20.sav` fallback is limited to the first slot. The setting is saved
as `AssistTools` in `launcher.cfg`, and an enabled run adds
`(Assist Tools: On)` to its title. This is host policy only and is not
serialized into the SNES snapshot.

Settings mirrors the pre-boot DKC2 choices. Volume, screen model, texture
filtering, both player source/deadzone values, and Assist Tools apply live.
Window scale, fullscreen, presenter choice, audio enable, and skip-launcher
are persisted but require a restart. The sample-rate value is mirrored and
persisted, but DKC2 currently outputs only the SNES-native 32,040 Hz stream;
alternate rates need a future tested host resampler. Controls has nested
Player 1, Player 2, Assist, and Fixed Shortcuts tabs. Each player page edits
the input source, deadzone, and all 12 SNES keyboard/controller bindings.
Assist edits Rewind, Fast-forward, Save State, and Load State. These are the
same settings used by the pre-boot launcher, so changes apply to the live host
and persist to `launcher.cfg` on clean exit. Restore All Settings resets the
complete shared value, including gameplay/Assist bindings and the Assist
Tools gate.

To test an in-game remap, open Controls, select a keyboard or controller chip,
and provide the replacement input. Escape cancels capture instead of closing
the overlay. Controller capture requires one neutral/released poll before it
accepts a button or signed axis; this prevents the UI activation button from
becoming the new binding. Resume and confirm the new mapping drives gameplay,
then close and relaunch to confirm persistence. Repeat once for each player
and once for an Assist action with the Assist gate both off and on. The Fixed
Shortcuts page should continue to show Escape / Guide / Start+Back for the
overlay and F for the performance log; those recovery shortcuts are
intentionally not remappable.

The GDI compatibility presenter renders the same ImGui overlay using SDL's
software renderer. Check Escape, mouse navigation, binding capture, resume,
and resize on GDI as well as OpenGL. Escape must display the menu and pause
gameplay rather than leave a paused frame with no visible controls.

The pre-boot launcher has separate Assist Tools and Credits pages. Confirm the
Assist checkbox and all four Assist bindings survive a launcher restart and
that Back returns to the Dashboard. Confirm each player binding chip captures
keyboard and controller input, and that Reset restores DKC2 defaults. Confirm
the host-supplied credits wrap and remain readable at every supported launcher
scale.

## Restore launcher defaults

On the Settings page, **Restore Defaults** opens a confirmation dialog and
then replaces the entire editable DKC2 launcher configuration with the values
from `Dkc2LauncherSettingsDefault`. This includes display, audio, player
sources/deadzones/bindings, Assist bindings, and `SkipLauncher`. It does not
change the selected ROM or
touch SRAM/save-state files. Press Play to commit the restored settings to
`launcher.cfg`; Cancel leaves every current value unchanged.

## Video and CRT screen-color model

The launcher's Display card offers **OpenGL** (default) and **GDI
compatibility**, **Nearest/Bilinear** scaling, and the screen models **Raw**
(default), **CRT**, **Composite**, and **Trinitron**. CRT is opt-in: select CRT,
press Play, and the choice will be written to `launcher.cfg`. Raw bypasses the
screen-color conversion byte for byte. The filtered choices use the
PSXRecomp-derived 32,768-entry present-time color LUT and do not change SNES
memory, save states, deterministic hashes, or private raw frame captures.

This initial CRT feature models color response—phosphor gamut, gamma,
luminance, and black floor. It intentionally does not yet simulate scanlines,
screen curvature, a bezel, or phosphor persistence. Nearest/Bilinear is a
separate scaling choice. If OpenGL cannot start, the window is recreated and
the atomic GDI compatibility presenter receives the same transformed pixels.

For repeatable tests without changing `launcher.cfg`, set `DKC2_SCREEN` to
`raw`, `crt`, `composite`, or `trinitron`. `DKC2_DESKTOP_REQUIRE_GPU=1` turns an
OpenGL initialization failure into a test failure;
`DKC2_DESKTOP_FORCE_GDI=1` explicitly exercises the fallback. These variables
are diagnostic controls, not emulated SNES settings.

Visible Windows OpenGL windows request VSync at one swap interval. Confirm the
active result in `diagnostics/last_run_report.json`: a Windows OpenGL backend
ends with `vsync=on`, `request-failed`, or `unsupported`. Hidden automated
windows report `vsync=off` by design, and GDI reports `compositor-managed`.

Visible macOS OpenGL uses a single timing authority instead: SDL swap interval
is zero by default, the exact 60.098811862 Hz deadline is waited with
`mach_wait_until` plus a short final spin, and that wait occurs before the
compositor-atomic presentation. A deadline missed by more than 2 ms is
re-anchored instead of producing a short catch-up frame. This avoids stacking
the game's non-60.000 Hz cadence behind a second 60/120 Hz swap gate. Set
`DKC2_KEEP_OPENGL_VSYNC=1` only to compare the old double-gated path; the
diagnostic backend reports `vsync=off; pacing=mach` for the default Mac path.
Normal-speed owner testing must still check long-run cadence, audio queue
stability, windowed mode, and fullscreen mode.

Fast-forward executes three console frames per presented host frame. Rewind
stores one complete in-memory state every three console frames and restores
one state per presented host frame, so both controls move at approximately
3x. Rewind history is bounded to 300 snapshots (about 15 seconds) and is lost
when the application closes. Audio is muted and the queued Windows audio is
discarded while either time control is active; normal audio resumes from the
restored/current SNES state when the control is released. If both controls are
held, rewind wins. The fixed multiplier is a named host constant so a later UI
can replace it with a slider without changing the state format.

## Performance diagnostics

The game-window title updates once per second with the number of completed
presentations, for example
`DKC2 Recomp Alpha Pre-Release (FPS: 60)`. Press `F`, or set
`DKC2_DESKTOP_PERF=1` before launch, to create `performance.log` beside the
executable. Each one-second sample reports average input, emulation, rewind,
PPU, audio, presentation, pacing, and untracked time, plus the fraction of
wall time for which the main thread was active. Intentional frame-pacing wait
is excluded from that active percentage.

The log records `backend=OpenGL` or `backend=GDI`, the selected screen model,
and CPU-side presentation time. GPU timestamp queries are not implemented, so
it writes `gpu_ms=n/a` for both backends. Use the phase totals to distinguish
expensive game emulation/filtering from host presentation or audio work; do
not interpret unavailable GPU time as zero. Telemetry is disabled by default
and adds phase timers only while enabled.

## Crash reports and diagnostic bundles

Every playable launch refreshes `diagnostics/last_run_report.json` beside the
executable. It records the build, host, operating system, loaded modules,
recent host breadcrumbs, last frame/resume PC, presentation backend, selected
screen model, audio availability, outcome, and fatal reason when present.
This is host observability only; it does not modify or serialize the emulated
machine.

A controlled runtime failure creates a timestamped
`diagnostics/diagnostic_bundle_YYYYMMDD_HHMMSS_PID` folder immediately. On
Windows, an unhandled exception also writes `crash_minidump.dmp`. Linux and
macOS fatal-signal handlers write a minimal async-safe marker; the next launch
turns that marker into a bundle because complex JSON and module enumeration
are not safe inside a signal handler.

To request a bundle after an otherwise clean manual run:

```powershell
$env:DKC2_DIAGNOSTIC_BUNDLE = "1"
.\DKC2Recomp.exe
```

The bundle allowlist is `report.json`, `README.txt`, optional `launcher.cfg`,
optional `performance.log`, and optional `crash_minidump.dmp`. It excludes
`rom.cfg`, ROM content and paths, generated code, SRAM, save states,
screenshots, and captured audio. Reports do contain loaded-module paths and
basic machine/OS details; review them before sending. Developers can run the
three deterministic private drills with:

```powershell
ctest --test-dir build-snesrecomp -C Release --output-on-failure `
  -R "supplied_rom_diagnostic_"
```

The drills cover clean requested bundles, a controlled fatal exit, and a real
Windows exception with a non-empty minidump. The harness rejects any file not
on the bundle allowlist. `SNESRECOMP_CRASH_TEST=die|seh` exists only for these
contained tests and intentionally terminates the process.

## Battery saves

The desktop host reads DKC2's 2 KiB cartridge SRAM after startup and writes it
after a clean exit. Paths are anchored beside the executable, independent of
the folder from which it was launched or the folder containing the ROM:

```text
build-snesrecomp\Release\saves\save.srm
build-snesrecomp\Release\saves\save.srm.bak
```

Before writing a new `save.srm`, the prior file is renamed to `save.srm.bak`.
These files are private user data and are ignored by Git. Automated CTest runs
set `DKC2_DESKTOP_DISABLE_SRAM=1`, so they cannot read or overwrite a player's
save. SRAM is separate from rewind: the former is the game's normal battery
memory, while rewind snapshots the full running console state only in memory.

F5/F9 file snapshots write `saves/dkc2s0.sav` beside the executable. Loading
tries that name first and then accepts the former `saves/dkc20.sav` name, so an
existing slot is not stranded. A successful load resets queued audio, deadline
anchors, and rewind history before redrawing the restored PPU boundary. A
user-provided slot immediately before a Pirate Panic death was replayed
headlessly and used to verify clean level restart.

## What to test

For the first manual pass, leave input neutral and watch one complete title and
three-demo cycle. Then press Start at the title, select a new game, and confirm
that directional and face-button mappings respond. Record:

1. whether any frame freezes, flashes incorrectly, loses a layer, or shows
   corrupt sprites;
2. whether music changes pitch, crackles, drops out unexpectedly, or develops
   repeated gaps;
3. whether keyboard and controller inputs respond once per press and remain
   stable when held; and
4. the approximate title/demo or level location of every problem.

For simultaneous co-op, attach two controllers (or set Player 1/2 sources in
the launcher), start a game with "2 PLAYER TEAM" selected, and record:

1. both Kongs move, jump, and attack at the same time, each from their own
   controller (the second Kong stands by until player 2 first presses a
   button, then stays playable);
2. when one Kong is hit, only that Kong runs off; the other player's
   controller keeps driving the survivor, and a broken DK barrel returns the
   lost Kong to the same player's controller;
3. the camera follows one Kong (deliberately separating the players can walk
   the second Kong off-screen; that is a known limitation, not a defect); and
4. with "2P Team mode" set to Classic (or `DKC2_COOP=classic`), control
   alternates exactly like the cartridge: the inactive Kong follows by AI and
   control passes on a hit.

Automated evidence for items 1 and 4 already exists: a deterministic
4,122-frame route (crafted TEAM save image plus recorded title-to-first-level
navigation, replayed by the headless host) drives controller 1 and controller
2 in separate and combined windows. Under the simultaneous policy the second
Kong's slot moves only when its own controller is active and both slots move
together in the combined window; under the classic policy the identical
recording leaves the second Kong's slot untouched. The `DKC2_COOP_TRACE`
probe in the headless host reproduces this comparison.

The automated desktop smoke test runs 180 hidden frames, opens and renders the
overlay for 30 host presentations while confirming emulation is paused,
closes it and resumes, executes one real 3x fast-forward iteration, captures
bounded history, and performs a real full-state restore after frame 120. It proves that
the verified ROM, window, game loop, renderer, audio-output initialization,
and rewind load path can start and shut down cleanly. Synthetic regressions
cover gamepad/trigger mapping, two-player source routing and port packing,
history wrap/pop order, five-slot wrap/clamp behavior, assist-action gating
and one-shot delivery, shared
4:3 viewport math, Raw byte-exact bypass, CRT
LUT application, atomic GDI presentation, FPS sampling, and telemetry
accounting. Dedicated hidden private-ROM tests require OpenGL+CRT and force
GDI+CRT for 60 frames each. Every automated desktop test disables SRAM
persistence.
The 12,000-frame headless test remains the authoritative deterministic gate for
two complete attract cycles. The 0.0.1 gate additionally includes completed
human watch/listen/controller passes for DKC2 and the four regression titles;
this is exercised-path sign-off, not a claim of 102% completion.

The intermittent one-frame black flashes in the first recorded test were host
presentation artifacts, not SNES forced blank: the old paint path visibly
cleared the client black and then stretched the game image in a second GDI
operation. The host now composes black borders and the scaled frame into an
off-screen bitmap and publishes it with one `BitBlt`. A corrected active-demo
recording contained zero isolated black frames; manual retesting on the user's
capture setup remains the final perceptual check.

The former 54-frame first-cycle timing lag has been corrected at its source:
the 65816 interpreter was discarding program-bank bit 7 and treating FastROM
execution as its SlowROM mirror. Current first-cycle completion is six frames
early relative to Snes9x, with every level-loading window within one frame.

## Private Version 10 diagnostic workflow

The playable Win32 and SDL hosts record one six-hex-digit controller sample
per emulated frame when `SNESRECOMP_INPUT_REC` names an output file. Recording
now opens before gameplay begins and fails visibly if the path is invalid.
While active, the title includes `(Recording Input)`. The host flushes each
sample and reports write, flush, and close errors instead of silently producing
no recording. Files use byte-stable LF endings on every platform.

The private diagnostic package at
`C:\Users\Nickt\Documents\DKC2 Personal Test Builds\Version 10` localizes the
executables, verified ROM, current saves, tools, and writable evidence folders
needed for the widescreen investigation. From that folder:

```powershell
.\Record-Pirate-Panic.ps1
.\Diagnose-Frame.ps1 -Recording .\recordings\<recording>.input -Frame 5499
```

The recording helper refuses to replace an existing route and preserves the
starting SRAM beside it as `<recording>.start.srm`; this makes replay independent
of later save progress. It also writes session metadata. The diagnosis helper
automatically uses that SRAM, the packaged trace runner, and a new timestamped
capture directory. `Verify-Diagnostic-Kit.ps1` performs a short record/replay/
capture smoke test without requiring a full manual level run.

## Experimental aspect validation

The normal launcher, pause-menu Settings page, and native Mac View menu expose
4:3 (256x224), 16:10 (308x224), and 16:9 (342x224). Authentic 4:3 remains the
default. For deterministic developer runs, select an exact aspect without
editing `launcher.cfg`:

```powershell
$env:DKC2_ASPECT = "16:10"
.\build-snesrecomp\Release\DKC2Recomp.exe
Remove-Item Env:\DKC2_ASPECT
```

`DKC2_WIDESCREEN=1` remains a compatibility alias for 16:9 when
`DKC2_ASPECT` is unset.

The trace-enabled headless build listens on TCP port 4382. Keep the completed
run alive, then capture the most recently presented PPU buffer:

```powershell
$env:DKC2_WIDESCREEN = "1"
$env:SNESRECOMP_TRACE_HOLD = "1"
.\build-snesrecomp\Release\dkc2_snesrecomp_headless.exe `
  "C:\private\dkc2.smc" 4000

python .\scripts\capture_tcp_screenshot.py `
  --frame 3999 `
  --output ".cache\widescreen-captures\frame4000.bmp"
```

Add `--wram-output <path>` and `--vram-output <path>` to export the selected
historical frame's matching 128 KiB WRAM and 64 KiB VRAM snapshots. These are
private diagnostics for level-stream calibration and must remain under an
ignored directory.

Set `SNESRECOMP_LAYER_MASK` to `1`, `2`, `4`, or `16` before launch to isolate
BG1, BG2, BG3, or OBJ. Add `--scan-oam-margins --report-output <path>` to scan
the route for sprites whose rendered coordinates enter the extra 43 columns.
The JSON report also includes `widescreen_shadow` hit/miss counters for BG1 and
BG2. A miss is served by the configured bounded fallback, never by an
unclassified raw VRAM margin.
All BMP/JSON/stdout diagnostics belong under ignored `.cache`; do not package
or commit them.

For a complete same-frame evidence bundle, use
`scripts/capture_widescreen_diagnostics.py`. It launches deterministic,
fresh-process composite/BG1/BG2/BG3/OBJ runs and correlates isolated images
with WRAM game-sprite state, render-consumed OAM, VRAM, PPU state, and margin
pixel coverage. Each report also contains a `screen_profile` derived from live
state: terrain owner, horizontal/vertical/square/ship-hold map layout, BG3
policy, BG2 policy, and the raw
level configuration used to make that decision. `unknown` and
`square_or_special` profiles are evidence requests, not permission to widen a
screen. See
[`WIDESCREEN_DIAGNOSTICS.md`](WIDESCREEN_DIAGNOSTICS.md) for the build command,
privacy boundary, output schema, and terrain/object decision workflow.

The private Bramble regression configures the trace build with
`DKC2_BRAMBLE_INPUT` and `DKC2_BRAMBLE_SRAM`, registering
`supplied_rom_widescreen_bramble_route`. It checks level `$002E` frame 1,600,
camera `(2653,2456)`, BG1 ownership, the proven `$60`-byte square row layout,
non-empty terrain in both margins, active shadow hits, zero findings, and no
blocking runtime event. The current fixture ends before the goal and therefore
does not replace entrance-to-goal acceptance.

The evidence-bundle wrapper also accepts `--scan-oam-margins` to preserve
every render-consumed margin OAM sample across the replay. Use
`--function-watch CODE_NAME` only with a trace-enabled runner when it is
necessary to prove that a particular statically recompiled routine executed;
the report stores the first hit and its call stack. These options can produce
large private traces and are not normal play settings.

Manual acceptance must cover:

1. switching 4:3/16:9 in both UI surfaces and across a clean restart;
2. title/menu/room sidebars remaining black without a previous frame leaking;
3. continuous foreground/background motion at both gameplay edges;
4. enemies spawning, animating, colliding, and despawning correctly in both
   margins; and
5. death/restart, bonus entry, goal, map, and save-state transitions.


## Windows dropdown regression (2026-09-13)

`desktop_menu` covers all valid choice ranges, invalid IDs and enum parity.
`windows_menu` creates an actual hidden SDL/Win32 window and checks native menu
hierarchy, dark owner drawing, mnemonics, client sizing, command delivery,
disabled-command rejection, state checkmarks, and detach/reattach across
fullscreen. `windows_input` verifies the complete default keyboard mapping
before SDL starts, during SDL initialization and after SDL_Quit, plus buffered
short taps. `desktop_input` covers controller binding/routing and requires
Start+Back to remain blocked until both buttons have released.

Run the complete configured Release CTest suite. If the Windows PowerShell
packaging test cannot find Get-FileHash when launched from PowerShell 7, prepend
`$env:SystemRoot\System32\WindowsPowerShell\v1.0\Modules` to the test process's
PSModulePath; this is an inherited environment issue, not a packaging-code fix.

For manual acceptance use a separate executable directory and external ROM;
set SNESRECOMP_INPUT_REC to an external recording path. Confirm keyboard Start,
directions and face buttons in gameplay; open menus by mouse and Alt mnemonics;
change aspect/player sources; pause/resume; Quick Save/Load with Assist off;
and Alt+Enter/Escape fullscreen recovery. Confirm no menu keys reach the guest
recording. Physical pads and Linux/macOS require separate platform acceptance.

`supplied_rom_gdi_overlay` exercises the software ImGui backend with the
external supported ROM. `desktop_present` checks that CPU overlay drawing sees
the finished game frame and appears in the final GDI presentation.
`supplied_rom_coop_route` runs `scripts/check_coop_route.py` from blank SRAM,
selects a new TEAM game and checks P1-only, P2-only, post-landing and concurrent
movement against classic mode (3,518 frames per policy). Its temporary replay
and WRAM traces are outside the repository and are deleted when the check ends.

`supplied_rom_coop_combat` runs `scripts/check_coop_combat.py` using the same
blank-SRAM TEAM navigation. P2 roll and stomp routes keep P1 over 100 pixels
away from the defeated enemy; the stomp must put P2 in bounce state and move
it upward while P1 remains grounded. A contact route must hurt only P2 while
the enemy stays alive, and a P1 roll route guards ordinary attack behavior.
The check compares each Kong's normal CGRAM colors against the supported
external ROM, without embedding palette bytes in source. Optional
`DKC2_COOP_TRACE_SPRITES=1` adds 24 sprite slots, reaction/collision pointers,
and both Kong palettes to the private `DKC2_COOP_TRACE` output. These traces
are observation-only and must remain outside Git. Roll and stomp checks now
also require P2 to walk and jump after its attack recovery.

`supplied_rom_coop_barrels` checks P2 DK-barrel throws in both directions and a
regular-barrel throw with P1 on a different platform height. It verifies the
carried position, release origin, trajectory and subsequent movement, and
rejects DK-barrel rescue of an already present player.
`supplied_rom_coop_lifecycle` checks hurt departure, held-input rejection,
save/load while lost, P1's DK-barrel rescue, restored P2 movement/collisions,
and a second loss/load. `DKC2_SAVESTATE_OUTPUT` writes the final headless frame
snapshot to an explicitly supplied private path for this check.

For the owner's September 13 Pirate Panic ledges snapshot, run:

```powershell
python scripts/check_coop_saved_state.py --runner build/Release/dkc2_snesrecomp_headless.exe --rom C:/private/dkc2.sfc --state C:/private/dkc2s0.sav
```

The checker prints the input state hash and covers both controller roles for
contact damage, roll kills and subsequent movement with P2 as the camera leader.
The accepted input hash is
`5cd521b68ff3daaca043c4e7624c1bd8ca211cd072f580a438ef303e3ff51ee3`.
This layout-specific route is additional acceptance, not a mandatory public fixture.
Full-game enemies/bosses, other animal types and special throwable interactions,
death/respawn and physical gamepads need separate acceptance.


### Rambi ownership and death handoff

`supplied_rom_coop_handoff` starts a new TEAM game, lets P1 take damage, and
requires P2's position to remain unchanged at the leader switch. It checks that
turn freeze clears, a live enemy changes position over subsequent frames, P2
can move, and P1 remains lost after save/load.

The later September 13 animal ledges save has SHA-256
`3f5a2b019b68fef226fd7b994ab31a566e7b9aec5e33a7a46a1ba01e6fb21307`.
Keep it outside Git and either run the check directly or configure its private
path to include it in CTest:

```powershell
python scripts/check_coop_animals.py --runner build/Release/dkc2_snesrecomp_headless.exe --rom C:/private/dkc2.sfc --state C:/private/animal-ledges.sav
cmake -S . -B build -DDKC2_COOP_ANIMAL_STATE=C:/private/animal-ledges.sav
ctest --test-dir build -C Release -R supplied_rom_coop_animals --output-on-failure
```

Both players must mount, ride, jump, dismount and remount. The on-foot partner
must retain a normal jump, and cannot steer or dismount the rider. Mounting
ownership must survive a fresh process loading the saved state; P2 must also
take over after P1 dismounts. Traces and generated snapshots use temporary
external directories. Other animal types, transformations and animal damage
handoffs still need separate acceptance.


`supplied_rom_coop_widescreen` uses the same optional `DKC2_COOP_ANIMAL_STATE`
fixture. It changes leader through actual mounting/dismounting, then walks
both Kong roles to both screen edges in 4:3, 16:10 and 16:9. The checks require
26 or 43 additional playable columns per side in a centered wide view, while
the other controller's Kong stays stationary. A fresh TEAM entrance snapshot
then checks Glide, Shift, Bars and Reflect policies at the west level bound,
including the extra east-side reach of an inward-shifted view. No private
state or generated image is committed with these tests.


### Mounted TEAM banana collection

`scripts/check_coop_bananas.py` accepts `--runner`, `--rom`, and an external
`--state` for the reported mounted Pirate Panic trail. Fixture SHA-256:
`4123ce28e54f2eaf7cda66e14374d530bf845b605550f5797d091bd66682e421`.
Set `DKC2_COOP_BANANA_STATE` to that private snapshot when configuring CMake
to enable `supplied_rom_coop_bananas`. The source-owned recipes move both
controller roles, exchange the rider, load mounted/collected snapshots, and
check separate on-foot, riding and dismounted collection. The shared BCD
banana count is observed through `DKC2_COOP_TRACE_SPRITES`; a repeated path
must not award previously collected bananas again. Tests do not patch guest
memory or store game data in Git. Physical controller and other-animal
acceptance remain separate.


### Display parity and rumble

`desktop_shaders` uses synthetic checkerboards, slopes and gradients to compare
native WGL with SDL output for four color presets, Nearest/Bilinear, all five
Reconstruct modes, and the three tuning endpoints (40 paired settings). It
requires a working OpenGL GPU. Default framebuffer versus RGBA8 FBO rounding
may differ by one 8-bit channel step in fewer than 0.1% of channels; larger
errors fail. Tuning controls must change output independently. GDI color-model
bytes and atomic overlay composition remain covered by `desktop_present`.

`supplied_rom_coop_widescreen` now covers 21:9 for both players in addition to
the original three aspects. `check_widescreen_state_corpus.py --aspect 21:9`
checks the authentic center and margins with external snapshots. The SDL
`DKC2_DESKTOP_TEST_LOADSTATE` hook now works on Windows as well as macOS.

The combat replay requires the P2 stomp to emit exactly P2 feedback and requires
ordinary rolling/jumping to emit none. Synthetic tests cover P1 solo mapping,
classic/contest mapping, clearing events on restore, and keyboard/gamepad
routing. Escape > Settings exposes Stomp rumble, per-player device status,
and separate test pulses. Physical motor feel remains a manual acceptance step.

`supplied_rom_desktop_settings` launches isolated WGL, GDI and SDL copies twice
each, opens and closes the pause overlay, and verifies persisted non-default
CRT, reconstruction, rumble, aspect, co-op and input preferences. GDI must
preserve the saved Reconstruct request even though its active sampler falls
back. This catches inadvertent global-setting resets during overlay creation.


## CRT, characters and replacement audio

`desktop_shaders` compares 40 flat and 13 CRT combinations on WGL and SDL.
`supplied_rom_desktop_settings` checks all persisted tube values across two
launches of each host, including GDI fallback. `project_kongs_runtime` covers
per-slot move state; `dkc2_msu1` and `dkc2_spc_music` use only synthetic fixtures.

Private acceptance tools:

- `check_project_kongs.py`: external pack and scene rendering/machine comparison.
- `check_coop_barrels.py --kongs-pack PACK`: both-player replacement callbacks.
- `check_msu1_pack.py`: real PCM headers, SFX, fallback and gameplay equivalence.

Keep every generated input, save, PCM capture and game image in an external
output directory. Test the native Escape Characters/Settings controls after
building; test results alone do not establish physical listening or rumble.
