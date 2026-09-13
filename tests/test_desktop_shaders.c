#define SDL_MAIN_HANDLED
#include "desktop_present_gl.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include "desktop_present_sdl.h"
#include "desktop_filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
enum { W = 32, H = 24, OW = 448, OH = 288, BYTES = OW * OH * 3 };
static uint8_t native_rgb[BYTES], sdl_rgb[BYTES], baseline[BYTES], tuned[BYTES];
static uint8_t source[W * H * 4], filtered[W * H * 4];

static void Capture(void *user, int width, int height) {
  (void)user;
  if (width != OW || height != OH) abort();
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadBuffer(GL_BACK);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, native_rgb);
  for (int y = 0; y < height / 2; ++y) {
    uint8_t row[OW * 3];
    memcpy(row, native_rgb + y * OW * 3, sizeof row);
    memcpy(native_rgb + y * OW * 3, native_rgb + (height - 1 - y) * OW * 3, sizeof row);
    memcpy(native_rgb + (height - 1 - y) * OW * 3, row, sizeof row);
  }
}

int main(void) {
  SDL_SetMainReady();
  CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
  WNDCLASSA wc = {0};
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = GetModuleHandleA(NULL);
  wc.lpszClassName = "Dkc2ShaderTest";
  CHECK(RegisterClassA(&wc));
  HWND window = CreateWindowA(wc.lpszClassName, "Synthetic shader test", WS_POPUP,
                             0, 0, OW, OH, NULL, NULL, wc.hInstance, NULL);
  CHECK(window);
  char error[512] = {0};
  Dkc2DesktopGlPresenter native = {0};
  CHECK(Dkc2DesktopGlPresenterInit(&native, window, false, error, sizeof error));
  CHECK(Dkc2DesktopGlReconstructAvailable(&native));
  Dkc2SdlPresenter sdl = {0};
  CHECK(Dkc2SdlPresenterInit(&sdl, 2, 0, true, false, W, H, error, sizeof error));
  SDL_SetWindowSize((SDL_Window *)sdl.window, OW, OH);
  CHECK(sdl.program != 0);
  printf("WGL: %s; SDL: %s\n", Dkc2DesktopGlVersion(&native), Dkc2SdlPresenterBackend(&sdl));
  /* Checkerboard dithers, shallow slopes, gradients and asymmetric colors. */
  for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
    uint8_t *p = source + (y * W + x) * 4;
    p[0] = (uint8_t)(x < 12 ? ((x + y) % 2 ? 25 : 210) : 30 + y * 6);
    p[1] = (uint8_t)(x > y * 2 / 3 ? 190 : 20);
    p[2] = (uint8_t)(x > y / 3 + 15 ? 230 : x * 4);
    p[3] = 255;
  }
  RECT client = {0, 0, OW, OH};
  for (int screen = 0; screen < 4; ++screen) {
    Dkc2DesktopColorFilter filter;
    CHECK(Dkc2DesktopColorFilterInit(&filter, screen));
    const uint8_t *pixels = Dkc2DesktopColorFilterApply(&filter, source, filtered, W * H);
    CHECK(pixels);
    /* nearest, bilinear, all five modes, and independent tuning endpoints */
    for (int setting = 0; setting < 10; ++setting) {
      int upscaler = setting < 2 ? setting : 2;
      int mode = setting < 7 ? (setting > 1 ? setting - 2 : 0) : 4;
      float strength = setting == 7 ? 0.0f : 1.0f;
      float softness = setting == 8 ? 1.0f : 0.5f;
      float shading = setting == 9 ? 0.0f : 0.6f;
      CHECK(Dkc2DesktopGlSetUpscaler(&native, upscaler, setting == 1, mode,
                                    strength, softness, shading) == upscaler);
      CHECK(Dkc2DesktopGlPresent(&native, &client, pixels, W, H, setting == 1, Capture, NULL));
      CHECK(Dkc2SdlPresenterSetUpscaler(&sdl, upscaler, mode, strength, softness, shading) == upscaler);
      Dkc2SdlPresenterArmCapture(&sdl, sdl_rgb, OW, OH);
      CHECK(Dkc2SdlPresenterPresent(&sdl, pixels, W, H, NULL, NULL));
      CHECK(sdl.capture_done);
      size_t changed = 0, differing = 0;
      int max_delta = 0;
      for (size_t i = 0; i < BYTES; ++i) {
        if (native_rgb[i] != sdl_rgb[i]) ++differing;
        int delta = abs((int)native_rgb[i] - sdl_rgb[i]);
        if (delta > max_delta) max_delta = delta;
        if (native_rgb[i] != baseline[i]) ++changed;
      }
      printf("screen=%d setting=%d: differing=%zu max_delta=%d\n", screen, setting, differing, max_delta);
      /* Default framebuffer and RGBA8 FBO conversion can round a few
       * channels differently. Reject differences beyond one 8-bit step. */
      CHECK(max_delta <= 1 && differing < BYTES / 1000);
      if (setting == 6) memcpy(tuned, native_rgb, BYTES);
      if (setting >= 7) CHECK(memcmp(tuned, native_rgb, BYTES) != 0);
      if (setting == 0) memcpy(baseline, native_rgb, BYTES);
      else CHECK(changed > 100);
      printf("screen=%d setting=%d: %zu bytes differ from nearest\n", screen, setting, changed);
    }
    Dkc2DesktopColorFilterDestroy(&filter);
  }
  CHECK(Dkc2DesktopGlCrtAvailable(&native));
  CHECK(Dkc2GlPipelineCrtAvailable(&sdl));
  for (int setting = 0; setting < 13; ++setting) {
    Dkc2CrtSettings crt;
    Dkc2CrtSettingsDefault(&crt);
    if (setting < 3) CHECK(Dkc2CrtSettingsApplyPreset(&crt, setting));
    else {
      crt.preset = kDkc2CrtPresetCustom;
      switch (setting) {
        case 3: crt.scanlines = 0; break;
        case 4: crt.sharpness = 0; break;
        case 5: crt.mask_strength = 100; break;
        case 6: crt.glow = 100; break;
        case 7: crt.halation = 100; break;
        case 8: crt.curvature = 0; break;
        default: crt.mask = setting - 9; break;
      }
    }
    CHECK(Dkc2DesktopGlSetDisplay(&native, kDkc2DisplayCrt, &crt) == kDkc2DisplayCrt);
    CHECK(Dkc2DesktopGlPresent(&native, &client, source, W, H, false, Capture, NULL));
    CHECK(Dkc2SdlPresenterSetDisplay(&sdl, kDkc2DisplayCrt, &crt) == kDkc2DisplayCrt);
    Dkc2SdlPresenterArmCapture(&sdl, sdl_rgb, OW, OH);
    CHECK(Dkc2SdlPresenterPresent(&sdl, source, W, H, NULL, NULL));
    CHECK(sdl.capture_done);
    size_t differing = 0;
    int max_delta = 0;
    for (size_t i = 0; i < BYTES; ++i) {
      int delta = abs((int)native_rgb[i] - sdl_rgb[i]);
      if (delta) ++differing;
      if (delta > max_delta) max_delta = delta;
    }
    printf("CRT setting=%d: differing=%zu max_delta=%d\n", setting, differing, max_delta);
    CHECK(max_delta <= 1 && differing < BYTES / 1000);
    if (setting == 0) memcpy(baseline, native_rgb, BYTES);
    else if (setting != 10) CHECK(memcmp(baseline, native_rgb, BYTES) != 0);
  }
  Dkc2SdlPresenterDestroy(&sdl);
  Dkc2DesktopGlPresenterDestroy(&native);
  CHECK(!Dkc2DesktopGlReconstructAvailable(&native));
  CHECK(Dkc2DesktopGlSetUpscaler(&native, 2, true, 4, 1, 1, 1) == 1);
  DestroyWindow(window);
  SDL_Quit();
  puts("All screen models, reconstruction levels and tuning controls passed on both renderers.");
  return 0;
}
