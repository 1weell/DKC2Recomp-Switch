#include "desktop_menu.h"
#include "dkc2_video.h"
#include "desktop_present_sdl.h"
#include <stdio.h>

#define CHECK(value) do { if (!(value)) { \
  fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); return 1; \
} } while (0)

int main(void) {
  CHECK(kDkc2VideoAspectCount == 4 && kDkc2VideoAspectNative == 0);
  CHECK(kDkc2VideoEdgePolicyCount == 4 && kDkc2VideoEdgeReflect == 0);
  CHECK(kDkc2UpscalerCount == 3 && kDkc2UpscalerReconstruct == 2);
  Dkc2MenuState state = {0, 0, 3, 3, 0, false};
  static const unsigned starts[] = {kDkc2MenuAspect, kDkc2MenuUpscaler,
      kDkc2MenuReconstruct, kDkc2MenuEdge, kDkc2MenuScreen,
      kDkc2MenuPlayer1, kDkc2MenuPlayer2, kDkc2MenuCoop};
  static const unsigned counts[] = {4, 3, 5, 4, 4, 3, 3, 2};
  for (unsigned group = 0; group < 8; ++group) {
    for (unsigned i = 0; i < counts[group]; ++i) {
      CHECK(Dkc2MenuApply(&state, starts[group] + i));
      for (unsigned j = 0; j < counts[group]; ++j)
        CHECK(Dkc2MenuSelected(&state, starts[group] + j) == (i == j));
    }
    CHECK(!Dkc2MenuApply(&state, starts[group] + counts[group]));
  }
  CHECK(state.aspect == 3 && state.upscaler == 2 && state.reconstruct == 4);
  CHECK(state.edge == 3 && state.screen == 3);
  CHECK(!Dkc2MenuApply(&state, kDkc2MenuFullscreen));
  CHECK(!Dkc2MenuSelected(&state, kDkc2MenuFullscreen));
  state.fullscreen = true;
  CHECK(Dkc2MenuSelected(&state, kDkc2MenuFullscreen));
  CHECK(!Dkc2MenuApply(NULL, kDkc2MenuAspect));
  CHECK(!Dkc2MenuSelected(NULL, kDkc2MenuAspect));
  puts("Desktop menu selections, bounds, enum parity and checkmarks passed");
  return 0;
}
