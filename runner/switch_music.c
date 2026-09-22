#include "dkc2_music.h"

#include <stddef.h>
#include <stdint.h>

/* First Switch milestone: keep the original SPC path alive, but do not load
 * desktop MSU-1 files or touch POSIX memory mapping. The host audio queue will
 * replace this adapter once video/input boot is stable. */
static const char kSwitchMusicStatus[] = "Switch: original SNES audio path";

void Dkc2MusicInitialize(void) {}

bool Dkc2MusicLoad(const char *directory) {
  (void)directory;
  return false;
}

void Dkc2MusicSetEnabled(bool enabled) { (void)enabled; }
bool Dkc2MusicEnabled(void) { return false; }
void Dkc2MusicSetGain(int percent) { (void)percent; }
int Dkc2MusicGain(void) { return 100; }
const char *Dkc2MusicDirectory(void) { return ""; }
const char *Dkc2MusicStatus(void) { return kSwitchMusicStatus; }
bool Dkc2MusicSaveSettings(void) { return false; }

void Dkc2MusicFrame(struct Apu *apu, uint8_t *rom, size_t size,
                    const uint8_t *ram) {
  (void)apu;
  (void)rom;
  (void)size;
  (void)ram;
}

void Dkc2MusicCommand(uint16_t song, uint16_t command) {
  (void)song;
  (void)command;
}

void Dkc2MusicStateLoaded(struct Apu *apu, const uint8_t *ram) {
  (void)apu;
  (void)ram;
}

void Dkc2MusicMix(int16_t *samples, int frames, int rate) {
  (void)samples;
  (void)frames;
  (void)rate;
}
