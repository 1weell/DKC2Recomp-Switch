#include "dkc2_music.h"
#include "dkc2_msu1.h"
#include "dkc2_spc_music.h"
#include "snes/apu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { kMuteOffset = 0x2E02D3 };
static Dkc2Msu1 *s_player;
static struct Apu *s_apu;
static uint8_t *s_rom;
static size_t s_size;
static char s_directory[4096], s_status[256] = "Original SNES music";
static bool s_enabled, s_replacement, s_policy_ready, s_started;
static unsigned s_song, s_variant;
static int s_gain = 100;

static void ApplyPolicy(void) {
  s_policy_ready = false;
  if (!s_rom || s_size <= kMuteOffset + 1) return;
  uint8_t *branch = s_rom + kMuteOffset;
  if (!((branch[0] == 0xD0 && branch[1] == 5) ||
        (branch[0] == 0 && branch[1] == 0))) return;
  /* Only the checksum-verified resident image is changed; never the ROM file.
   * SFX uploads and the SPC command handshake retain their original paths. */
  const bool mute = s_enabled && s_replacement;
  branch[0] = mute ? 0 : 0xD0;
  branch[1] = mute ? 0 : 5;
  s_policy_ready = Dkc2RestoreSpcMusicPolicy(s_apu, s_rom, s_size) != 0;
}

static void SelectCurrent(void) {
  s_replacement = s_enabled && s_started &&
      Dkc2Msu1Select(s_player, s_song, s_variant);
  if (!s_replacement) Dkc2Msu1Reset(s_player);
  if (!s_enabled)
    snprintf(s_status, sizeof s_status, "Original SNES music");
  else if (!s_player)
    snprintf(s_status, sizeof s_status, "Choose an extracted MSU-1 music folder");
  else if (s_replacement)
    snprintf(s_status, sizeof s_status, "MSU-1 track %u", Dkc2Msu1CurrentTrack(s_player));
  else if (s_started && s_song)
    snprintf(s_status, sizeof s_status, "Track %u unavailable; using SNES music",
             Dkc2Msu1TrackNumber(s_song, s_variant));
  else
    snprintf(s_status, sizeof s_status, "MSU-1 ready; waiting for music");
  if (getenv("DKC2_MSU1_TRACE")) fprintf(stderr, "music: %s song=%u variant=%u\n", s_status, s_song, s_variant);
  ApplyPolicy();
}

bool Dkc2MusicLoad(const char *directory) {
  char error[256];
  Dkc2Msu1 *next = Dkc2Msu1Open(directory, error, sizeof error);
  if (!next) {
    snprintf(s_status, sizeof s_status, "%s", error);
    return false;
  }
  Dkc2Msu1Close(s_player);
  s_player = next;
  snprintf(s_directory, sizeof s_directory, "%s", directory);
  Dkc2Msu1SetGain(s_player, s_gain);
  s_enabled = true;
  SelectCurrent();
  return true;
}
void Dkc2MusicSetEnabled(bool enabled) { s_enabled = enabled; SelectCurrent(); }
bool Dkc2MusicEnabled(void) { return s_enabled; }
void Dkc2MusicSetGain(int percent) {
  s_gain = percent < 0 ? 0 : percent > 200 ? 200 : percent;
  Dkc2Msu1SetGain(s_player, s_gain);
}
int Dkc2MusicGain(void) { return s_gain; }
const char *Dkc2MusicDirectory(void) { return s_directory; }
const char *Dkc2MusicStatus(void) { return s_status; }
bool Dkc2MusicSaveSettings(void) {
  FILE *f = fopen("msu1.cfg", "w");
  if (!f) return false;
  bool ok = fprintf(f, "%d %d\n%s\n", s_enabled ? 1 : 0, s_gain, s_directory) > 0;
  return fclose(f) == 0 && ok;
}
void Dkc2MusicInitialize(void) {
  char path[4096] = "", line[4096];
  int enabled = 0, gain = 100;
  FILE *f = fopen("msu1.cfg", "r");
  if (f) {
    if (fgets(line, sizeof line, f)) (void)sscanf(line, "%d %d", &enabled, &gain);
    if (fgets(path, sizeof path, f)) path[strcspn(path, "\r\n")] = 0;
    fclose(f);
  }
  const char *gain_override = getenv("DKC2_MSU1_VOLUME");
  if (gain_override && *gain_override) {
    char *end = NULL;
    const long parsed = strtol(gain_override, &end, 10);
    if (!*end && parsed >= 0 && parsed <= 200) gain = (int)parsed;
  }
  Dkc2MusicSetGain(gain);
  const char *override = getenv("DKC2_MSU1_PATH");
  if (override) {
    snprintf(path, sizeof path, "%s", override);
    enabled = *override != 0;
  }
  if (*path && !Dkc2MusicLoad(path)) {
    fprintf(stderr, "MSU-1: %s\n", s_status);
    s_enabled = false;
  } else Dkc2MusicSetEnabled(enabled != 0);
}

void Dkc2MusicCommand(uint16_t song, uint16_t command) {
  const unsigned action = command & 255u;
  if (action == 255) {
    s_song = song & 255u;
    s_variant = 0;
    s_started = false;
    SelectCurrent();
  } else if (action == 251) {
    s_song = song & 255u;
    s_variant = command >> 8;
    s_started = true;
    SelectCurrent();
  } else if (action == 254 && !s_started) {
    s_song = song & 255u;
    s_started = true;
    SelectCurrent();
  }
}

void Dkc2MusicStateLoaded(struct Apu *apu, const uint8_t *ram) {
  s_song = ram ? ram[0x1C] : 0;
  s_variant = 0;
  /* $E5/E6 is the stable song-sequence table base. A state contains no host
   * PCM position, so restart the restored song/variant, including on rewind. */
  if (apu) {
    const unsigned base = apu->ram[0xE5] | (unsigned)apu->ram[0xE6] << 8;
    if (base != 0x1300) for (unsigned i = 1; i <= 5; ++i) {
      const unsigned a = 0x1312 + 2 * i;
      if (base == (apu->ram[a] | (unsigned)apu->ram[a + 1] << 8)) {
        s_variant = i;
        break;
      }
    }
  }
  s_started = true;
  SelectCurrent();
}
void Dkc2MusicFrame(struct Apu *apu, uint8_t *rom, size_t size, const uint8_t *ram) {
  (void)ram;
  s_apu = apu;
  s_rom = rom;
  s_size = size;
  ApplyPolicy();
}
void Dkc2MusicMix(int16_t *samples, int frames, int rate) {
  if (s_enabled && s_replacement && s_policy_ready)
    Dkc2Msu1Mix(s_player, samples, frames, 2, rate);
}
