#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <switch.h>
#include <switch/runtime/pad.h>
#include <SDL.h>
#include <SDL_test_font.h>
#include "switch_settings.h"

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "snes/dsp.h"
#include "snes/snes.h"
#include "dkc2_game.h"
#include "dkc2_kongs.h"
#include "dkc2_music.h"
#include "dkc2_video.h"
#include "host_report.h"
#include "verified_rom.h"
#include "switch_policy.h"
#include "switch_sram.h"
#include <errno.h>

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

static bool EnsureDirectory(const char *path) {
  if (mkdir(path, 0755) == 0) return true;
  struct stat info;
  if (errno == EEXIST && stat(path, &info) == 0 && S_ISDIR(info.st_mode)) return true;
  fprintf(stderr, "Directory %s: %s\n", path, strerror(errno));
  return false;
}

static void ShowError(const char *message) {
  if (!consoleInit(NULL)) { fprintf(stderr, "%s\n", message); return; }
  printf("DKC2Recomp\n\n%s\n\nPress + to exit.\n", message);
  PadState pad;
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);
  while (appletMainLoop()) {
    padUpdate(&pad);
    if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
    consoleUpdate(NULL);
  }
  consoleExit(NULL);
}

static unsigned char *s_saved_sram;
static SwitchSettings s_settings;
static int s_volume = 10; /* Changed only while the SDL audio device is paused. */
static bool s_save_failed;
static bool s_config_failed;
static const char kSettingsPath[] = "sdmc:/switch/DKC2Recomp/.runtime/settings.bin";

static void LoadSettings(void) {
  uint8_t bytes[12];
  s_settings = SwitchSettingsDefaults();
  if (!SwitchSramRead(kSettingsPath, bytes, sizeof bytes) ||
      !SwitchSettingsDecode(bytes, &s_settings)) {
    if (!SwitchSramRead("sdmc:/switch/DKC2Recomp/.runtime/settings.bin.bak", bytes, sizeof bytes) ||
        !SwitchSettingsDecode(bytes, &s_settings))
      host_report_breadcrumb("[config] defaults (missing or invalid settings)");
    else host_report_breadcrumb("[config] recovered backup");
  }
  s_volume = s_settings.volume;
}
static void StoreSettings(void) {
  uint8_t bytes[12];
  SwitchSettingsEncode(&s_settings, bytes);
  s_config_failed = !SwitchSramWrite(kSettingsPath, bytes, sizeof bytes);
  host_report_breadcrumb("[config] save=%s", s_config_failed ? "failed" : "ok");
}
static void DrawText(SDL_Renderer *renderer, int x, int y, const char *text) {
  float sx, sy;
  SDL_RenderGetScale(renderer, &sx, &sy);
  SDL_RenderSetScale(renderer, sx * 3, sy * 3);
  SDLTest_DrawString(renderer, x / 3, y / 3, text);
  SDL_RenderSetScale(renderer, sx, sy);
}
static void DrawMenu(SDL_Renderer *renderer, int selected, bool confirm,
                     bool audio_available) {
  SDL_SetRenderDrawColor(renderer, 12, 20, 32, 255);
  SDL_RenderClear(renderer);
  SDL_SetRenderDrawColor(renderer, 230, 240, 255, 255);
  DrawText(renderer, 90, 60, "DKC2 - PAUSADO");
  char rows[7][80];
  snprintf(rows[0], 80, "RETOMAR");
  snprintf(rows[1], 80, "VIDEO: %s (PROXIMA ABERTURA)", s_settings.wide ? "16:9" : "4:3");
  snprintf(rows[2], 80, "AUDIO: %s  VOLUME %d", audio_available ? (s_settings.volume ? "ATIVO" : "MUDO") : "INDISPONIVEL", s_settings.volume);
  snprintf(rows[3], 80, "ESCALA: %s", s_settings.smooth ? "SUAVE" : "NITIDA");
  snprintf(rows[4], 80, "CONTROLES: MENU %s", s_settings.shortcut ? "L+R+MENOS" : "ZL+ZR");
  snprintf(rows[5], 80, "%s", confirm ? "REINICIAR? A CONFIRMA / B CANCELA" : "REINICIAR");
  snprintf(rows[6], 80, "SALVAR E SAIR");
  for (int i = 0; i < 7; ++i) {
    SDL_SetRenderDrawColor(renderer, i == selected ? 255 : 210,
        i == selected ? 210 : 220, i == selected ? 70 : 235, 255);
    DrawText(renderer, 90, 135 + i * 60, rows[i]);
  }
  SDL_SetRenderDrawColor(renderer, 190, 205, 220, 255);
  DrawText(renderer, 90, 600, "DIRECIONAL: NAVEGAR  A: MUDAR  B: VOLTAR");
  if (s_save_failed || s_config_failed) {
    SDL_SetRenderDrawColor(renderer, 255, 110, 100, 255);
    DrawText(renderer, 90, 660, "FALHA AO SALVAR - VERIFIQUE O CARTAO SD");
  }
  SDL_RenderPresent(renderer);
}
static bool SaveChangedSram(void) {
  if (!g_sram || g_sram_size <= 0) return true;
  if (s_saved_sram && memcmp(s_saved_sram, g_sram, (size_t)g_sram_size) == 0) return true;
  char path[256];
  RtlSramFilePath(path, sizeof path);
  if (!SwitchSramWrite(path, g_sram, (size_t)g_sram_size)) {
    s_save_failed = true;
    host_report_breadcrumb("[save] write failed; retained backup, will retry");
    return false;
  }
  if (s_saved_sram) memcpy(s_saved_sram, g_sram, (size_t)g_sram_size);
  s_save_failed = false;
  host_report_breadcrumb("[save] SRAM committed");
  return true;
}

typedef struct SwitchLifecycle {
  SDL_AudioDeviceID audio;
  bool refocus;
  bool save_failed;
} SwitchLifecycle;

/* libnx dispatches hooks while the main thread processes applet messages.
 * Stop the SDL callback before the OS suspends this applet. */
static void LifecycleHook(AppletHookType hook, void *opaque) {
  SwitchLifecycle *life = opaque;
  if (hook == AppletHookType_OnFocusState &&
      appletGetFocusState() != AppletFocusState_InFocus) {
    if (life->audio) SDL_PauseAudioDevice(life->audio, 1);
    life->save_failed = !SaveChangedSram();
    life->refocus = true;
  }
  if (hook == AppletHookType_OnResume || hook == AppletHookType_OnFocusState)
    life->refocus = true;
}

typedef struct SwitchAudioStats {
  uint32_t callbacks, empty_callbacks, minimum, maximum;
} SwitchAudioStats;
static SwitchAudioStats s_audio_stats = {.minimum = UINT32_MAX};

enum { kPerfFrames = 600, kPerfPhases = 4 };
static uint64_t s_phase_ticks[kPerfPhases][kPerfFrames];
static unsigned s_perf_count, s_late_frames;
static int CompareTicks(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}
static void RecordPerf(const uint64_t *times, uint64_t frequency, bool late) {
  static const char *names[] = {"simulation", "ppu", "upload", "present"};
  for (unsigned i = 0; i < kPerfPhases; ++i)
    s_phase_ticks[i][s_perf_count] = times[i];
  s_late_frames += late;
  if (++s_perf_count < kPerfFrames) return;
  for (unsigned i = 0; i < kPerfPhases; ++i) {
    uint64_t sum = 0;
    for (unsigned j = 0; j < kPerfFrames; ++j) sum += s_phase_ticks[i][j];
    qsort(s_phase_ticks[i], kPerfFrames, sizeof(uint64_t), CompareTicks);
    double ms = 1000.0 / (double)frequency;
    host_report_breadcrumb("[perf] %s mean=%.3f p95=%.3f p99=%.3f max=%.3f ms",
        names[i], sum * ms / kPerfFrames, s_phase_ticks[i][569] * ms,
        s_phase_ticks[i][593] * ms, s_phase_ticks[i][599] * ms);
  }
  host_report_breadcrumb("[perf] frames=%u late=%u", s_perf_count, s_late_frames);
  SwitchAudioStats audio;
  RtlApuLock();
  audio = s_audio_stats;
  s_audio_stats = (SwitchAudioStats){.minimum = UINT32_MAX};
  RtlApuUnlock();
  host_report_breadcrumb("[audio] callbacks=%u empty_at_entry=%u ring_min=%u ring_max=%u",
      audio.callbacks, audio.empty_callbacks, audio.callbacks ? audio.minimum : 0, audio.maximum);
  s_perf_count = s_late_frames = 0;
}

/* Ensure the portable policy follows the installed libnx ABI exactly. */
#define CHECK_NPAD(name, bit) _Static_assert(HidNpadButton_##name == (UINT64_C(1) << bit), "Npad ABI changed")
CHECK_NPAD(B, 1); CHECK_NPAD(Y, 3); CHECK_NPAD(Minus, 11); CHECK_NPAD(Plus, 10);
CHECK_NPAD(Up, 13); CHECK_NPAD(Down, 15); CHECK_NPAD(Left, 12); CHECK_NPAD(Right, 14);
CHECK_NPAD(A, 0); CHECK_NPAD(X, 2); CHECK_NPAD(L, 6); CHECK_NPAD(R, 7);
#undef CHECK_NPAD

static void SwitchAudioCallback(void *userdata, Uint8 *stream, int length) {
  (void)userdata;
  if (!stream || length <= 0) return;
  const int frame_bytes = (int)(sizeof(int16_t) * 2);
  const int frames = length / frame_bytes;
  SDL_memset(stream, 0, (size_t)length);
  if (frames > 0) {
    RtlApuLock();
    uint32_t available = dsp_available(g_snes->apu->dsp);
    ++s_audio_stats.callbacks;
    s_audio_stats.empty_callbacks += available == 0;
    if (available < s_audio_stats.minimum) s_audio_stats.minimum = available;
    if (available > s_audio_stats.maximum) s_audio_stats.maximum = available;
    RtlRenderAudio((int16_t *)stream, frames, 2);
    RtlApuUnlock();
    int16_t *samples = (int16_t *)stream;
    for (int i = 0; i < frames * 2; ++i)
      samples[i] = (int16_t)((int32_t)samples[i] * s_volume / 10);
  }
}

static SDL_AudioDeviceID InitSwitchAudio(void) {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    host_report_breadcrumb("[audio] subsystem unavailable: %s", SDL_GetError());
    return 0;
  }
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
    host_report_breadcrumb("[audio] unavailable: %s", SDL_GetError());
    return 0;
  }
  if (obtained.format != AUDIO_S16SYS || obtained.channels != 2) {
    host_report_breadcrumb("[audio] unsupported SDL audio format; continuing muted");
    SDL_CloseAudioDevice(device);
    return 0;
  }
  RtlSetAudioOutputRate(obtained.freq);
  /* Remain paused until the first simulated frame is ready. */
  host_report_breadcrumb("[audio] rate=%d format=%u channels=%u samples=%u bytes=%u",
      obtained.freq, obtained.format, obtained.channels, obtained.samples, obtained.size);
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
  if (!renderer) {
    host_report_breadcrumb("[gfx] accelerated renderer failed: %s; software fallback", SDL_GetError());
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
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
  SDL_SetTextureScaleMode(texture, s_settings.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
  SDL_RenderSetLogicalSize(renderer, kSwitchOutputWidth, kSwitchOutputHeight);
  SDL_RendererInfo info;
  int width = 0, height = 0;
  SDL_GetRendererOutputSize(renderer, &width, &height);
  if (SDL_GetRendererInfo(renderer, &info) == 0)
    host_report_breadcrumb("[gfx] renderer=%s flags=%u output=%dx%d frame=%dx%d",
        info.name, info.flags, width, height, frame_width, kDkc2VideoHeight);
  *window_out = window;
  *renderer_out = renderer;
  *texture_out = texture;
  return true;
}

int main(int argc, char **argv) {
  if (!EnsureDirectory("sdmc:/switch") || !EnsureDirectory(kSwitchRoot) ||
      !EnsureDirectory(kSwitchRuntime)) {
    ShowError("Cannot create runtime directory on SD card.");
    return EXIT_FAILURE;
  }
  host_report_set_output_directory(kSwitchRuntime);
  host_report_init("DKC2RecompSwitch", DKC2_SWITCH_BUILD);
  host_report_breadcrumb("[build] compiler=%s built=%s %s", DKC2_SWITCH_COMPILER, __DATE__, __TIME__);
  host_report_breadcrumb("[runtime] Switch host starting");

  LoadSettings();
  const char *rom_path = argc > 1 ? argv[1] : kSwitchRom;
  size_t rom_size = 0;
  char rom_error[256] = {0};
  uint8_t *rom = Dkc2ReadVerifiedRom(
      rom_path, &rom_size, rom_error, sizeof rom_error);
  if (!rom) {
    host_report_fatal(rom_error);
    ShowError(rom_error);
    return EXIT_FAILURE;
  }

  Dkc2KongsInitialize();
  Dkc2MusicInitialize();
  RtlRegisterGame(Dkc2GameInfo());
  /* Aspect changes apply on next launch, before framebuffer allocation. */
  Dkc2VideoSetAspect(s_settings.wide ? kDkc2VideoAspect16x9 : kDkc2VideoAspectNative);
  if (!SnesInit(rom, (int)rom_size)) {
    host_report_fatal("SnesInit rejected the verified ROM");
    ShowError("SnesInit rejected the verified ROM");
    free(rom);
    return EXIT_FAILURE;
  }
  RtlSetSaveRoot(kSwitchRuntime);
  RtlEnsureSaveDir();
  char sram_path[256];
  RtlSramFilePath(sram_path, sizeof sram_path);
  int recovered = SwitchSramRead(sram_path, g_sram, (size_t)g_sram_size);
  host_report_breadcrumb("[save] read=%d (0=new/unreadable 1=primary 2=backup)", recovered);
  s_saved_sram = malloc((size_t)g_sram_size);
  if (!s_saved_sram) {
    host_report_fatal("SRAM tracking allocation failed");
    ShowError("SRAM tracking allocation failed");
    free(rom);
    return EXIT_FAILURE;
  }
  memcpy(s_saved_sram, g_sram, (size_t)g_sram_size);
  host_report_breadcrumb("[runtime] ROM and SNES runtime initialized");

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    char error[256];
    snprintf(error, sizeof error, "SDL initialization: %s", SDL_GetError());
    host_report_fatal(error);
    SDL_Quit();
    ShowError(error);
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
    ShowError(presenter_error);
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
    ShowError("Framebuffer allocation failed");
    free(rom);
    return EXIT_FAILURE;
  }
  Dkc2BeginDrawing((uint8_t *)pixels,
                   (size_t)Dkc2VideoWidth() * sizeof(*pixels));
  host_report_breadcrumb("[gfx] SDL renderer created");
  SDL_AudioDeviceID audio_device = InitSwitchAudio();
  SwitchLifecycle life = {.audio = audio_device};
  AppletHookCookie lifecycle_cookie;
  appletHook(&lifecycle_cookie, LifecycleHook, &life);
  if (appletGetAppletType() == AppletType_Application) {
    Result result = appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleepNotify);
    host_report_breadcrumb("[lifecycle] focus notification mode: 0x%x", result);
  }

  padConfigureInput(2, HidNpadStyleSet_NpadFullCtrl);
  PadState pad1;
  PadState pad2;
  padInitialize(&pad1, HidNpadIdType_No1, HidNpadIdType_Handheld);
  padInitialize(&pad2, HidNpadIdType_No2);
  uint64_t frequency = SDL_GetPerformanceFrequency();
  uint64_t deadline = SDL_GetPerformanceCounter();
  uint64_t frame_ticks = frequency / 60u;
  uint64_t next_save = deadline + frequency * 30;
  bool focused = true;
  bool running = true;
  bool menu = false, confirm_reset = false, chord_held = false;
  bool release_inputs = false;
  int selected = 0;
  bool connected1 = false, connected2 = false;
  bool first_frame = true;
  while (running && appletMainLoop()) {
    if (life.refocus) {
      deadline = SDL_GetPerformanceCounter();
      s_perf_count = s_late_frames = 0;
      life.refocus = false;
    }
    if (life.save_failed) { s_save_failed = true; life.save_failed = false; }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
          focused = false;
          if (audio_device) SDL_PauseAudioDevice(audio_device, 1);
          (void)SaveChangedSram();
        } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
          focused = true;
          deadline = SDL_GetPerformanceCounter();
          /* Audio resumes only after gameplay produces a frame. */
        }
      }
    }
    if (!running) break;
    if (!focused || appletGetFocusState() != AppletFocusState_InFocus) { SDL_Delay(20); continue; }

    padUpdate(&pad1);
    padUpdate(&pad2);
    bool now1 = padIsConnected(&pad1), now2 = padIsConnected(&pad2);
    if (now1 != connected1 || now2 != connected2) {
      host_report_breadcrumb("[input] P1=%s P2=%s", now1 ? "connected" : "disconnected",
          now2 ? "connected" : "disconnected");
      connected1 = now1; connected2 = now2;
      release_inputs = true;
    }
    uint64_t held = padGetButtons(&pad1);
    uint64_t down = padGetButtonsDown(&pad1);
    uint64_t chord = s_settings.shortcut ?
        HidNpadButton_L | HidNpadButton_R | HidNpadButton_Minus :
        HidNpadButton_ZL | HidNpadButton_ZR;
    bool chord_now = (held & chord) == chord;
    if (chord_now && !chord_held) {
      menu = !menu;
      release_inputs = true;
      confirm_reset = false;
      deadline = SDL_GetPerformanceCounter();
      if (audio_device) SDL_PauseAudioDevice(audio_device, 1);
      if (menu) (void)SaveChangedSram();
    }
    chord_held = chord_now;
    if (menu) {
      if (down & HidNpadButton_B) {
        if (confirm_reset) confirm_reset = false;
        else { menu = false; StoreSettings(); }
      }
      if (!confirm_reset) {
        if (down & HidNpadButton_Up) selected = (selected + 6) % 7;
        if (down & HidNpadButton_Down) selected = (selected + 1) % 7;
      }
      if (down & (HidNpadButton_A | HidNpadButton_Left | HidNpadButton_Right)) {
        int delta = down & HidNpadButton_Left ? -1 : 1;
        bool activate = (down & HidNpadButton_A) != 0;
        if (selected == 0 && activate) { menu = false; StoreSettings(); }
        if (selected == 1) s_settings.wide = !s_settings.wide;
        if (selected == 2) {
          s_settings.volume = (s_settings.volume + delta + 11) % 11;
          s_volume = s_settings.volume;
        }
        if (selected == 3) s_settings.smooth = !s_settings.smooth;
        if (selected == 4) s_settings.shortcut = !s_settings.shortcut;
        if (selected == 5 && activate) {
          if (confirm_reset) {
            if (SaveChangedSram()) { RtlReset(1); menu = false; }
            confirm_reset = false;
          } else confirm_reset = true;
        }
        if (selected == 6 && activate) {
          StoreSettings();
          if (SaveChangedSram() && !s_config_failed) running = false;
        }
        if (selected >= 1 && selected <= 4) StoreSettings();
      }
      SDL_SetTextureScaleMode(texture, s_settings.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
      release_inputs = true;
      DrawMenu(renderer, selected, confirm_reset, audio_device != 0);
      deadline = SDL_GetPerformanceCounter();
      s_perf_count = s_late_frames = 0;
      SDL_Delay(16);
      continue;
    }
    HidAnalogStickState stick1 = padGetStickPos(&pad1, 0);
    HidAnalogStickState stick2 = padGetStickPos(&pad2, 0);
    uint32_t inputs = SwitchDirections(SwitchMapNpad(padGetButtons(&pad1)), stick1.x, stick1.y);
    inputs |= SwitchDirections(SwitchMapNpad(padGetButtons(&pad2)), stick2.x, stick2.y) << 12;
    if (release_inputs) {
      if (!held && !padGetButtons(&pad2) && !inputs) release_inputs = false;
      inputs = 0;
    }
    if (first_frame) {
      host_report_breadcrumb("[runtime] first frame input loop");
      first_frame = false;
    }
    uint64_t phase_times[4];
    uint64_t phase_start = SDL_GetPerformanceCounter();
    (void)RtlRunFrame(inputs);
    phase_times[0] = SDL_GetPerformanceCounter() - phase_start;
    if (audio_device && SDL_GetAudioDeviceStatus(audio_device) == SDL_AUDIO_PAUSED)
      SDL_PauseAudioDevice(audio_device, 0);
    phase_start = SDL_GetPerformanceCounter();
    Dkc2DrawPpuFrame();
    phase_times[1] = SDL_GetPerformanceCounter() - phase_start;
    phase_start = SDL_GetPerformanceCounter();
    SDL_UpdateTexture(texture, NULL, pixels,
                      Dkc2VideoWidth() * (int)sizeof(*pixels));
    phase_times[2] = SDL_GetPerformanceCounter() - phase_start;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_Rect destination = {0, 0, 1280, 720};
    if (Dkc2VideoGetAspect() == kDkc2VideoAspectNative)
      destination = (SDL_Rect){160, 0, 960, 720};
    SDL_RenderCopy(renderer, texture, NULL, &destination);
    if (s_save_failed || s_config_failed) {
      SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
      DrawText(renderer, 30, 30, "FALHA AO SALVAR - VERIFIQUE O SD");
    }
    phase_start = SDL_GetPerformanceCounter();
    SDL_RenderPresent(renderer);
    uint64_t presented = SDL_GetPerformanceCounter();
    phase_times[3] = presented - phase_start;
    RecordPerf(phase_times, frequency, presented > deadline + frame_ticks);

    uint64_t present_time = SDL_GetPerformanceCounter();
    if (present_time >= next_save) {
      (void)SaveChangedSram();
      next_save = SDL_GetPerformanceCounter() + frequency * 30;
    }
    deadline = SwitchDeadline(deadline + frame_ticks,
                              SDL_GetPerformanceCounter(), frame_ticks);
    while (running) {
      uint64_t now = SDL_GetPerformanceCounter();
      if (now >= deadline) break;
      SDL_Delay(1);
    }
  }

  appletUnhook(&lifecycle_cookie);
  if (audio_device) SDL_PauseAudioDevice(audio_device, 1);
  bool final_save_ok = SaveChangedSram();
  free(s_saved_sram);
  host_report_breadcrumb("[runtime] clean shutdown");
  if (audio_device) {
    SDL_PauseAudioDevice(audio_device, 1);
    SDL_CloseAudioDevice(audio_device);
  }
  free(pixels);
  SDL_DestroyTexture(texture);
  SDLTest_CleanupTextDrawing();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  if (!final_save_ok) ShowError("Save failed. Previous backup retained.");
  free(rom);
  return EXIT_SUCCESS;
}
