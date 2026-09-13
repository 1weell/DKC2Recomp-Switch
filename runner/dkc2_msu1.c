#include "dkc2_msu1.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define open(path, flags) _open(path, (flags) | _O_BINARY)
#define close _close
#define fstat _fstat64
#define stat _stat64
#define PROT_READ 0
#define MAP_PRIVATE 0
#define MAP_FAILED ((void *)-1)
static void *mmap(void *address,size_t size,int protection,int flags,int fd,int offset) {
  (void)address; (void)protection; (void)flags; (void)offset;
  HANDLE mapping=CreateFileMappingW((HANDLE)_get_osfhandle(fd),NULL,PAGE_READONLY,0,0,NULL);
  if (!mapping) return MAP_FAILED;
  void *view=MapViewOfFile(mapping,FILE_MAP_READ,0,0,size);
  CloseHandle(mapping); return view ? view : MAP_FAILED;
}
static int munmap(void *address,size_t size) {
  (void)size; return UnmapViewOfFile(address) ? 0 : -1;
}
#else
#include <sys/mman.h>
#include <unistd.h>
#endif



#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

enum {
  kMsuInputRate = 44100,
  kMsuPcmHeaderSize = 8,
  kMsuMaximumTrack = 239,
  kMsuTrackCount = kMsuMaximumTrack,

};

typedef struct Dkc2MsuTrack {
  const uint8_t *mapping;
  size_t mapping_size;
  uint32_t total_frames;
  uint32_t loop_frame;
  int descriptor;
  bool present;
} Dkc2MsuTrack;

struct Dkc2Msu1 {
  char directory[PATH_MAX];
  Dkc2MsuTrack tracks[kMsuTrackCount];
  const Dkc2MsuTrack *track;
  uint32_t total_frames;
  uint32_t loop_frame;
  uint32_t source_frame;
  uint32_t phase;
  uint16_t song;
  unsigned track_number;
  int16_t current_sample[2];
  int16_t next_sample[2];
  double gain;
  bool loop;
  bool playing;
  bool final_sample;
  bool song_valid;
};

static void SetError(char *error, size_t error_size, const char *message) {
  if (error && error_size)
    (void)snprintf(error, error_size, "%s",
                   message ? message : "unknown error");
}

static uint32_t ReadLittle32(const uint8_t bytes[4]) {
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int16_t ReadLittle16(const uint8_t bytes[2]) {
  return (int16_t)(uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void CloseTrack(Dkc2Msu1 *player) {
  player->track = NULL;
  player->playing = false;
  player->final_sample = false;
  player->track_number = 0;
  player->total_frames = 0;
  player->loop_frame = 0;
  player->source_frame = 0;
  player->phase = 0;
  memset(player->current_sample, 0, sizeof player->current_sample);
  memset(player->next_sample, 0, sizeof player->next_sample);
}

static bool SeekFrame(Dkc2Msu1 *player, uint32_t frame) {
  if (!player->track || frame >= player->total_frames)
    return false;
  player->source_frame = frame;
  return true;
}

static bool ReadFrame(Dkc2Msu1 *player, int16_t sample[2]) {
  if (!player->track)
    return false;
  if (player->source_frame >= player->total_frames) {
    if (!player->loop || !SeekFrame(player, player->loop_frame))
      return false;
  }
  const size_t offset =
      kMsuPcmHeaderSize + (size_t)player->source_frame * 4u;
  if (offset > player->track->mapping_size ||
      player->track->mapping_size - offset < 4u)
    return false;
  const uint8_t *bytes = player->track->mapping + offset;
  sample[0] = ReadLittle16(bytes);
  sample[1] = ReadLittle16(bytes + 2);
  player->source_frame++;
  return true;
}

static void UnmapTrack(Dkc2MsuTrack *track) {
  if (!track)
    return;
  if (track->mapping && track->mapping_size)
    (void)munmap((void *)track->mapping, track->mapping_size);
  if (track->descriptor >= 0)
    (void)close(track->descriptor);
  *track = (Dkc2MsuTrack){.descriptor = -1};
}

/* Map every available PCM during host startup. Playback then performs
 * pointer reads without opening files in the frame loop. OS paging can still occur. */
static int MapTrackFile(const char *path, Dkc2MsuTrack *track) {
  if (!path || !track)
    return -1;
  const int descriptor = open(path, O_RDONLY);
  if (descriptor < 0)
    return errno == ENOENT ? 0 : -1;
  struct stat info;
  if (fstat(descriptor, &info) != 0 ||
      info.st_size < kMsuPcmHeaderSize + 4 ||
      (uintmax_t)info.st_size > (uintmax_t)SIZE_MAX) {
    (void)close(descriptor);
    return -1;
  }
  const size_t mapping_size = (size_t)info.st_size;
  const uint8_t *mapping = mmap(
      NULL, mapping_size, PROT_READ, MAP_PRIVATE, descriptor, 0);
  if (mapping == MAP_FAILED) {
    (void)close(descriptor);
    return -1;
  }
  if ((mapping_size - kMsuPcmHeaderSize) / 4u > UINT32_MAX ||
      memcmp(mapping, "MSU1", 4) != 0 ||
      ((mapping_size - kMsuPcmHeaderSize) & 3u) != 0) {
    (void)munmap((void *)mapping, mapping_size);
    (void)close(descriptor);
    return -1;
  }
#ifdef F_RDAHEAD
  (void)fcntl(descriptor, F_RDAHEAD, 1);
#endif
#ifdef MADV_SEQUENTIAL
  (void)madvise((void *)mapping, mapping_size, MADV_SEQUENTIAL);
#endif
  track->mapping = mapping;
  track->mapping_size = mapping_size;
  track->total_frames =
      (uint32_t)((mapping_size - kMsuPcmHeaderSize) / 4u);
  track->loop_frame = ReadLittle32(mapping + 4);
  track->descriptor = descriptor;
  track->present = true;
  return 1;
}

static int CacheTrack(Dkc2Msu1 *player, unsigned track_number) {
  if (!player || track_number == 0 || track_number > kMsuMaximumTrack)
    return -1;
  Dkc2MsuTrack *track = &player->tracks[track_number - 1u];
  const char *patterns[] = {
      "%s/track-%u.pcm",
      "%s/dkc2_msu-%u.pcm",
      "%s/dkc2_msu1-%u.pcm",
  };
  char path[PATH_MAX];
  for (size_t i = 0; i < sizeof patterns / sizeof patterns[0]; i++) {
    if (snprintf(path, sizeof path, patterns[i], player->directory,
                 track_number) >= (int)sizeof path)
      continue;
    const int mapped = MapTrackFile(path, track);
    if (mapped != 0)
      return mapped;
  }
  return 0;
}

static bool TrackLoops(unsigned track) {
  /* Stock fanfares and secondary death/victory/target tracks play once.
   * Flying Krock, Token Tango, Screech and Rambi Chase loop. */
  return (track < 40 && track != 17 && track != 19 && track != 20 && track != 36)
      || track == 59 || track == 135 || track == 209 || track == 211;
}

static bool OpenTrack(Dkc2Msu1 *player, unsigned track_number) {
  CloseTrack(player);
  if (track_number == 0 || track_number > kMsuMaximumTrack)
    return false;
  const Dkc2MsuTrack *cached = &player->tracks[track_number - 1u];
  if (!cached->present)
    return false;
  player->track = cached;
  player->total_frames = cached->total_frames;
  player->loop_frame = cached->loop_frame;
  player->loop = TrackLoops(track_number) &&
                 player->loop_frame < player->total_frames;
  player->track_number = track_number;
  if (!SeekFrame(player, 0) ||
      !ReadFrame(player, player->current_sample)) {
    CloseTrack(player);
    return false;
  }
  if (!ReadFrame(player, player->next_sample)) {
    memcpy(player->next_sample, player->current_sample, sizeof player->next_sample);
    player->final_sample = true;
  }
  player->playing = true;
  return true;
}

Dkc2Msu1 *Dkc2Msu1Open(const char *directory, char *error,
                        size_t error_size) {
  if (!directory || !*directory) {
    SetError(error, error_size, "MSU-1 directory is empty");
    return NULL;
  }
  Dkc2Msu1 *player = (Dkc2Msu1 *)calloc(1, sizeof *player);
  if (!player) {
    SetError(error, error_size, "out of memory opening MSU-1 pack");
    return NULL;
  }
  if (snprintf(player->directory, sizeof player->directory, "%s", directory) >=
      (int)sizeof player->directory) {
    SetError(error, error_size, "MSU-1 directory path is too long");
    free(player);
    return NULL;
  }
  for (unsigned track = 0; track < kMsuTrackCount; track++)
    player->tracks[track].descriptor = -1;
  for (unsigned track = 1; track <= kMsuMaximumTrack; track++)
    (void)CacheTrack(player, track);
  if (!player->tracks[0].present) {
    SetError(error, error_size,
             "MSU-1 pack has no valid track 1 (track-1.pcm or "
             "dkc2_msu-1.pcm)");
    Dkc2Msu1Close(player);
    return NULL;
  }
  player->gain = 1.0;
  const char *gain = getenv("DKC2_MSU1_GAIN");
  if (gain && *gain) {
    char *end = NULL;
    const double parsed = strtod(gain, &end);
    if (end && !*end && parsed >= 0.0 && parsed <= 4.0)
      player->gain = parsed;
  }
  SetError(error, error_size, "");
  return player;
}

void Dkc2Msu1Close(Dkc2Msu1 *player) {
  if (!player)
    return;
  CloseTrack(player);
  for (unsigned track = 0; track < kMsuTrackCount; track++)
    UnmapTrack(&player->tracks[track]);
  free(player);
}

unsigned Dkc2Msu1TrackNumber(unsigned song, unsigned variant) {
  /* The five alternate SFX banks share a primary soundtrack. */
  static const unsigned aliases[] = {15, 21, 15, 15, 36, 15};
  if (song >= 32 && song <= 37) song = aliases[song - 32];
  return song > 0 && song < 40 && variant <= 5 ? song + 40 * variant : 0;
}

bool Dkc2Msu1Select(Dkc2Msu1 *player, unsigned song, unsigned variant) {
  return player && OpenTrack(player, Dkc2Msu1TrackNumber(song, variant));
}
void Dkc2Msu1SetGain(Dkc2Msu1 *player, int percent) {
  if (player) player->gain = (percent < 0 ? 0 : percent > 200 ? 200 : percent) / 100.0;
}

void Dkc2Msu1Reset(Dkc2Msu1 *player) {
  if (!player)
    return;
  CloseTrack(player);
  player->song = 0;
  player->song_valid = false;
}

static int16_t Saturate16(int value) {
  if (value > INT16_MAX)
    return INT16_MAX;
  if (value < INT16_MIN)
    return INT16_MIN;
  return (int16_t)value;
}

void Dkc2Msu1Mix(Dkc2Msu1 *player, int16_t *samples, int frames,
                 int channels, int output_rate) {
  if (!player || !player->playing || !samples || frames <= 0 ||
      channels != 2 || output_rate <= 0)
    return;

  for (int frame = 0; frame < frames && player->playing; frame++) {
    for (int channel = 0; channel < 2; channel++) {
      const int64_t interpolated =
          ((int64_t)player->current_sample[channel] *
               (output_rate - (int)player->phase) +
           (int64_t)player->next_sample[channel] * player->phase) /
          output_rate;
      const int external = (int)(interpolated * player->gain);
      const int index = frame * channels + channel;
      samples[index] = Saturate16((int)samples[index] + external);
    }

    player->phase += kMsuInputRate;
    while (player->phase >= (uint32_t)output_rate) {
      player->phase -= (uint32_t)output_rate;
      player->current_sample[0] = player->next_sample[0];
      player->current_sample[1] = player->next_sample[1];
      if (!ReadFrame(player, player->next_sample)) {
        if (player->final_sample) {
          player->playing = false;
          break;
        }
        player->final_sample = true;
        memcpy(player->next_sample, player->current_sample, sizeof player->next_sample);
      }
    }
  }
}

unsigned Dkc2Msu1CurrentTrack(const Dkc2Msu1 *player) {
  return player ? player->track_number : 0;
}

const char *Dkc2Msu1Directory(const Dkc2Msu1 *player) {
  return player ? player->directory : "";
}
