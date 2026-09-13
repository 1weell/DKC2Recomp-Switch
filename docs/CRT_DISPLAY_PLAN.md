# CRT television display mode: plan

Status: implemented 2026-09-04 (phases 1 to 3 in one pass; see
`docs/IMPLEMENTATION_JOURNAL.md` for what the build changed against this
plan: sigma_bright is 0.50 and the knee 0.9 because a normalised white
periodic sum peaks slightly above 1 for any width under 0.56; the mask is
applied in the compose pass in window pixels so curvature cannot resample
it; the living-room scanlines default is 55; the beating check compares
linear-light row profiles with the scene trend removed).

## Goal

An optional display mode that makes the presented frame look like a good
consumer CRT television on the 16-inch MacBook Pro's Retina XDR panel: soft
phosphor glow, a beam whose lines are felt more than seen, a fine mask
texture that resolves into "tube" rather than "grid" at viewing distance,
and no loss of brightness. It is a present-only stage, like the screen-color
model: the 256x224 native frame, its hashes, the widescreen margins, save
states, and the recomp are untouched by construction.

It is explicitly not a "scanline shader": no fixed dark lines multiplied
over the image, no darkening, no moire against the panel's pixel grid.

## Why overlay scanline shaders look wrong here

1. They multiply the image by a fixed line pattern in gamma space, so every
   row loses the same light regardless of content. Real beams do the
   opposite: a bright line is wide and almost fills the gap to its
   neighbours; a dark line is thin. Lines are visible in shadow and
   disappear in highlights.
2. They assume an integer scale. On this panel the 224-line frame is drawn
   at a fractional pitch, so a line pattern baked on a fixed grid beats
   against the source rows and produces low-frequency bands.
3. They are single-pass in 8-bit gamma space, so any glow or mask is
   guessed rather than computed, and the result bands in gradients.

## The physical numbers we are matching

Panel: 3456x2234 backing pixels, about 254 pixels per inch, 0.1 mm per pixel.
The viewport keeps the SNES 7:6 pixel aspect (`desktop_viewport.c`).

| Aspect (frame)  | Viewport (px) | Source pixel on screen | Pixels per scanline |
|-----------------|---------------|------------------------|---------------------|
| 4:3, 256x224    | 2978x2234     | 11.6 x 10.0 px, 1.2 x 1.0 mm | 9.97 |
| 16:10, 308x224  | 3456x2154     | 11.2 x 9.6 px, 1.1 x 1.0 mm  | 9.62 |
| 16:9, 342x224   | 3456x1940     | 10.1 x 8.7 px, 1.0 x 0.9 mm  | 8.66 |

Fullscreen on this laptop draws a source pixel about one millimetre across,
which is the physical scale of a 14-inch professional monitor rather than a
20-inch living-room set. That is good news: the panel has 9-12 pixels to
spend on each source pixel, enough to render a beam profile and a
three-pixel phosphor triad analytically instead of faking either.

A 3-pixel triad is a 0.3 mm pitch (fine-pitch monitor territory); a 6-pixel
triad is 0.6 mm (consumer Trinitron territory). Both are below the eye's
resolving limit at arm's length, so the mask reads as texture, not grid.

## The model

Everything below runs in linear light. The frame after the screen-color
LUT is sRGB-encoded 8-bit; the first pass decodes it, the last pass encodes
it and dithers.

### Beam (vertical)

For an output pixel at continuous line coordinate `y` (scanline pitch = 1),
each nearby source line `i` contributes a Gaussian centred on `i + 0.5`:

    w_i(c) = L_i(c) * exp(-d_i^2 / (2 sigma^2)) / (sigma * sqrt(2 pi))
    d_i     = y - (i + 0.5)
    sigma   = mix(sigma_dark, sigma_bright, sqrt(L_i(c)))

per channel `c`, summed over the two to four lines within reach. Because
the kernel is normalised, the average of the periodic sum over one pitch is
exactly the source brightness for any sigma: mean brightness is conserved,
not darkened. The ripple (line visibility) then follows content:

| Line brightness | sigma (built: 0.32 dark / 0.50 white) | Peak | Mid-gap | Visibility |
|-----------------|---------------------------------------|------|---------|------------|
| 1.00 (white)    | 0.50                                  | 1.01 | 0.97    | 5% ripple, lines vanish |
| 0.30            | 0.42                                  | 0.32 | 0.26    | soft lines |
| 0.05 (shadow)   | 0.36                                  | 0.06 | 0.04    | clear lines, but dark |

Two knobs: "Scanlines" maps to `sigma_dark` (0.18 thin ... 0.50 none; the
living-room default of 55 gives 0.32) and the white width is 0.50. A
normalised periodic sum peaks a little above 1 for any width under 0.56,
so the compose pass bends everything above 0.9 with a soft knee instead of
clipping. Per-channel sigma is what gives bright reds and whites their
slight bloom; the difference is kept modest.

Below about 4 output pixels per line the beam cannot be represented and
the model blends to flat (sigma grows toward 1); this is derived from the
viewport, not a setting, so small windows stay clean.

### Horizontal response

The horizontal pass resamples each source row to the viewport width with a
Gaussian of width `sigma_h` in source pixels (0.25 sharp ... 0.60 soft,
default 0.32). This is the video-bandwidth softness of an RGB or S-video
signal: dithers and 1-pixel lines smear the way they did on the tube,
without the halo of a sharpening kernel. NTSC composite artefacts (color
bleed, dot crawl) are out of scope; the existing Composite screen model
stays a colour transform only.

### Phosphor mask

Aperture grille by default: output column `x mod 3` is the R, G, or B
stripe. With strength `m`, a stripe passes its own channel at 1.0 and the
others at `1 - m`; the mean transmission is `1 - 2m/3` and the pass applies
the inverse gain so a flat field keeps its brightness. The gain can push
the passing stripe past 1.0 for bright saturated colours, so a soft knee
above 0.8 replaces hard clipping. Defaults: strength 0.30, 3-pixel triad.
Options: 4-pixel triad (coarser), slot mask (period locked to whole
pixels, subtle), none. The mask fades out below 6 output pixels per
source pixel.

On an SDR panel a full-strength mask cannot be made bright; that needs the
HDR headroom Sony-Megatron-style shaders use, which SDL's OpenGL path does
not expose. Strength around 0.3 with the knee is the honest SDR setting.

### Glow and halation

Bright content spreads light into the glass and neighbouring phosphors.
The beam output is downsampled to quarter resolution and blurred
(separable Gaussian, about 1 source pixel wide) for phosphor glow, and to
one-sixteenth resolution for halation (about 5 source pixels wide). The
composition adds `g * glow + h * halation` and scales the direct term by
`1 - g - h` so energy is conserved (defaults g = 0.08, h = 0.05). This is
what makes highlights bloom softly, fills the gaps between bright lines,
and reads as "television" rather than "filter".

### Geometry

Optional cylindrical curvature (flat vertically, curved horizontally, as a
Trinitron), 0 to 3%, default 1.5%; rounded corners with a radius of about
1.5% of the height; a faint vignette (2-3% at the corners). The image is
shrunk about 1% so the curved edge stays inside the viewport. No drawn
bezel, no reflections.

### Output

Linear to sRGB encode, then triangular dither of half a code value
(interleaved gradient noise, no texture) so the beam's smooth gradients do
not band in the 8-bit backbuffer.

## Pipeline

All passes are GLSL 1.20 on the existing legacy 2.1 context. Probed on this
Mac (`GL 2.1 Metal - 91.7`): `GL_ARB_texture_float`,
`GL_EXT_framebuffer_object`, `GL_ARB_framebuffer_sRGB`, `GL_EXT_texture_sRGB`,
`GL_ARB_texture_non_power_of_two`, `GL_EXT_framebuffer_blit`, and a 16384
maximum texture size are all present. Half-float render targets use
`GL_RGBA16F_ARB`.

| Pass | Target                         | Work |
|------|--------------------------------|------|
| P0   | existing 8-bit frame texture   | upload after the screen-color LUT (unchanged) |
| P1   | A: viewport_w x 224, RGBA16F   | sRGB decode, horizontal Gaussian resample |
| P2   | B: viewport_w x viewport_h, RGBA16F | vertical beam (per-channel sigma), mask, gain, knee |
| P3   | C: B/4, D: B/4, E: B/16 (x2)   | downsample, two 1-D blurs each for glow and halation |
| P4   | backbuffer or capture FBO      | curvature remap, glow and halation add, corners, vignette, sRGB encode, dither |

Targets are created once per drawable size and recreated on resize. The
full-resolution targets are B (about 62 MB) and the backbuffer; the rest
are tiny. Estimated GPU cost on the M3 Max is under a millisecond per
frame; pacing must be re-measured (see Verification).

The ImGui overlay draws after P4 in the backbuffer, so the pause menu is
not affected by the tube.

### Relation to the upscaler

With the tube on, the upscaler combo (Nearest, Bilinear, Reconstruct) is
bypassed and shown disabled with a note: P1 is the scaler. Chaining
Reconstruct's dither decoding ahead of P1 is possible later (render
Reconstruct into A at 2x source instead of the decode-and-resample) but is
not part of this plan; a real tube blurs dithers by itself.

## Fit with the code

| Area | Change |
|------|--------|
| `runner/desktop_crt.h/.c` (new) | Pure C model: settings struct, presets, and the per-frame derivation (sigma pair, mask pitch and gain, fade thresholds from the viewport scale, curvature amounts). No GL, so it is unit-testable like `desktop_viewport.c`. Name lookups for env and config. |
| `runner/desktop_present_sdl.c` | Refactor `Dkc2SdlPresenterPresent` so one `RenderFrame(target)` draws the chosen chain into the window or the capture FBO (the capture currently repeats the quad; with a multi-pass chain that duplication is a bug). Add the FBO pool, the four programs, uniform plumbing, and `Dkc2SdlPresenterSetDisplay(...)`. Shader failures fall back to the flat path and report in `shader_error`, as Reconstruct does. |
| `runner/desktop_present_sdl.h` | `kDkc2DisplayFlat = 0`, `kDkc2DisplayCrt = 1`; state and uniform locations. |
| `runner/desktop_launcher.c/.h` | Keys `Display=`, `CrtPreset=`, `CrtScanlines=`, `CrtSharpness=`, `CrtMask=`, `CrtMaskStrength=`, `CrtGlow=`, `CrtHalation=`, `CrtCurvature=` (integers, percent where continuous), getters and setters, defaults. |
| `runner/sdl_main.c` | Env overrides for one run: `DKC2_DISPLAY=flat|crt`, `DKC2_CRT_PRESET=`, `DKC2_CRT_*=`; apply on settings change; write the display name into the diagnostics backend string and the run report. |
| `runner/desktop_overlay.cpp` | Settings page: a "Display" combo (Flat panel / CRT television) above the upscaler, a preset combo and the sliders shown only when CRT is chosen (same pattern as the Reconstruct block), the upscaler combo disabled with a note while CRT is on. Relabel the existing "Screen model" combo "Phosphor colors" so the two CRT-named things stop colliding. |
| `tests/test_desktop_crt.c` (new) | Beam normalisation (numerically integrate the periodic sum for sigma 0.15 to 0.6: mean equals L within 1e-3; white peak below 1.0), mask gain formula, fade thresholds, preset round-trip through name and config parsing. |
| `scripts/crt_capture_compare.py` (new) | Given a flat and a CRT PPM from the same state: mean linear luminance ratio, row-profile spectrum (the only strong period must be the line pitch; any lower-frequency envelope above 2% fails), and a side-by-side crop. |
| Docs | README (Display section beside the upscaler paragraph), `docs/ARCHITECTURE.md` (presenter chapter, after Reconstruct), `docs/DESKTOP_TESTING.md` (capture recipe and env), `docs/IMPLEMENTATION_JOURNAL.md`, `CHANGELOG.md`. |

Presets (starting values, to be tuned with the owner at fullscreen):

| Preset | Scanlines (sigma_dark) | Sharpness (sigma_h) | Mask | Glow / halation | Curvature |
|--------|------------------------|---------------------|------|-----------------|-----------|
| Living room (default) | 0.28 | 0.32 | grille 3 px, 0.30 | 0.08 / 0.05 | 1.5% |
| Studio monitor        | 0.22 | 0.26 | grille 3 px, 0.40 | 0.05 / 0.03 | 0% |
| Soft                  | 0.36 | 0.42 | grille 4 px, 0.20 | 0.12 / 0.08 | 2% |

## Phases

1. **Foundation.** The pure C model and its unit test; the presenter
   refactor with the FBO pool and shared `RenderFrame`; the capture path
   verified to still produce the byte-identical Reconstruct screenshot it
   produces today. No visible change.
2. **Tube.** P1, P2, P4 without glow or curvature: beam and mask only.
   Settings, env, menu, config persistence. First fullscreen review with
   the owner using the corpus states (Pirate Panic, an underwater stage,
   Bramble Blast, the Lost World).
3. **Glass.** P3 glow and halation, curvature, corners, vignette, dither,
   presets. Second review; tune defaults; write docs and the journal
   entry.
4. **Ship.** Pacing measurement, the compare script in CI form, CHANGELOG,
   release notes. Port to DKC3Recomp's presenter (same lineage; check
   `git status` first, another session shares that tree).

Each phase is one working session; phase 2 is where the look is decided,
so it should not be rushed into phase 3.

## Verification

- **Native oracle.** Present-only by construction; the 4:3 hash tests and
  the widescreen corpus run unchanged. No PPU or video-code file changes.
- **Energy.** `DKC2_DESKTOP_SCREENSHOT` captures from the same absolute
  `DKC2_DESKTOP_TEST_LOADSTATE` with `DKC2_DISPLAY=flat` and `crt`; mean
  linear luminance within 3%. The capture must run the whole chain, which
  the phase-1 refactor guarantees.
- **No beating.** Row-mean spectrum from the compare script: line pitch
  only, no envelope. Run for all three aspects, since each has a different
  fractional pitch.
- **Pacing.** `DKC2_PACING_LOG` plus `scripts/analyze_pacing_log.py` at
  16:9 fullscreen with the tube on: refreshes per frame all 1, as today.
- **Panel-size captures from a hidden run.** The hidden test window is
  small; add `DKC2_DESKTOP_TEST_WINDOW=1728x1117` (points, so the Retina
  drawable is the full panel) or confirm hidden fullscreen-desktop gives
  the 3456-wide drawable. To be settled in phase 1.
- **Owner review** at fullscreen, both themes of content: a bright daytime
  stage and a dark one, since the beam model behaves differently in each.

## Risks and answers

- **Dim result.** Mask strength above about 0.4 will look dim on a 500-nit
  SDR panel no matter the gain, because the knee has to hold whites.
  Default 0.3, and the glow adds perceived brightness back.
- **Moire.** Aperture grille is vertical-only, so it cannot beat with the
  line pitch. Slot mask has vertical structure; keep its period an integer
  number of pixels and its strength low.
- **Banding.** Half-float intermediates and the output dither.
- **Colour fringing** from per-channel beam widths is real CRT behaviour;
  keep the sigma spread small and expose it through the presets only.
- **Pacing.** Two full-resolution passes; measure before merging. The
  target sizes are fixed per drawable, so there are no per-frame
  allocations.
- **Legacy OpenGL.** It is a Metal-backed 2.1 on macOS 27 and works today.
  The math lives in C and straightforward GLSL, so a future Metal presenter
  ports the passes directly.
- **Windowed play at 1x or 2x.** The fades derived from the viewport make
  the tube degrade to a soft flat image; nothing aliases.

## Out of scope

Composite NTSC artefact simulation, drawn bezels or reflections, HDR or EDR
output, black-frame insertion or phosphor persistence on the 120 Hz panel
(halves brightness), interlacing (DKC2 is 224p).

## Decisions taken as assumptions

- The default preset is "Living room" with gentle curvature on. The owner
  asked for an older television, not a studio monitor.
- The tube bypasses the upscaler rather than chaining it.
- "Screen model" is relabelled "Phosphor colors"; its keys and values do
  not change.
