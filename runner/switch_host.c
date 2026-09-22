#include "host_report.h"
#include "spc_player.h"
#include "types.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* The first Switch milestone intentionally keeps the audio device out of the
 * host. The shared runtime still needs an SPC player object for reset/upload
 * callbacks, so provide the same safe no-op adapter used by the headless host.
 * The real SDL/libnx audio consumer will replace this without changing the
 * game or SNES runtime ABI. */
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

static void SwitchLogV(const char *format, va_list args) {
  if (!g_switch_log) g_switch_log = fopen("boot.log", "w");
  if (g_switch_log) {
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

void RtlApuLock(void) {}
void RtlApuUnlock(void) {}

void host_report_init(const char *game_name, const char *build_version) {
  if (!g_switch_log) g_switch_log = fopen("boot.log", "w");
  host_report_breadcrumb("[runtime] %s %s", game_name ? game_name : "game",
                         build_version ? build_version : "dev");
}

void host_report_set_output_directory(const char *directory) {
  (void)directory;
}

void host_report_breadcrumb(const char *format, ...) {
  va_list args;
  va_start(args, format);
  SwitchLogV(format, args);
  va_end(args);
}

void host_report_fatal(const char *message) {
  host_report_breadcrumb("[fatal] %s", message ? message : "unknown error");
}

int host_report_has_fatal(void) { return 0; }
void host_report_dump_json(FILE *stream) { (void)stream; }
const char *host_report_write_minidump(void *info) {
  (void)info;
  return NULL;
}
const char *host_report_preserve_crash_copy(const char *path) {
  (void)path;
  return NULL;
}
void host_report_crash_test_tick(void) {}
