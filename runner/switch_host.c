#include "host_report.h"
#include "spc_player.h"
#include "types.h"

#include <switch.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Original SPC audio is rendered by the shared DSP; no HLE SPC adapter. */
bool g_new_ppu = true;

static void SwitchSpcInitialize(SpcPlayer *player) { (void)player; }
static void SwitchSpcUpload(SpcPlayer *player, const uint8_t *data) {
  (void)player;
  (void)data;
}

static SpcPlayer g_switch_spc_player = {
    .initialize = SwitchSpcInitialize,
    .upload = SwitchSpcUpload,
};

SpcPlayer *g_spc_player = &g_switch_spc_player;

static FILE *g_switch_log;
static int g_switch_fatal;
static int g_log_attempted;
static unsigned g_log_lines;
static char g_log_directory[384] = "sdmc:/switch/DKC2Recomp/.runtime";
/* Static recursive lock exists before SnesInit and outlives SDL_CloseAudioDevice.
 * Nested runtime hooks may acquire it again on the producer thread. */
static RMutex g_apu_mutex;

static void OpenLog(void) {
  if (g_log_attempted) return;
  g_log_attempted = 1;
  char path[512], previous[512];
  snprintf(path, sizeof path, "%s/boot.log", g_log_directory);
  snprintf(previous, sizeof previous, "%s/boot.previous.log", g_log_directory);
  if (remove(previous) != 0 && errno != ENOENT) {
    fprintf(stderr, "Cannot rotate log: %s\n", strerror(errno));
    return;
  }
  if (rename(path, previous) != 0 && errno != ENOENT) {
    fprintf(stderr, "Cannot preserve boot.log: %s\n", strerror(errno));
    return;
  }
  g_switch_log = fopen(path, "w");
  if (!g_switch_log) fprintf(stderr, "Cannot open boot.log: %s\n", strerror(errno));
}

static void SwitchLogV(const char *format, va_list args) {
  OpenLog();
  if (!g_switch_log) { vfprintf(stderr, format, args); fputc('\n', stderr); return; }
  if (g_log_lines++ < 4096) {
    vfprintf(g_switch_log, format, args);
    fputc('\n', g_switch_log);
    fflush(g_switch_log);
  }
}

void NORETURN Die(const char *error) {
  host_report_fatal(error ? error : "unknown error");
  fprintf(stderr, "fatal: %s\n", error ? error : "unknown error");
  exit(EXIT_FAILURE);
}

void RtlApuLock(void) { rmutexLock(&g_apu_mutex); }
void RtlApuUnlock(void) { rmutexUnlock(&g_apu_mutex); }

void host_report_init(const char *game_name, const char *build_version) {
  OpenLog();
  host_report_breadcrumb("[runtime] %s %s", game_name ? game_name : "game",
                         build_version ? build_version : "dev");
}

void host_report_set_output_directory(const char *directory) {
  if (directory && *directory && !g_log_attempted &&
      strlen(directory) < sizeof g_log_directory)
    snprintf(g_log_directory, sizeof g_log_directory, "%s", directory);
}

void host_report_breadcrumb(const char *format, ...) {
  va_list args;
  va_start(args, format);
  SwitchLogV(format, args);
  va_end(args);
}

void host_report_fatal(const char *message) {
  g_switch_fatal = 1;
  if (g_log_lines >= 4096) g_log_lines = 4095;
  host_report_breadcrumb("[fatal] %s", message ? message : "unknown error");
}

int host_report_has_fatal(void) { return g_switch_fatal; }
void host_report_dump_json(FILE *stream) {
  if (stream) fprintf(stream, "\"switch\":{\"fatal\":%s},\n", g_switch_fatal ? "true" : "false");
}
const char *host_report_write_minidump(void *info) {
  (void)info;
  return NULL;
}
const char *host_report_preserve_crash_copy(const char *path) {
  (void)path;
  return NULL;
}
void host_report_crash_test_tick(void) {}
