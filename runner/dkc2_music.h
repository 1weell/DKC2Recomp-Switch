#ifndef DKC2_MUSIC_H
#define DKC2_MUSIC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct Apu;
void Dkc2MusicInitialize(void);
bool Dkc2MusicLoad(const char *directory);
void Dkc2MusicSetEnabled(bool enabled);
bool Dkc2MusicEnabled(void);
void Dkc2MusicSetGain(int percent);
int Dkc2MusicGain(void);
const char *Dkc2MusicDirectory(void);
const char *Dkc2MusicStatus(void);
bool Dkc2MusicSaveSettings(void);
void Dkc2MusicFrame(struct Apu *apu, uint8_t *rom, size_t size, const uint8_t *ram);
void Dkc2MusicCommand(uint16_t song, uint16_t command);
void Dkc2MusicStateLoaded(struct Apu *apu, const uint8_t *ram);
void Dkc2MusicMix(int16_t *samples, int frames, int rate);
#ifdef __cplusplus
}
#endif
#endif
