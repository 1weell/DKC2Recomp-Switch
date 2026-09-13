#include "desktop_menu_settings.h"
#include "desktop_launcher.h"
#include "dkc2_coop.h"
#include "dkc2_video.h"

Dkc2MenuState Dkc2MenuReadSettings(const RecompLauncherCSettings *settings,
                                  bool fullscreen, bool paused,
                                  bool settings_available,
                                  bool reconstruct_available) {
  Dkc2MenuState state = {0};
  state.aspect = settings->aspect_index;
  state.upscaler = reconstruct_available && Dkc2LauncherUpscaler() == 2
      ? 2 : (settings->texture_filter != 0);
  state.reconstruct = Dkc2LauncherReconstructMode();
  state.edge = Dkc2LauncherWidescreenEdge();
  state.screen = settings->screen_kind;
  state.fullscreen = fullscreen;
  state.paused = paused;
  state.player_source[0] = settings->player_src[0];
  state.player_source[1] = settings->player_src[1];
  state.coop = Dkc2LauncherCoopMode();
  state.settings_available = settings_available;
  state.reconstruct_available = reconstruct_available;
  return state;
}

bool Dkc2MenuChangeSettings(RecompLauncherCSettings *settings,
                            Dkc2MenuState *state, unsigned command) {
  if (!state->reconstruct_available &&
      (command == kDkc2MenuUpscaler + 2 ||
       (command >= kDkc2MenuReconstruct && command < kDkc2MenuReconstruct + 5)))
    return false;
  if (!Dkc2MenuApply(state, command)) return false;
  settings->aspect_index = state->aspect;
  settings->widescreen = state->aspect != kDkc2VideoAspectNative;
  settings->texture_filter = state->upscaler == 1;
  settings->screen_kind = state->screen;
  settings->player_src[0] = state->player_source[0];
  settings->player_src[1] = state->player_source[1];
  if ((command >= kDkc2MenuUpscaler && command < kDkc2MenuUpscaler + 3) ||
      (command >= kDkc2MenuReconstruct && command < kDkc2MenuReconstruct + 5)) {
    Dkc2LauncherSetUpscaler(state->upscaler);
    Dkc2LauncherSetReconstructMode(state->reconstruct);
  }
  Dkc2LauncherSetWidescreenEdge(state->edge);
  if (command >= kDkc2MenuEdge && command < kDkc2MenuEdge + 4)
    Dkc2VideoSetEdgePolicy((Dkc2VideoEdgePolicy)state->edge);
  Dkc2LauncherSetCoopMode(state->coop);
  if (command >= kDkc2MenuCoop && command < kDkc2MenuCoop + 2)
    Dkc2CoopSetMode((Dkc2CoopMode)state->coop);
  return true;
}
