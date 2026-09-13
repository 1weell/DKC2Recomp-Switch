#ifndef DKC2_DESKTOP_CRT_H
#define DKC2_DESKTOP_CRT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CRT television display model: the present-only tube simulation the macOS
 * presenter can draw instead of the flat upscaled frame. This module holds
 * the settings, the presets, and the per-frame derivation of shader
 * parameters from the viewport, with no OpenGL, so it is unit-testable and
 * shared by the launcher, the overlay, and the presenter.
 *
 * The model, in linear light:
 *  - Vertical beam: each source line is a Gaussian centred on its row whose
 *    width grows with the line's brightness per channel, normalised so the
 *    average over one line pitch equals the source brightness for any
 *    width. Bright lines widen and merge; dark lines stay thin.
 *  - Horizontal response: a Gaussian resample in source pixels, the video
 *    bandwidth softness of an RGB or S-video signal.
 *  - Phosphor mask: an aperture grille (or slot mask) in output pixels with
 *    the inverse of its mean transmission applied as gain and a soft knee
 *    above `knee` instead of hard clipping.
 *  - Glow and halation: blurred copies added energy-neutrally.
 *  - Geometry: cylindrical curvature, rounded corners, a faint vignette.
 * Below a few output pixels per source line or column the beam and the mask
 * fade out, derived from the viewport rather than from a setting.
 */

enum {
  kDkc2DisplayFlat = 0,
  kDkc2DisplayCrt = 1,
  kDkc2DisplayCount = 2,
};

enum {
  kDkc2CrtPresetLivingRoom = 0,
  kDkc2CrtPresetStudio = 1,
  kDkc2CrtPresetSoft = 2,
  kDkc2CrtPresetCustom = 3,
  kDkc2CrtPresetCount = 4,
};

enum {
  kDkc2CrtMaskNone = 0,
  kDkc2CrtMaskGrilleFine = 1,   /* 3-pixel RGB triad */
  kDkc2CrtMaskGrilleCoarse = 2, /* 6-pixel RRGGBB triad */
  kDkc2CrtMaskSlot = 3,         /* 6-pixel triad with staggered slots */
  kDkc2CrtMaskCount = 4,
};

/* Persisted settings; every value is an integer so launcher.cfg and the
 * overlay sliders share them. Percentages run 0..100. */
typedef struct Dkc2CrtSettings {
  int preset;        /* kDkc2CrtPreset*; Custom once a slider moves */
  int scanlines;     /* 0 none .. 100 thin: sigma_dark 0.50 .. 0.18 lines */
  int sharpness;     /* 0 soft .. 100 sharp: sigma_h 0.60 .. 0.25 pixels */
  int mask;          /* kDkc2CrtMask* */
  int mask_strength; /* 0..100 */
  int glow;          /* 0..100 -> 0 .. 0.20 of the light */
  int halation;      /* 0..100 -> 0 .. 0.20 of the light */
  int curvature;     /* 0..100 -> 0 .. 3% */
} Dkc2CrtSettings;

/* Shader parameters for one frame, derived from the settings and the
 * viewport the frame is drawn into. */
typedef struct Dkc2CrtFrameParams {
  float sigma_dark;    /* beam width (line units) of a black line */
  float sigma_bright;  /* beam width of a white line */
  float sigma_h;       /* horizontal Gaussian width in source pixels */
  float beam_fade;     /* 0 flat .. 1 full beam, from pixels per line */
  int mask;            /* effective kDkc2CrtMask*, None when faded out */
  float mask_pitch;    /* output pixels per triad */
  float mask_strength; /* effective 0..1 */
  float mask_gain;     /* inverse of the mask's mean transmission */
  float knee;          /* soft-knee start, 0..1 */
  float glow;          /* fraction of the light from the glow blur */
  float halation;      /* fraction of the light from the halation blur */
  float curvature_x;   /* horizontal warp coefficient */
  float curvature_y;   /* vertical warp coefficient */
  float corner_radius; /* fraction of the viewport height, 0 = square */
  float vignette;      /* darkening at the corners, 0..1 */
  float scale_x;       /* output pixels per source pixel */
  float scale_y;       /* output pixels per source line */
} Dkc2CrtFrameParams;

void Dkc2CrtSettingsDefault(Dkc2CrtSettings *settings);
/* Overwrite every slider with a preset's values; false for Custom or an
 * invalid preset (settings untouched). */
bool Dkc2CrtSettingsApplyPreset(Dkc2CrtSettings *settings, int preset);
void Dkc2CrtSettingsClamp(Dkc2CrtSettings *settings);
/* Derive one frame's parameters. False for an empty viewport or source. */
bool Dkc2CrtDerive(const Dkc2CrtSettings *settings, int viewport_width,
                   int viewport_height, int source_width,
                   int source_height, Dkc2CrtFrameParams *params);

const char *Dkc2CrtDisplayName(int display);
bool Dkc2CrtDisplayFromName(const char *name, int *display);
const char *Dkc2CrtPresetName(int preset);
bool Dkc2CrtPresetFromName(const char *name, int *preset);
const char *Dkc2CrtMaskName(int mask);
bool Dkc2CrtMaskFromName(const char *name, int *mask);

/* Reference implementations of the shader arithmetic, for tests and for
 * offline comparison. lines[] holds one channel's linear value per source
 * line; y is the continuous line coordinate (line i spans i..i+1). */
float Dkc2CrtBeamSample(const float *lines, int count, float y,
                        float sigma_dark, float sigma_bright);
float Dkc2CrtMaskGain(int mask, float strength);
float Dkc2CrtSoftKnee(float value, float knee);

#ifdef __cplusplus
}
#endif

#endif
