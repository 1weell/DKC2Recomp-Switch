#ifndef DKC2_DESKTOP_MENU_H
#define DKC2_DESKTOP_MENU_H

#include <stdbool.h>

/* Stable native-menu IDs; ranges follow the existing persisted setting enums. */
enum {
  kDkc2MenuPause = 1000, kDkc2MenuSave, kDkc2MenuLoad,
  kDkc2MenuFullscreen, kDkc2MenuQuit, kDkc2MenuAbout, kDkc2MenuSettings,
  kDkc2MenuAspect = 1100,       /* three aspects */
  kDkc2MenuUpscaler = 1200,     /* nearest, bilinear, reconstruct */
  kDkc2MenuReconstruct = 1300,  /* five reconstruction levels */
  kDkc2MenuEdge = 1400,         /* reflect, bars, shift, glide */
  kDkc2MenuPlayer1 = 1600,     /* none, keyboard, gamepad */
  kDkc2MenuPlayer2 = 1700,
  kDkc2MenuCoop = 1800,        /* simultaneous, classic */
  kDkc2MenuScreen = 1500,       /* raw, CRT, composite, Trinitron */
};

typedef struct Dkc2MenuState {
  int aspect, upscaler, reconstruct, edge, screen;
  bool fullscreen;
  int player_source[2], coop;
  bool paused, settings_available, reconstruct_available;
} Dkc2MenuState;

/* Only presentation selections mutate state. Actions are dispatched by the host. */
bool Dkc2MenuApply(Dkc2MenuState *state, unsigned command);
bool Dkc2MenuSelected(const Dkc2MenuState *state, unsigned command);

#endif
