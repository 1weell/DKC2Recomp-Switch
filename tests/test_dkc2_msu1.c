#include "dkc2_msu1.h"
#include "dkc2_music.h"
#include "snes/apu.h"
#include "snes/dsp.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static uint8_t rom[0x400000], ram[0x20000];
static Apu apu;
static Dsp dsp;

int main(int argc, char **argv) {
  CHECK(argc == 2);
  char error[256];
  CHECK(Dkc2Msu1Open("", error, sizeof error) == NULL);
  Dkc2Msu1 *player = Dkc2Msu1Open(argv[1], error, sizeof error);
  CHECK(player);
  CHECK(Dkc2Msu1TrackNumber(6, 1) == 46);
  CHECK(Dkc2Msu1TrackNumber(32, 3) == 135);
  CHECK(Dkc2Msu1TrackNumber(33, 1) == 61);
  CHECK(Dkc2Msu1TrackNumber(34, 2) == 95);
  CHECK(Dkc2Msu1TrackNumber(35, 0) == 15);
  CHECK(Dkc2Msu1TrackNumber(37, 1) == 55);
  CHECK(Dkc2Msu1TrackNumber(0, 4) == 0);
  CHECK(Dkc2Msu1TrackNumber(6, 6) == 0);
  CHECK(Dkc2Msu1Select(player, 1, 0));
  int16_t output[16] = {0};
  Dkc2Msu1Mix(player, output, 4, 2, 44100);
  CHECK(output[0] == 1000 && output[1] == -1000);
  CHECK(output[2] == 2000 && output[4] == 3000 && output[6] == 2000);
  Dkc2Msu1SetGain(player, 200);
  output[0] = 32760; output[1] = -32760;
  Dkc2Msu1Mix(player, output, 1, 2, 44100);
  CHECK(output[0] == 32767 && output[1] == -32768);
  CHECK(Dkc2Msu1Select(player, 1, 0));
  Dkc2Msu1SetGain(player, 100);
  memset(output, 0, sizeof output);
  Dkc2Msu1Mix(player, output, 2, 2, 88200);
  CHECK(output[0] == 1000 && output[2] == 1500);
  CHECK(Dkc2Msu1Select(player, 17, 0));
  memset(output, 0, sizeof output);
  Dkc2Msu1Mix(player, output, 4, 2, 44100);
  CHECK(output[0] == 7000 && output[2] == 8000 && output[4] == 0);
  CHECK(!Dkc2Msu1Select(player, 2, 0)); /* invalid file */
  CHECK(!Dkc2Msu1Select(player, 3, 0)); /* missing file */
  Dkc2Msu1Close(player);

  apu.dsp = &dsp; dsp.apu_ram = apu.ram;
  dsp.echoDelay = 512;
  dsp.echoBufferAdr = 0xF800;
  for (unsigned i = 0; i < 0x32; ++i)
    rom[0x2E02D1 + i] = apu.ram[0x7A9 + i] = (uint8_t)(0x40 + i);
  rom[0x2E02D3] = apu.ram[0x7AB] = 0xD0;
  rom[0x2E02D4] = apu.ram[0x7AC] = 5;
  Dkc2MusicFrame(&apu, rom, sizeof rom, ram);
  CHECK(Dkc2MusicLoad(argv[1]));
  Dkc2MusicCommand(1, 255);
  Dkc2MusicCommand(1, 254);
  CHECK(apu.ram[0x7AB] == 0 && rom[0x2E02D3] == 0);
  memset(output, 0, sizeof output);
  Dkc2MusicMix(output, 1, 44100);
  CHECK(output[0] == 1000);
  Dkc2MusicCommand(1, 0x01FB); /* track 41 */
  CHECK(strstr(Dkc2MusicStatus(), "track 41"));
  Dkc2MusicCommand(1, 0x02FB); /* missing track 81 restores SPC */
  CHECK(apu.ram[0x7AB] == 0xD0 && rom[0x2E02D3] == 0xD0);
  memset(output, 0, sizeof output);
  Dkc2MusicMix(output, 1, 44100);
  CHECK(output[0] == 0);
  ram[0x1C] = 1;
  apu.ram[0xE5] = 0; apu.ram[0xE6] = 0x13;
  Dkc2MusicStateLoaded(&apu, ram);
  CHECK(strstr(Dkc2MusicStatus(), "track 1"));
  CHECK(apu.ram[0x7AB] == 0);
  Dkc2MusicSetEnabled(false);
  CHECK(apu.ram[0x7AB] == 0xD0);
  CHECK(Dkc2MusicSaveSettings());
  puts("MSU-1 PCM, looping, one-shot, resampling, gain, commands, fallback and state restoration passed.");
  return 0;
}
