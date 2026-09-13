#pragma once
#include "desktop_menu.h"
#include "recomp_launcher.h"

Dkc2MenuState Dkc2MenuReadSettings(const RecompLauncherCSettings *settings,
                                  bool fullscreen, bool paused,
                                  bool settings_available,
                                  bool reconstruct_available);
bool Dkc2MenuChangeSettings(RecompLauncherCSettings *settings,
                            Dkc2MenuState *state, unsigned command);
