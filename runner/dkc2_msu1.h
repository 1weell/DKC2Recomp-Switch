#ifndef DKC2_MSU1_H
#define DKC2_MSU1_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct Dkc2Msu1 Dkc2Msu1;
Dkc2Msu1 *Dkc2Msu1Open(const char *directory, char *error, size_t error_size);
void Dkc2Msu1Close(Dkc2Msu1 *player);
unsigned Dkc2Msu1TrackNumber(unsigned song, unsigned variant);
bool Dkc2Msu1Select(Dkc2Msu1 *player, unsigned song, unsigned variant);
void Dkc2Msu1Reset(Dkc2Msu1 *player);
void Dkc2Msu1SetGain(Dkc2Msu1 *player, int percent);
void Dkc2Msu1Mix(Dkc2Msu1 *player, int16_t *samples, int frames, int channels, int output_rate);
unsigned Dkc2Msu1CurrentTrack(const Dkc2Msu1 *player);
const char *Dkc2Msu1Directory(const Dkc2Msu1 *player);
#endif
