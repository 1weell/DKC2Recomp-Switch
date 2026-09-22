#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <switch.h>
#include <switch/runtime/pad.h>
#include <SDL.h>

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "dkc2_game.h"
#include "dkc2_kongs.h"
#include "dkc2_music.h"
#include "dkc2_video.h"
#include "host_report.h"
#include "verified_rom.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
  kSwitchOutputWidth = 1280,
  kSwitchOutputHeight = 720,
};

static const char kSwitchRoot[] = "sdmc:/switch/DKC2Recomp";
static const char kSwitchRuntime[] = "sdmc:/switch/DKC2Recomp/.runtime";
static const char kSwitchRom[] =
    "sdmc:/switch/DKC2Recomp/DKC2-USA-v1.0.sfc";

static void EnsureDirectory(const char *path) {
  if (mkdir(path, 0755) != 0) {
    /* An existing directory is expected on every launch. */
  }
}

static uint32_t MapNpad(uint64_t keys) {
  uint32_t input = 0;
  /* Keep the physical face-button positions consistent with the SNES pad:
   * Switch B/Y/A/X are respectively bottom/left/right/top, while the packed
   * SNES bits are B/Y/A/X in that same physical order. */
  if (keys & HidNpadButton_B) input |= 1u << 0;      /* SNES B */
  if (keys & HidNpadButton_Y) input |= 1u << 1;      /* SNES Y */
  if (keys & HidNpadButton_Minus) input |= 1u << 2;  /* Select */
  if (keys & HidNpadButton_Plus) input |= 1u << 3;   /* Start */
  if (keys & HidNpadButton_Up) input |= 1u << 4;
  if (keys & HidNpadButton_Down) input |= 1u << 5;
  if (keys & HidNpadButton_Left) input |= 1u << 6;
  if (keys & HidNpadButton_Right) input |= 1u << 7;
  if (keys & HidNpadButton_A) input |= 1u << 8;      /* SNES A */
  if (keys & HidNpadButton_X) input |= 1u << 9;      /* SNES X */
  if (keys & HidNpadButton_L) input |= 1u << 10;
  if (keys & HidNpadButton_R) input |= 1u << 11;
  return input;
}

static void SwitchAudioCallback(void *userdata, Uint8 *stream, int length) {
  (void)userdata;
  if (!stream || length <= 0) return;
  const int frame_bytes = (int)(sizeof(int16_t) * 2);
  const int frames = length / frame_bytes;
  SDL_memset(stream, 0, (size_t)length);
  if (frames > 0)
    RtlRenderAudio((int16_t *)stream, frames, 2);
}

static SDL_AudioDeviceID InitSwitchAudio(void) {
  SDL_AudioSpec desired = {0};
  SDL_AudioSpec obtained = {0};
  desired.freq = 32040;
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = 534;
  desired.callback = SwitchAudioCallback;
  SDL_AudioDeviceID device = SDL_OpenAudioDevice(
      NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (!device) {
    host_report_breadcrumb("[audio] SDL audio unavailable; continuing muted");
    return 0;
  }
  if (obtained.format != AUDIO_S16SYS || obtained.channels != 2) {
    host_report_breadcrumb("[audio] unsupported SDL audio format; continuing muted");
    SDL_CloseAudioDevice(device);
    return 0;
  }
  RtlSetAudioOutputRate(obtained.freq);
  SDL_PauseAudioDevice(device, 0);
  host_report_breadcrumb("[audio] SDL DSP output started");
  return device;
}

static bool InitPresenter(SDL_Window **window_out, SDL_Renderer **renderer_out,
                          SDL_Texture **texture_out, char *error,
                          size_t error_size) {
  const int frame_width = Dkc2VideoWidth();
  SDL_Window *window = SDL_CreateWindow(
      "Donkey Kong Country 2: Recompiled", SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED, kSwitchOutputWidth, kSwitchOutputHeight,
      SDL_WINDOW_SHOWN);
  if (!window) {
    snprintf(error, error_size, "SDL_CreateWindow: %s", SDL_GetError());
    return false;
  }
  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    snprintf(error, error_size, "SDL_CreateRenderer: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    return false;
  }
  SDL_Texture *texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
      frame_width, kDkc2VideoHeight);
  if (!texture) {
    snprintf(error, error_size, "SDL_CreateTexture: %s", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return false;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  SDL_RenderSetLogicalSize(renderer, kSwitchOutputWidth, kSwitchOutputHeight);
  *window_out = window;
  *renderer_out = renderer;
  *texture_out = texture;
  return true;
}

int main(int argc, char **argv) {
  EnsureDirectory("sdmc:/switch");
  EnsureDirectory(kSwitchRoot);
  EnsureDirectory(kSwitchRuntime);
  host_report_init("DKC2RecompSwitch", "0.0.1-switch");
  host_report_breadcrumb("[runtime] Switch host starting");

  const char *rom_path = argc > 1 ? argv[1] : kSwitchRom;
  size_t rom_size = 0;
  char rom_error[256] = {0};
  uint8_t *rom = Dkc2ReadVerifiedRom(
      rom_path, &rom_size, rom_error, sizeof rom_error);
  if (!rom) {
    host_report_fatal(rom_error);
    return EXIT_FAILURE;
  }

  Dkc2KongsInitialize();
  Dkc2MusicInitialize();
  RtlRegisterGame(Dkc2GameInfo());
  /* The original project already has a validated 16:9 presentation path.
   * Activate it before PPU initialization so all host buffers use its 342px
   * logical frame width (256 + 43 columns per side). */
  Dkc2VideoSetAspect(kDkc2VideoAspect16x9);
  if (!SnesInit(rom, (int)rom_size)) {
    host_report_fatal("SnesInit rejected the verified ROM");
    free(rom);
    return EXIT_FAILURE;
  }
  RtlSetSaveRoot(kSwitchRuntime);
  RtlEnsureSaveDir();
  RtlReadSram();
  host_report_breadcrumb("[runtime] ROM and SNES runtime initialized");

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    host_report_fatal(SDL_GetError());
    free(rom);
    return EXIT_FAILURE;
  }
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;
  char presenter_error[256] = {0};
  if (!InitPresenter(&window, &renderer, &texture, presenter_error,
                     sizeof presenter_error)) {
    host_report_fatal(presenter_error);
    SDL_Quit();
    free(rom);
    return EXIT_FAILURE;
  }

  uint32_t *pixels = (uint32_t *)calloc(
      (size_t)Dkc2VideoWidth() * kDkc2VideoHeight, sizeof(*pixels));
  if (!pixels) {
    host_report_fatal("framebuffer allocation failed");
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(rom);
    return EXIT_FAILURE;
  }
  Dkc2BeginDrawing((uint8_t *)pixels,
                   (size_t)Dkc2VideoWidth() * sizeof(*pixels));
  host_report_breadcrumb("[gfx] SDL renderer created");
  SDL_AudioDeviceID audio_device = InitSwitchAudio();

  padConfigureInput(2, HidNpadStyleSet_NpadStandard);
  PadState pad1;
  PadState pad2;
  padInitialize(&pad1, HidNpadIdType_No1, HidNpadIdType_Handheld);
  padInitialize(&pad2, HidNpadIdType_No2, HidNpadIdType_Handheld);
  uint64_t frequency = SDL_GetPerformanceFrequency();
  uint64_t deadline = SDL_GetPerformanceCounter();
  uint64_t frame_ticks = frequency / 60u;
  bool running = true;
  bool first_frame = true;
  while (running && appletMainLoop()) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
      if (event.type == SDL_QUIT) running = false;

    padUpdate(&pad1);
    padUpdate(&pad2);
    uint32_t inputs = MapNpad(padGetButtons(&pad1));
    inputs |= MapNpad(padGetButtons(&pad2)) << 12;
    if (first_frame) {
      host_report_breadcrumb("[runtime] first frame input loop");
      first_frame = false;
    }
    (void)RtlRunFrame(inputs);
    Dkc2DrawPpuFrame();
    SDL_UpdateTexture(texture, NULL, pixels,
                      Dkc2VideoWidth() * (int)sizeof(*pixels));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    deadline += frame_ticks;
    while (running) {
      uint64_t now = SDL_GetPerformanceCounter();
      if (now >= deadline) break;
      SDL_Delay(1);
    }
  }

  RtlWriteSram();
  host_report_breadcrumb("[runtime] clean shutdown");
  if (audio_device) {
    SDL_PauseAudioDevice(audio_device, 1);
    SDL_CloseAudioDevice(audio_device);
  }
  free(pixels);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  free(rom);
  return EXIT_SUCCESS;
}
