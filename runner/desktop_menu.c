#include "desktop_menu.h"

static int *Selection(Dkc2MenuState *state, unsigned command, unsigned *base) {
  if (command >= kDkc2MenuAspect && command < kDkc2MenuAspect + 4) {
    *base = kDkc2MenuAspect; return &state->aspect;
  }
  if (command >= kDkc2MenuUpscaler && command < kDkc2MenuUpscaler + 3) {
    *base = kDkc2MenuUpscaler; return &state->upscaler;
  }
  if (command >= kDkc2MenuReconstruct && command < kDkc2MenuReconstruct + 5) {
    *base = kDkc2MenuReconstruct; return &state->reconstruct;
  }
  if (command >= kDkc2MenuEdge && command < kDkc2MenuEdge + 4) {
    *base = kDkc2MenuEdge; return &state->edge;
  }
  if (command >= kDkc2MenuScreen && command < kDkc2MenuScreen + 4) {
    *base = kDkc2MenuScreen; return &state->screen;
  }
  if (command >= kDkc2MenuPlayer1 && command < kDkc2MenuPlayer1 + 3) {
    *base = kDkc2MenuPlayer1; return &state->player_source[0];
  }
  if (command >= kDkc2MenuPlayer2 && command < kDkc2MenuPlayer2 + 3) {
    *base = kDkc2MenuPlayer2; return &state->player_source[1];
  }
  if (command >= kDkc2MenuCoop && command < kDkc2MenuCoop + 2) {
    *base = kDkc2MenuCoop; return &state->coop;
  }
  return 0;
}

bool Dkc2MenuApply(Dkc2MenuState *state, unsigned command) {
  if (!state) return false;
  unsigned base = 0;
  int *value = Selection(state, command, &base);
  if (!value) return false;
  *value = (int)(command - base);
  /* Selecting a reconstruction level also enables the reconstruction shader. */
  if (base == kDkc2MenuReconstruct) state->upscaler = 2;
  return true;
}

bool Dkc2MenuSelected(const Dkc2MenuState *state, unsigned command) {
  if (!state) return false;
  if (command == kDkc2MenuFullscreen) return state->fullscreen;
  if (command == kDkc2MenuPause) return state->paused;
  Dkc2MenuState copy = *state;
  unsigned base = 0;
  int *value = Selection(&copy, command, &base);
  return value && *value == (int)(command - base);
}
