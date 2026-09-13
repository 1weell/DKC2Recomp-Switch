#include "dkc2_kongs.h"
#include "snes/ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { kMaxFrames = 4096, kAnimations = 182, kMaxSteps = 2048, kMaxMounted = 8192 };
typedef struct KongFrame {
  uint16_t id, width, height;
  int16_t x, y;
  uint8_t *pixels;
  int16_t carry_x, carry_y;
  bool has_carry;
} KongFrame;
typedef struct KongStep { uint16_t frame, duration; } KongStep;
typedef struct KongSequence {
  uint16_t count, loop;
  KongStep *steps;
} KongSequence;
typedef struct KongMountedPose {
  uint16_t choice, animation, animal_graphic, frame, explicit_offset, duration;
  int16_t x, y;
} KongMountedPose;
typedef struct KongPack {
  uint16_t palettes[2][16];
  size_t count;
  KongFrame *frames;
  KongSequence sequences[2][kAnimations];
  int16_t attachment[2][5][2];
  size_t mounted_count;
  KongMountedPose *mounted;
  bool has_mounted, has_moves, has_handoff, has_team;
} KongPack;
typedef struct KongActor {
  const KongFrame *frame;
  int x, y, choice;
  uint16_t properties;
  uint8_t slots[128];
  int trigger;
} KongActor;

static KongPack s_pack;
static int s_choices[2];
static char s_path[4096];
static char s_status[256] = "Import a Project Kongs character pack to enable these choices.";
static KongActor s_actors[2];
static uint16_t s_saved_palette[2][16];
static bool s_palette_active;
typedef struct KongClock {
  uint32_t key, tick, elapsed;
  bool valid;
} KongClock;
static KongClock s_rider_clocks[2], s_pose_clocks[2], s_actor_clocks[2];
static KongClock s_handoff_clock;
static KongClock s_team_clock;
static uint32_t s_last_tick, s_visual_tick;
static bool s_tick_valid;
typedef struct KongMove {
  unsigned address, choice, kind, started, landed;
  bool airborne, prepared;
} KongMove;
static KongMove s_moves[2];

void Dkc2KongsReset(void) {
  memset(s_rider_clocks, 0, sizeof s_rider_clocks);
  memset(s_pose_clocks, 0, sizeof s_pose_clocks);
  memset(s_actor_clocks, 0, sizeof s_actor_clocks);
  memset(&s_handoff_clock, 0, sizeof s_handoff_clock);
  memset(&s_team_clock, 0, sizeof s_team_clock);
  memset(s_moves, 0, sizeof s_moves);
  s_tick_valid = false;
}


static uint16_t Read16(const uint8_t *p) {
  return (uint16_t)(p[0] | (unsigned)p[1] << 8);
}
static uint32_t Read32(const uint8_t *p) {
  return Read16(p) | (uint32_t)Read16(p + 2) << 16;
}
static void FreePack(KongPack *pack) {
  for (size_t i = 0; i < pack->count; ++i) free(pack->frames[i].pixels);
  free(pack->frames);
  free(pack->mounted);
  for (unsigned c = 0; c < 2; ++c)
    for (unsigned a = 0; a < kAnimations; ++a)
      free(pack->sequences[c][a].steps);
  memset(pack, 0, sizeof *pack);
}
void Dkc2KongsUnload(void) {
  FreePack(&s_pack);
  memset(s_actors, 0, sizeof s_actors);
  Dkc2KongsReset();
  s_path[0] = 0;
}
bool Dkc2KongsReady(void) { return s_pack.count != 0; }
const char *Dkc2KongsStatus(void) { return s_status; }
const char *Dkc2KongsPath(void) { return s_path; }
size_t Dkc2KongsFrameCount(void) { return s_pack.count; }
int Dkc2KongsChoice(int slot) {
  return (unsigned)slot < 2 ? s_choices[slot] : 0;
}
void Dkc2KongsSetChoice(int slot, int choice) {
  if ((unsigned)slot < 2)
    s_choices[slot] = (unsigned)choice < kDkc2KongCount ? choice : 0;
}

static int FindFrame(const KongPack *pack, uint16_t id) {
  for (size_t i = 0; i < pack->count; ++i)
    if (pack->frames[i].id == id) return (int)i;
  return -1;
}

bool Dkc2KongsLoadBytes(const uint8_t *data, size_t size) {
  KongPack pack = {0};
  size_t cursor = 80;
  if (!data || size < cursor || size > 16u * 1024u * 1024u ||
      (memcmp(data, "DKC2KNG1", 8) != 0 && memcmp(data, "DKC2KNG2", 8) != 0 &&
       memcmp(data, "DKC2KNG3", 8) != 0 && memcmp(data, "DKC2KNG4", 8) != 0 &&
       memcmp(data, "DKC2KNG5", 8) != 0)) goto invalid;
  pack.has_mounted = data[7] >= '2';
  pack.has_moves = data[7] >= '3';
  pack.has_handoff = data[7] >= '4';
  pack.has_team = data[7] >= '5';
  const uint32_t count = Read32(data + 8), sequences = Read32(data + 12);
  if (!count || count > kMaxFrames || sequences > 2 * kAnimations) goto invalid;
  for (unsigned c = 0; c < 2; ++c)
    for (unsigned i = 0; i < 16; ++i) {
      pack.palettes[c][i] = Read16(data + 16 + c * 32 + i * 2);
      if (pack.palettes[c][i] > 0x7fff) goto invalid;
    }
  pack.frames = (KongFrame *)calloc(count, sizeof *pack.frames);
  if (!pack.frames) goto invalid;
  pack.count = count;
  for (size_t i = 0; i < count; ++i) {
    if (size - cursor < 10) goto invalid;
    KongFrame *frame = &pack.frames[i];
    frame->id = Read16(data + cursor);
    frame->x = (int16_t)Read16(data + cursor + 2);
    frame->y = (int16_t)Read16(data + cursor + 4);
    frame->width = Read16(data + cursor + 6);
    frame->height = Read16(data + cursor + 8);
    cursor += 10;
    const size_t pixels = (size_t)frame->width * frame->height;
    if (!frame->width || frame->width > 128 || !frame->height ||
        frame->height > 128 || frame->x < -128 || frame->x > 127 ||
        frame->y < -128 || frame->y > 127 || size - cursor < pixels)
      goto invalid;
    for (size_t j = 0; j < i; ++j)
      if (pack.frames[j].id == frame->id) goto invalid;
    frame->pixels = (uint8_t *)malloc(pixels);
    if (!frame->pixels) goto invalid;
    for (size_t j = 0; j < pixels; ++j)
      if (data[cursor + j] > 15) goto invalid;
    memcpy(frame->pixels, data + cursor, pixels);
    cursor += pixels;
  }
  for (size_t i = 0; i < sequences; ++i) {
    if (size - cursor < 8) goto invalid;
    const unsigned character = Read16(data + cursor);
    const unsigned animation = Read16(data + cursor + 2);
    const unsigned steps = Read16(data + cursor + 4);
    const unsigned loop = Read16(data + cursor + 6);
    cursor += 8;
    if (character < 1 || character > 2 || animation >= kAnimations ||
        !steps || steps > kMaxSteps || loop >= steps ||
        size - cursor < steps * 4u) goto invalid;
    KongSequence *sequence = &pack.sequences[character - 1][animation];
    if (sequence->steps) goto invalid;
    sequence->steps = (KongStep *)calloc(steps, sizeof *sequence->steps);
    if (!sequence->steps) goto invalid;
    sequence->count = (uint16_t)steps;
    sequence->loop = (uint16_t)loop;
    for (unsigned j = 0; j < steps; ++j) {
      const int index = FindFrame(&pack, Read16(data + cursor));
      const unsigned duration = Read16(data + cursor + 2);
      cursor += 4;
      if (index < 0 || !duration || duration > 127) goto invalid;
      sequence->steps[j].frame = (uint16_t)index;
      sequence->steps[j].duration = (uint16_t)duration;
    }
  }
  if (pack.has_mounted) {
    if (size - cursor < 44) goto invalid;
    for (unsigned c = 0; c < 2; ++c)
      for (unsigned animal = 0; animal < 5; ++animal)
        for (unsigned axis = 0; axis < 2; ++axis) {
          const int16_t value = (int16_t)Read16(data + cursor);
          if (value < -128 || value > 127) goto invalid;
          pack.attachment[c][animal][axis] = value;
          cursor += 2;
        }
    pack.mounted_count = Read32(data + cursor);
    cursor += 4;
    if (pack.mounted_count > kMaxMounted || size - cursor < pack.mounted_count * 16)
      goto invalid;
    if (pack.mounted_count) {
      pack.mounted = (KongMountedPose *)calloc(pack.mounted_count, sizeof *pack.mounted);
      if (!pack.mounted) goto invalid;
    }
    for (size_t i = 0; i < pack.mounted_count; ++i) {
      KongMountedPose *pose = &pack.mounted[i];
      pose->choice = Read16(data + cursor);
      pose->animation = Read16(data + cursor + 2);
      pose->animal_graphic = Read16(data + cursor + 4);
      const int frame = FindFrame(&pack, Read16(data + cursor + 6));
      pose->x = (int16_t)Read16(data + cursor + 8);
      pose->y = (int16_t)Read16(data + cursor + 10);
      pose->explicit_offset = Read16(data + cursor + 12);
      pose->duration = Read16(data + cursor + 14);
      cursor += 16;
      if (pose->choice < 1 || pose->choice > 2 || pose->animation < 94 ||
          pose->animation > 158 || !pose->animal_graphic || pose->animal_graphic >= 0x35a0 ||
          (pose->animal_graphic & 3) || frame < 0 || pose->x < -128 || pose->x > 127 ||
          pose->y < -128 || pose->y > 127 || pose->explicit_offset > 1 ||
          !pose->duration || pose->duration > 127) goto invalid;
      pose->frame = (uint16_t)frame;
    }
    for (unsigned c = 0; c < 2; ++c)
      for (unsigned a = 164; a < 174; ++a)
        if (!pack.sequences[c][a].steps) goto invalid;
  }
  if (pack.has_moves) {
    if (size - cursor < 4) goto invalid;
    const unsigned carry_count = Read32(data + cursor);
    cursor += 4;
    if (carry_count > count || size - cursor < carry_count * 6u ||
        !pack.sequences[0][174].steps || !pack.sequences[1][174].steps ||
        !pack.sequences[1][175].steps) goto invalid;
    for (unsigned i = 0; i < carry_count; ++i) {
      const int index = FindFrame(&pack, Read16(data + cursor));
      const int16_t x = (int16_t)Read16(data + cursor + 2);
      const int16_t y = (int16_t)Read16(data + cursor + 4);
      cursor += 6;
      if (index < 0 || pack.frames[index].has_carry || x < -128 || x > 127 ||
          y < -128 || y > 127) goto invalid;
      pack.frames[index].has_carry = true;
      pack.frames[index].carry_x = x;
      pack.frames[index].carry_y = y;
    }
  }
  if (pack.has_handoff)
    for (unsigned c = 0; c < 2; ++c)
      for (unsigned a = 176; a < 178; ++a)
        if (!pack.sequences[c][a].steps) goto invalid;
  if (pack.has_team)
    for (unsigned c = 0; c < 2; ++c) {
      if (!pack.sequences[c][38].steps || !pack.sequences[c][39].steps) goto invalid;
      for (unsigned a = 178; a < 182; ++a)
        if (!pack.sequences[c][a].steps) goto invalid;
    }
  if (cursor != size || !pack.sequences[0][1].steps ||
      !pack.sequences[1][1].steps) goto invalid;
  Dkc2KongsUnload();
  s_pack = pack;
  (void)snprintf(s_status, sizeof s_status,
                 pack.has_team ? "%zu character frames loaded. Changes apply when you resume." :
                 pack.has_handoff ? "%zu character frames loaded. Re-import to enable paired team throws." :
                 pack.has_moves ? "%zu character frames loaded. Re-import to enable tag handoffs." :
                 pack.has_mounted ? "%zu character frames loaded. Re-import to enable ground attacks and corrected throws." :
                 "%zu character frames loaded. Re-import this legacy pack to enable animal riders.", pack.count);
  return true;
invalid:
  FreePack(&pack);
  (void)snprintf(s_status, sizeof s_status,
                 "Invalid character pack. The previous pack is still available.");
  return false;
}

bool Dkc2KongsLoad(const char *path) {
  if (!path || !*path || strlen(path) >= sizeof s_path) return false;
  char saved_path[sizeof s_path];
  (void)snprintf(saved_path, sizeof saved_path, "%s", path);
  FILE *f = fopen(path, "rb");
  if (!f) {
    (void)snprintf(s_status, sizeof s_status, "Character pack could not be opened.");
    return false;
  }
  bool ok = false;
  if (fseek(f, 0, SEEK_END) == 0) {
    const long size = ftell(f);
    if (size >= 80 && size <= 16 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0) {
      uint8_t *bytes = (uint8_t *)malloc((size_t)size);
      if (bytes) {
        if (fread(bytes, 1, (size_t)size, f) == (size_t)size && !ferror(f))
          ok = Dkc2KongsLoadBytes(bytes, (size_t)size);
        free(bytes);
      }
    }
  }
  fclose(f);
  if (ok) (void)snprintf(s_path, sizeof s_path, "%s", saved_path);
  else (void)snprintf(s_status, sizeof s_status, "Could not load character pack. Choose a valid .dkc2kongs file.");
  return ok;
}

bool Dkc2KongsSaveSettings(void) {
  FILE *f = fopen("kongs.cfg", "w");
  if (!f) return false;
  const bool ok = fprintf(f, "%d %d\n%s\n", s_choices[0], s_choices[1], s_path) > 0;
  return fclose(f) == 0 && ok;
}
void Dkc2KongsInitialize(void) {
  FILE *f = fopen("kongs.cfg", "r");
  char path[4096] = "mods/project-kongs.dkc2kongs";
  if (f) {
    char line[4096];
    int a = 0, b = 0;
    if (fgets(line, sizeof line, f) && sscanf(line, "%d %d", &a, &b) == 2) {
      Dkc2KongsSetChoice(0, a);
      Dkc2KongsSetChoice(1, b);
      if (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (*line) (void)snprintf(path, sizeof path, "%s", line);
      }
    }
    fclose(f);
  }
  const char *override = getenv("DKC2_KONGS_PACK");
  if (override && *override) (void)Dkc2KongsLoad(override);
  else {
    f = fopen(path, "rb");
    if (f) { fclose(f); (void)Dkc2KongsLoad(path); }
  }
  const char *choices = getenv("DKC2_KONGS");
  int a, b;
  if (choices && sscanf(choices, "%d,%d", &a, &b) == 2) {
    Dkc2KongsSetChoice(0, a);
    Dkc2KongsSetChoice(1, b);
  }
}

static const KongFrame *SelectFrame(int choice, unsigned animation, uint32_t tick) {
  if (animation >= kAnimations || choice < 1 || choice > 2) return NULL;
  const KongSequence *sequence = &s_pack.sequences[choice - 1][animation];
  if (!sequence->steps) return NULL;
  unsigned duration = 0, intro = 0;
  for (unsigned i = 0; i < sequence->count; ++i) {
    if (i < sequence->loop) intro += sequence->steps[i].duration;
    duration += sequence->steps[i].duration;
  }
  if (tick >= intro) tick = intro + (tick - intro) % (duration - intro);
  for (unsigned i = 0; i < sequence->count; ++i) {
    if (tick < sequence->steps[i].duration)
      return &s_pack.frames[sequence->steps[i].frame];
    tick -= sequence->steps[i].duration;
  }
  return NULL;
}

static uint32_t AnimationTime(KongClock *clock, uint32_t key, uint32_t tick) {
  if (!clock->valid || clock->key != key || (tick != clock->tick && tick - clock->tick != 1)) {
    clock->elapsed = 0;
  } else if (tick != clock->tick) {
    ++clock->elapsed;
  }
  clock->valid = true;
  clock->key = key;
  clock->tick = tick;
  return clock->elapsed;
}

static uint32_t PresentationTime(const uint8_t *ram, uint32_t tick) {
  if (!s_tick_valid || (tick != s_last_tick && tick - s_last_tick != 1)) {
    Dkc2KongsReset();
    s_visual_tick = 0;
  } else if (tick != s_last_tick && !(Read16(ram + 0x8c2) & 0x40)) {
    ++s_visual_tick;
  }
  s_tick_valid = true;
  s_last_tick = tick;
  return s_visual_tick;
}

static const KongFrame *ActorFrame(unsigned who, unsigned address,
                                   unsigned animation, uint32_t tick) {
  const KongMove *move = &s_moves[who];
  if (move->kind && move->address == address && move->choice == (unsigned)s_choices[who]) {
    unsigned phase = tick - move->started;
    unsigned semantic = move->kind == 3 ? 38 : move->kind == 2 ? 20 : 174;
    if (move->kind == 1 && move->landed) {
      semantic = 175;
      phase = tick - move->landed;
    }
    return SelectFrame(s_choices[who], semantic, phase);
  }
  const uint32_t key = address | animation << 16 | (unsigned)s_choices[who] << 25;
  return SelectFrame(s_choices[who], animation, AnimationTime(&s_actor_clocks[who], key, tick));
}

/* Simulation-side adaptation of the original actor and collision contracts.
 * This never runs from the PPU renderer. All addresses target US v1.0. */
static void Write16(uint8_t *ram, unsigned address, unsigned value) {
  ram[address] = (uint8_t)value;
  ram[address + 1] = (uint8_t)(value >> 8);
}
static bool ActorAddress(unsigned a) {
  return a >= 0xde2 && a < 0x16b2 && (a - 0xde2) % 0x5e == 0;
}
bool Dkc2KongsUseCallbacks(const uint8_t *ram) {
  if (!ram || !s_pack.has_moves || Read16(ram + 0x6c)) return false;
  const unsigned a = Read16(ram + 0x593);
  if (!ActorAddress(a)) return false;
  const unsigned type = Read16(ram + a);
  return (type == 0xe4 || type == 0xe8) && s_choices[type == 0xe8] != 0;
}
static void NativeAnimation(uint8_t *ram, const uint8_t *rom, unsigned a, unsigned animation) {
  if (Read16(ram + a) == 0xe8) animation += 0xa3;
  const uint8_t *entry = rom + 0x390000 + animation * 4;
  Write16(ram, a + 0x36, animation);
  Write16(ram, a + 0x38, 0);
  Write16(ram, a + 0x3a, 0x100);
  Write16(ram, a + 0x3c, Read16(entry));
  Write16(ram, a + 0x3e, 0);
  Write16(ram, a + 0x40, Read16(entry + 2));
}
static bool SlapTarget(unsigned type, unsigned state) {
  switch (type) {
    case 0x1e0: return state == 4 || state == 5; /* Kutlass, swords down */
    case 0x204: return state == 0; /* Cat O' Nine Tails, before spinning */
    case 0x6c: case 0x1ac: case 0x1d8: case 0x1dc: case 0x1e4:
    case 0x1ec: case 0x1f0: case 0x200: case 0x214: case 0x21c:
    case 0x220: case 0x25c: return true;
    default: return false;
  }
}
typedef struct KongBox { int x, y, w, h; } KongBox;
static KongBox NativeBox(const uint8_t *ram, unsigned a, const uint8_t *box) {
  KongBox b = {(int16_t)Read16(box), (int16_t)Read16(box + 2),
               Read16(box + 4), Read16(box + 6)};
  const unsigned flip = Read16(ram + a + 0x12);
  if (flip & 0x4000) b.x = -b.x - b.w;
  if (flip & 0x8000) b.y = -b.y - b.h;
  b.x += Read16(ram + a + 6);
  b.y += Read16(ram + a + 10);
  return b;
}
static void GroundImpact(uint8_t *ram, const uint8_t *rom, unsigned a, unsigned choice, bool feedback) {
  /* Project Kongs adds a fifteenth clipping entry; the base ROM ends at
   * fourteen, so its extension must be expressed here rather than read as ROM. */
  KongBox attack = {Read16(ram + a + 6), Read16(ram + a + 10) - 52, 53, 60};
  if (Read16(ram + a + 0x12) & 0x4000) attack.x -= attack.w;
  if (choice == kDkc2KongKiddy) {
    attack = (KongBox){Read16(ram + a + 6) - 28, Read16(ram + a + 10) - 16, 56, 20};
  }
  if (feedback) {
  Write16(ram, 0xaf8, 0x400);
  /* The original eight-entry sound queue: deduplicate, and leave a full ring
   * intact. Channel 7, Rambi footstep, is the source hand-slap impact sound. */
  const unsigned sound = 0x754, write = Read16(ram + 0x634) & 14;
  const unsigned next = (write + 2) & 14;
  if (Read16(ram + 0x622 + write) != sound && !Read16(ram + 0x622 + next)) {
    Write16(ram, 0x622 + next, sound);
    Write16(ram, 0x634, next);
  }
  }
  unsigned hits = 0;
  for (unsigned enemy = 0xe9e; enemy < 0x16b2; enemy += 0x5e) {
    if (enemy == a || !(Read16(ram + enemy + 0x30) & 0x20) ||
        !SlapTarget(Read16(ram + enemy), Read16(ram + enemy + 0x2e))) continue;
    const unsigned graphic = Read16(ram + enemy + 0x1a);
    if (!graphic || graphic >= 0x35a0 || (graphic & 3)) continue;
    const unsigned clipping = Read16(rom + 0x3cb600 + graphic / 2);
    if (clipping < 0x8000 || clipping > 0xfff8) continue;
    const KongBox b = NativeBox(ram, enemy, rom + 0x3c0000 + clipping);
    if (!b.w || !b.h || b.w > 256 || b.h > 256 ||
        attack.x > b.x + b.w || attack.x + attack.w < b.x ||
        attack.y > b.y + b.h || attack.y + attack.h < b.y) continue;
    /* The native enemy update consumes defeat bit 8 and its attacker field. */
    Write16(ram, enemy + 0x32, Read16(ram + enemy + 0x32) | 8);
    Write16(ram, enemy + 0x34, (Read16(ram + a + 0x12) & 0x4000) | (a - 0xd84));
    ++hits;
  }
  if (getenv("DKC2_KONGS_TRACE") && (feedback || hits))
    fprintf(stderr, "kong %s choice=%u actor=%04x hits=%u\n", feedback ? "impact" : "contact", choice, a, hits);
}
static unsigned SequenceDuration(unsigned choice, unsigned animation) {
  const KongSequence *seq = &s_pack.sequences[choice - 1][animation];
  unsigned duration = 0;
  for (unsigned i = 0; i < seq->count; ++i) duration += seq->steps[i].duration;
  return duration;
}
static unsigned TeamPhase(unsigned a, unsigned choice, unsigned animation, uint32_t tick) {
  for (unsigned who = 0; who < 2; ++who)
    if (s_moves[who].kind == 3 && s_moves[who].address == a)
      return tick - s_moves[who].started;
  return AnimationTime(&s_team_clock, a | animation << 16 | choice << 25, tick);
}
static const KongFrame *TeamTopFrame(unsigned choice, unsigned carrier_choice,
                                     unsigned animation, unsigned phase) {
  unsigned semantic = animation == 31 ? 179 : animation >= 33 ? 180 : 178;
  if (animation == 38) {
    const unsigned curl = carrier_choice == 1 ? 3 : 8;
    semantic = phase < curl ? 178 : 181;
    phase = phase < curl ? 0 : phase - curl;
  }
  return SelectFrame((int)choice, semantic, phase);
}
/* Local contact-point adaptation. DKC3's Kiddy windup has five successive
 * forward hand positions; Project Kongs drops those paired-frame operands.
 * DK's top role uses his seated art, whose origin differs from Kiddy's seat.
 * These anchors describe the art, never the native collision rectangle. */
static void TeamOffset(unsigned bottom_choice, unsigned top_choice, unsigned animation,
                       unsigned phase, const KongFrame *bottom, const KongFrame *top,
                       int *dx, int *dy) {
  const bool curled = animation == 38 && phase >= (bottom_choice == 1 ? 3u : 8u);
  if (!curled) {
    *dx = (bottom_choice == 1 ? 0 : 6) - (top_choice == 1 ? -5 : 8);
    *dy = bottom->y + (bottom_choice == 1 ? 15 : 10) - (top_choice == 1 ? -35 : -22);
    return;
  }
  int hand_x, hand_y;
  if (bottom_choice == 1) {
    static const int hands[5][2] = {{18,-23},{22,-31},{10,-40},{-5,-46},{-10,-52}};
    const unsigned step = phase / 3 < 5 ? phase / 3 : 4;
    hand_x = hands[step][0]; hand_y = hands[step][1];
  } else {
    static const int hands[5][2] = {{24,-8},{18,-3},{12,2},{20,0},{28,-2}};
    const unsigned step = (phase - 8) / 2 < 5 ? (phase - 8) / 2 : 4;
    hand_x = hands[step][0]; hand_y = hands[step][1];
  }
  /* Normalize each tumble frame at its lower silhouette so rotating origins
   * cannot slide the carried body out of the carrier's hands. */
  int left = 128, right = -128, foot = -128;
  for (unsigned y = 0; y < top->height; ++y)
    for (unsigned x = 0; x < top->width; ++x)
      if (top->pixels[y * top->width + x]) {
        const int px = top->x + (int)x, py = top->y + (int)y;
        if (px < left) left = px;
        if (px > right) right = px;
        if (py > foot) foot = py;
      }
  *dx = hand_x - (left + right) / 2;
  *dy = hand_y - foot;
}
static void CarryPosition(uint8_t *ram, unsigned a, unsigned object, const KongFrame *frame) {
  if (!frame || !frame->has_carry) return;
  Write16(ram, 0xd7c, (unsigned)frame->carry_x);
  Write16(ram, 0xd7e, (unsigned)frame->carry_y);
  int dx = frame->carry_x;
  if (Read16(ram + a + 0x12) & 0x4000) dx = -dx;
  Write16(ram, object + 6, (unsigned)(Read16(ram + a + 6) + dx));
  Write16(ram, object + 10, (unsigned)(Read16(ram + a + 10) + frame->carry_y));
}
static uint32_t WaitAnimationCallback(uint8_t *ram, unsigned a) {
  /* Command 81 has already advanced its cursor by three bytes. Rewind that
   * command and end this animation update; the native call stack stays intact. */
  Write16(ram, a + 0x3c, Read16(ram + a + 0x3c) - 3u);
  Write16(ram, a + 0x38, 0x100);
  return 0xb9d9b0; /* RTS, matching the animation callback's JSR frame. */
}
static void SeekThrowCallback(uint8_t *ram, const uint8_t *rom, unsigned a,
                              unsigned callback) {
  const unsigned animation = Read16(ram + a + 0x36);
  if (animation != 20 && animation != 20 + 0xa3 &&
      animation != 38 && animation != 38 + 0xa3) return;
  unsigned cursor = Read16(rom + 0x390000 + animation * 4);
  for (unsigned i = 0; i < 64 && cursor < 0xfff8; ++i) {
    const uint8_t *op = rom + 0x390000 + cursor;
    if (op[0] == 0x81 && Read16(op + 1) == callback) {
      Write16(ram, a + 0x3c, cursor);
      Write16(ram, a + 0x38, 0);
      return;
    }
    if (op[0] == 0x8a) cursor += 10;
    else if (op[0] == 0x8b) cursor += 8;
    else if ((op[0] > 0 && op[0] < 0x80) || op[0] == 0x81 ||
             op[0] == 0x83 || op[0] == 0x84) cursor += 3;
    else return; /* Only verified commands from the base throw streams. */
  }
}
uint32_t Dkc2KongsGameplay(uint8_t *ram, const uint8_t *rom, size_t rom_size,
                          uint32_t pc, uint32_t tick) {
  if (!ram || !rom || rom_size != 0x400000 || !s_pack.has_moves) return 0;
  tick = PresentationTime(ram, tick);
  if (Read16(ram + 0x8c2) & 0x40) return 0;
  pc |= 0x800000;
  const unsigned a = Read16(ram + 0x593);
  if (!ActorAddress(a) || Read16(ram + 0x6c) || Read16(ram + 0x96) == 11) {
    memset(s_moves, 0, sizeof s_moves);
    return 0;
  }
  const unsigned type = Read16(ram + a);
  if (type != 0xe4 && type != 0xe8) return 0;
  const unsigned who = type == 0xe8, choice = (unsigned)s_choices[who];
  KongMove *move = &s_moves[who];
  const unsigned carried = Read16(ram + 0xd7a);
  const unsigned state = Read16(ram + a + 0x2e);
  const unsigned animation = Read16(ram + a + 0x36) - (who ? 0xa3u : 0u);
  if (!choice || (move->kind && (move->address != a || move->choice != choice ||
      (move->kind == 1 && (carried || (state != 0 && state != 1 && state != 7 && !(choice == 2 && state == 0x16)))) ||
      (move->kind == 2 && animation != 20) ||
      (move->kind == 3 && (animation != 38 || (!move->airborne && carried != Read16(ram + 0x597))))))) {
    if (move->kind && getenv("DKC2_KONGS_TRACE"))
      fprintf(stderr, "kong cancel kind=%u choice=%u state=%u animation=%u carried=%04x\n",
              move->kind, choice, state, animation, carried);
    memset(move, 0, sizeof *move);
    if (!choice) return 0;
  }
  const unsigned current = Read16(ram + 0x64);
  if (pc == 0xb39fe7 && ActorAddress(carried) && current == carried) {
    CarryPosition(ram, a, carried, ActorFrame(who, a, animation, tick));
    return 0;
  }
  if (current != a) return 0;
  if (s_pack.has_team && animation == 38 && !move->kind &&
      ActorAddress(carried) && carried == Read16(ram + 0x597))
    *move = (KongMove){a, choice, 3, tick, 0, false, false};
  if (move->kind == 3) {
    const unsigned phase = tick - move->started, release = choice == 1 ? 15 : 18;
    if (pc == 0xb9dcea) {
      if (phase + 1 < release) return WaitAnimationCallback(ram, a);
      /* The base Dixie stream has more swing frames between these callbacks.
       * Prepare the top on the last windup frame and release on the next. */
      SeekThrowCallback(ram, rom, a, 0xd8be);
      move->prepared = true;
    } else if (pc == 0xb9d8be) {
      if (phase < release) return WaitAnimationCallback(ram, a);
      const unsigned top_type = ActorAddress(carried) ? Read16(ram + carried) : 0;
      const unsigned top_choice = (top_type == 0xe4 || top_type == 0xe8) ?
                                  (unsigned)s_choices[top_type == 0xe8] : 0;
      const KongFrame *bottom = SelectFrame((int)choice, 38, release - 1);
      const KongFrame *top = SelectFrame((int)top_choice, 39, 0);
      if (bottom && top) {
        int dx, dy;
        TeamOffset(choice, top_choice, 38, release - 1, bottom, top, &dx, &dy);
        Write16(ram, 0xd7c, (unsigned)dx); Write16(ram, 0xd7e, (unsigned)dy);
        if (Read16(ram + a + 0x12) & 0x4000) dx = -dx;
        Write16(ram, carried + 6, (unsigned)(Read16(ram + a + 6) + dx));
        Write16(ram, carried + 10, (unsigned)(Read16(ram + a + 10) + dy));
      }
      move->airborne = true;
      if (getenv("DKC2_KONGS_TRACE"))
        fprintf(stderr, "kong team release choice=%u phase=%u partner=%04x\n", choice, phase, carried);
    } else if (pc == 0xb9dfd5) {
      if (phase < SequenceDuration(choice, 38)) return WaitAnimationCallback(ram, a);
      memset(move, 0, sizeof *move);
    } else if (pc == 0xb89616) {
      if (phase >= SequenceDuration(choice, 38)) SeekThrowCallback(ram, rom, a, 0xdfd5);
      else if (!move->airborne && move->prepared && phase >= release)
        SeekThrowCallback(ram, rom, a, 0xd8be);
      else if (!move->airborne && !move->prepared && phase + 1 >= release)
        SeekThrowCallback(ram, rom, a, 0xdcea);
    }
    return 0;
  }
  /* Y still enables normal air steering/run speed, but only original Dixie
   * can enter her helicopter animation. The hook follows that shared update. */
  if (pc == 0xb8c924) return 0xb8c92d;
  if (pc == 0xb89616 && state == 6 && animation == 11) {
    /* A pack/menu change or old save can resume an already-running glide.
     * Restore normal gravity and terminal speed from this actor's ROM table. */
    const unsigned variables = Read16(ram + 0x66);
    const unsigned constants = (Read16(ram + 0x8e) | (unsigned)ram[0x90] << 16) & 0x3fffff;
    if ((variables == 0x16b2 || variables == 0x16d8) && constants >= 0x8000 &&
        constants <= rom_size - 4) {
      Write16(ram, variables + 8, Read16(rom + constants));
      Write16(ram, variables + 10, Read16(rom + constants + 2));
      if ((int16_t)Read16(ram + a + 0x24) < 0) Write16(ram, a + 0x24, 0);
      NativeAnimation(ram, rom, a, 7);
    }
  }
  if (pc == 0xb9d8ac && animation == 20 && ActorAddress(carried)) {
    *move = (KongMove){a, choice, 2, tick, 0, false, false};
  } else if (pc == 0xb9d967 && move->kind == 2) {
    const unsigned release = choice == 1 ? 22 : 18;
    if (tick - move->started < release) return WaitAnimationCallback(ram, a);
    move->airborne = true; /* Release consumed; do not seek it again. */
    /* At release use the final attached windup pose, before the free barrel
     * begins its native trajectory and collision routine. */
    if (ActorAddress(carried)) {
      CarryPosition(ram, a, carried, SelectFrame((int)choice, 20, release - 1));
      if (getenv("DKC2_KONGS_TRACE"))
        fprintf(stderr, "kong throw choice=%u phase=%u object=%04x offset=%d,%d\n", choice,
                tick - move->started, carried, (int16_t)Read16(ram + 0xd7c), (int16_t)Read16(ram + 0xd7e));
    }
  } else if (pc == 0xb9d9e0 && move->kind == 2 && ActorAddress(carried)) {
    /* Both donor characters use this forward trajectory; Dixie originally
     * used a slower, flatter release. Keep the native wall/terrain correction. */
    const unsigned vx = (Read16(ram + a + 0x12) & 0x4000) ? 0xf400 : 0xc00;
    Write16(ram, carried + 0x20, vx);
    Write16(ram, carried + 0x26, vx);
    Write16(ram, carried + 0x24, 0xff00);
  } else if (pc == 0xb9dfd5 && move->kind == 2) {
    if (tick - move->started < SequenceDuration(choice, 20))
      return WaitAnimationCallback(ram, a);
    memset(move, 0, sizeof *move);
  } else if (pc == 0xb89616) {
    if (move->kind == 2) {
      const unsigned phase = tick - move->started;
      if (phase >= SequenceDuration(choice, 20))
        SeekThrowCallback(ram, rom, a, 0xdfd5);
      else if (!move->airborne && phase >= (choice == 1 ? 22u : 18u))
        SeekThrowCallback(ram, rom, a, 0xd967);
      return 0;
    }
    if (!move->kind && (state == 0 || state == 7 || state == 2) && !carried &&
        !Read16(ram + a + 0xe) && (Read16(ram + 0x981) & 0x400) &&
        (Read16(ram + 0x983) & 0x4000)) {
      *move = (KongMove){a, choice, 1, tick, 0, false, false};
      Write16(ram, a + 0x2e, choice == 2 ? 1 : 0);
      NativeAnimation(ram, rom, a, choice == 2 ? 6 : 13);
      if (choice == 2) {
        Write16(ram, a + 0x24, 0xfa00);
        Write16(ram, a + 0xe, 0xffff);
      }
      if (getenv("DKC2_KONGS_TRACE"))
        fprintf(stderr, "kong pound start choice=%u actor=%04x\n", choice, a);
    }
    if (move->kind != 1) return 0;
    const unsigned phase = tick - move->started;
    if ((choice == 1 && phase >= SequenceDuration(choice, 174)) ||
        (move->landed && tick - move->landed >= 18) || phase > 180) {
      memset(move, 0, sizeof *move);
      NativeAnimation(ram, rom, a, Read16(ram + a + 0xe) ? 6 : 1);
      return 0;
    }
    if (choice == 2 && state == 0x16) {
      /* The native downward contact has defeated an enemy and requested its
       * ordinary bounce. A body slam continues downward to the floor. */
      Write16(ram, a + 0x2e, 1);
      Write16(ram, a + 0x24, 0x600);
      if (getenv("DKC2_KONGS_TRACE")) fprintf(stderr, "kong slam stomp actor=%04x\n", a);
    }
    Write16(ram, a + 0x20, 0);
    Write16(ram, a + 0x26, 0);
    if (choice == 1) {
      if (Read16(ram + a + 0xe)) { memset(move, 0, sizeof *move); return 0; }
      if (phase == 18 || phase == 26 || phase == 32) GroundImpact(ram, rom, a, choice, true);
    } else if (!move->landed) {
      if (Read16(ram + a + 0xe)) move->airborne = true;
      if (phase >= 8 && move->airborne && !Read16(ram + a + 0xe) &&
          (int16_t)Read16(ram + a + 0x24) >= 0) {
        move->landed = tick;
        GroundImpact(ram, rom, a, choice, true);
      } else if (phase >= 10) {
        if (Read16(ram + a + 0xe) <= 20)
          GroundImpact(ram, rom, a, choice, false);
        Write16(ram, a + 0x24, 0x600);
      }
    }
    /* Skip input dispatch, retaining native gravity, terrain, enemy contacts,
     * animation, and the normal scheduler continuation. Never add guest states. */
    return 0xb8995f;
  }
  return 0;
}

static const KongFrame *MountedFrame(unsigned who, unsigned address, unsigned type,
                                     const uint8_t *ram, uint32_t tick, int *dx, int *dy) {
  if (!s_pack.has_mounted) return NULL;
  const unsigned animal = (type - 0x190) / 4;
  const unsigned leader = Read16(ram + 0x593);
  if (leader < 0xde2 || leader >= 0x16b2 || (leader - 0xde2) % 0x5e) return NULL;
  const unsigned choice = (unsigned)s_choices[who];
  unsigned animation = Read16(ram + leader + 0x36);
  if (who) {
    if (animation < 0xa3) return NULL;
    animation -= 0xa3;
  }
  if (animation < 94 || animation > 158) return NULL;
  // $26 is the movement predicate used by the source rider callback. Enguarde
  // explicitly stays in the seated loop; Rattly and Squawks have separate poses.
  const bool moving = (animal == 0 || animal == 3) && Read16(ram + leader + 0x26) != 0;
  const uint32_t key = address | choice << 16 | animal << 20 | (uint32_t)moving << 24;
  const uint32_t elapsed = AnimationTime(&s_rider_clocks[who], key, tick);
  const KongFrame *frame = SelectFrame((int)choice, (moving ? 169 : 164) + animal, elapsed);
  *dx = s_pack.attachment[choice - 1][animal][0] - (int16_t)Read16(ram + 0xd72);
  *dy = s_pack.attachment[choice - 1][animal][1] - (int16_t)Read16(ram + 0xd74);
  const unsigned graphic = Read16(ram + leader + 0x18);
  uint32_t phase = AnimationTime(&s_pose_clocks[who], graphic | animation << 16 | choice << 25, tick);
  const KongMountedPose *selected = NULL;
  for (size_t i = 0; i < s_pack.mounted_count; ++i) {
    const KongMountedPose *pose = &s_pack.mounted[i];
    if (pose->choice != choice || pose->animation != animation || pose->animal_graphic != graphic)
      continue;
    selected = pose;
    if (phase < pose->duration) break;
    phase -= pose->duration;
  }
  if (selected) {
    frame = &s_pack.frames[selected->frame];
    if (selected->explicit_offset) {
      *dx = s_pack.attachment[choice - 1][animal][0] + selected->x - (int16_t)Read16(ram + 0xd76);
      *dy = s_pack.attachment[choice - 1][animal][1] + selected->y - (int16_t)Read16(ram + 0xd78);
    }
  }
  return frame;
}

static bool MatchLayout(const Ppu *ppu, KongActor *actor,
                        const uint8_t *layout, unsigned slots) {
  const unsigned pieces = layout[0] + layout[1] + layout[3];
  if (pieces < slots || !pieces || pieces > 64) return false;
  const bool flip = (actor->properties & 0x4000) != 0;
  const bool flip_y = (actor->properties & 0x8000) != 0;
  unsigned matched = 0;
  int anchor_x = 0, anchor_y = 0;
  bool origin = false;
  uint8_t used[128] = {0};
  for (unsigned i = 0; i < pieces; ++i) {
    const int size = i < layout[0] ? 16 : 8;
    const int ox = (int)layout[8 + i * 2] - 128;
    const int dx = flip ? -ox - size : ox;
    const int oy = (int)layout[9 + i * 2] - 128;
    const int dy = flip_y ? -oy - size : oy;
    const unsigned tile = i < layout[0] ? i / 8 * 32 + i % 8 * 2 :
        (i < (unsigned)layout[0] + layout[1] ? layout[2] + i - layout[0] :
         layout[4] + i - layout[0] - layout[1]);
    const uint16_t expected = (uint16_t)(actor->properties + tile);
    bool found = false;
    for (unsigned slot = 0; slot < 128; ++slot) {
      if (!actor->slots[slot] || used[slot] || ppu->oam[slot * 2 + 1] != expected) continue;
      const unsigned high = (ppu->highOam[slot / 4] >> (slot % 4 * 2)) & 3;
      const int px = (ppu->oam[slot * 2] & 255) | ((high & 1) << 8);
      const int py = ppu->oam[slot * 2] >> 8;
      if (((high & 2) != 0) != (size == 16)) continue;
      if (!origin) {
        anchor_x = px - dx;
        anchor_x = actor->x + ((anchor_x - actor->x + 256) & 511) - 256;
        anchor_y = actor->y + ((py - dy - actor->y + 128) & 255) - 128;
        if (abs(anchor_x - actor->x) > 128 || abs(anchor_y - actor->y) > 128) continue;
        origin = true;
      }
      if (px != ((anchor_x + dx) & 511) || py != ((anchor_y + dy) & 255)) continue;
      used[slot] = 1; found = true; break;
    }
    if (found) ++matched;
  }
  if (matched != slots) return false;
  actor->x = anchor_x;
  actor->y = anchor_y;
  return true;
}

void Dkc2KongsPrepare(Ppu *ppu, const uint8_t *ram, const uint8_t *rom,
                      size_t rom_size, uint32_t tick) {
  memset(s_actors, 0, sizeof s_actors);
  if (!Dkc2KongsReady() || !ppu || !ram || !rom || rom_size != 0x400000 ||
      Read16(ram + 0x24) != 0x8819 || Read16(ram + 0x96) == 11 ||
      PPU_mode(ppu) != 1) {
    memset(s_rider_clocks, 0, sizeof s_rider_clocks);
    memset(s_pose_clocks, 0, sizeof s_pose_clocks);
    return;
  }
  tick = PresentationTime(ram, tick);
  /* The native incoming Kong owns animation 73, while paired commands draw
   * the outgoing Kong without updating its animation ID. Drive both roles
   * from one clock, including when that partner still reports idle. */
  unsigned handoff = 0, handoff_phase = 0, handoff_release = 0;
  const unsigned leader = Read16(ram + 0x593), partner = Read16(ram + 0x597);
  if (s_pack.has_handoff && !Read16(ram + 0x6c) && ActorAddress(leader) && ActorAddress(partner)) {
    const unsigned type = Read16(ram + leader);
    if ((type == 0xe4 || type == 0xe8) &&
        Read16(ram + leader + 0x36) == 73 + (type == 0xe8 ? 0xa3 : 0)) {
      handoff = leader;
      handoff_release = type == 0xe8 ? 26 : 44;
      const unsigned key = leader | (unsigned)s_choices[0] << 16 | (unsigned)s_choices[1] << 20;
      handoff_phase = AnimationTime(&s_handoff_clock, key, tick);
    }
  }
  if (!handoff) s_handoff_clock.valid = false;
  unsigned team_animation = 0, team_phase = 0, bottom_slot = 0, top_slot = 0;
  if (s_pack.has_team && !Read16(ram + 0x6c) && ActorAddress(leader) &&
      ActorAddress(partner) && leader != partner && Read16(ram + 0xd7a) == partner) {
    const unsigned bottom_type = Read16(ram + leader), top_type = Read16(ram + partner);
    bottom_slot = bottom_type == 0xe8; top_slot = top_type == 0xe8;
    const unsigned animation = Read16(ram + leader + 0x36) - bottom_slot * 0xa3;
    if ((bottom_type == 0xe4 || bottom_type == 0xe8) &&
        (top_type == 0xe4 || top_type == 0xe8) && bottom_slot != top_slot &&
        s_choices[bottom_slot] && s_choices[top_slot] &&
        animation >= 29 && animation <= 38 && animation != 32) {
      team_animation = animation;
      team_phase = TeamPhase(leader, (unsigned)s_choices[bottom_slot], animation, tick);
    }
  }
  if (!team_animation) s_team_clock.valid = false;
  bool rider_seen[2] = {false, false};
  for (unsigned a = 0x0de2; a < 0x16b2; a += 0x5e) {
    const uint16_t type = Read16(ram + a);
    unsigned animation = Read16(ram + a + 0x36);
    // While riding, the original player slot runs the animal animation and
    // $6C names the separate Kong rider. Keep the animal and replace its rider.
    const bool rider = a == Read16(ram + 0x6c) &&
        type >= 0x190 && type <= 0x1a0 && (type & 3) == 0;
    if (type != 0xe4 && type != 0xe8 && !rider) continue;
    const unsigned who = rider ? animation >= 0xa4 : type == 0xe8;
    KongActor *actor = &s_actors[who];
    if (!s_choices[who] || actor->frame) continue;
    if (who) {
      if (animation < 0xa4) continue;
      animation -= 0xa3;
    }
    int rider_dx = 0, rider_dy = 0;
    const KongFrame *frame = rider ? MountedFrame(who, a, type, ram, tick, &rider_dx, &rider_dy) :
                                    ActorFrame(who, a, animation, tick);
    if (team_animation && a == partner)
      frame = TeamTopFrame((unsigned)s_choices[who], (unsigned)s_choices[bottom_slot],
                           team_animation, team_phase);
    if (handoff && (a == handoff || a == partner)) {
      if (handoff_phase < handoff_release) {
        const unsigned semantic = a == handoff ? 176 : 177;
        const unsigned duration = SequenceDuration((unsigned)s_choices[who], semantic);
        frame = SelectFrame(s_choices[who], semantic, handoff_phase * duration / handoff_release);
      } else if (a == handoff) {
        frame = SelectFrame(s_choices[who], 1, handoff_phase - handoff_release);
      }
    }
    if (rider) rider_seen[who] = true;
    const unsigned graphic = Read16(ram + a + 0x18);
    if (!frame || !graphic || graphic >= 0x35a0 || (graphic & 3)) continue;
    const uint8_t *entry = rom + 0x3c8000 + graphic;
    const size_t address = (Read16(entry) | (uint32_t)entry[2] << 16) & 0x3fffff;
    if (address + 8 > rom_size) continue;
    const uint8_t *layout = rom + address;
    const unsigned pieces = layout[0] + layout[1] + layout[3];
    if (!pieces || pieces > 64 || address + 8 + pieces * 2 > rom_size) continue;
    actor->x = (int16_t)(Read16(ram + a + 6) - Read16(ram + 0x17ba)) - 1;
    actor->y = (int16_t)(Read16(ram + a + 10) - Read16(ram + 0x17c0));
    actor->properties = Read16(ram + a + 0x12);
    actor->choice = s_choices[who];
    actor->trigger = 128;
    const unsigned base_tile = actor->properties & 511;
    unsigned slots = 0;
    for (unsigned slot = 0; slot < 128; ++slot) {
      const unsigned attr = ppu->oam[slot * 2 + 1];
      const unsigned tile = attr & 511;
      if ((attr & 0x3e00) != (actor->properties & 0x3e00) ||
          tile < base_tile || tile >= base_tile + 32 ||
          (ppu->oam[slot * 2] >> 8) == 240) continue;
      actor->slots[slot] = 1;
      if ((int)slot < actor->trigger) actor->trigger = (int)slot;
      ++slots;
    }
    bool matched = false;
    if (slots) {
      // OAM is committed before the next actor update. Recover the origin
      // and layout from the actual displayed compound sprite, rather than
      // assuming that current WRAM positions and animation are already on screen.
      actor->properties = (uint16_t)((ppu->oam[actor->trigger * 2 + 1] & 0xfe00) | base_tile);
      matched = MatchLayout(ppu, actor, layout, slots);
      if (!matched) {
        for (unsigned candidate = 4; candidate < 0x35a0; candidate += 4) {
          const uint8_t *pointer = rom + 0x3c8000 + candidate;
          const size_t offset = (Read16(pointer) | (uint32_t)pointer[2] << 16) & 0x3fffff;
          if (offset + 8 > rom_size) continue;
          const uint8_t *shape = rom + offset;
          const unsigned count = shape[0] + shape[1] + shape[3];
          if (count < slots || count > 64 || offset + 8 + count * 2 > rom_size) continue;
          if (MatchLayout(ppu, actor, shape, slots)) { matched = true; break; }
        }
      }
    }
    if (matched) {
      actor->frame = frame;
      if (rider) {
        actor->x += (actor->properties & 0x4000) ? -rider_dx : rider_dx;
        actor->y += (actor->properties & 0x8000) ? -rider_dy : rider_dy;
      }
    }
    else memset(actor, 0, sizeof *actor);
    if (getenv("DKC2_KONGS_TRACE"))
      fprintf(stderr, "kongs tick=%u slot=%u animation=%u graphic=%04x matched=%u/%u active=%d rider=%d pose=%04x offset=%d,%d origin=%d,%d\n",
              tick, who, animation, graphic, matched ? slots : 0, slots, actor->frame != NULL,
              rider, frame->id, rider_dx, rider_dy, actor->x, actor->y);
  }
  if (team_animation && s_actors[bottom_slot].frame && s_actors[top_slot].frame) {
    KongActor *bottom = &s_actors[bottom_slot], *top = &s_actors[top_slot];
    int dx, dy;
    TeamOffset((unsigned)bottom->choice, (unsigned)top->choice, team_animation,
                team_phase, bottom->frame, top->frame, &dx, &dy);
    top->x = bottom->x + ((bottom->properties & 0x4000) ? -dx : dx);
    top->y = bottom->y + dy;
    top->properties = (uint16_t)((top->properties & ~0x4000) | (bottom->properties & 0x4000));
  }
  for (unsigned who = 0; who < 2; ++who)
    if (!rider_seen[who]) s_rider_clocks[who].valid = s_pose_clocks[who].valid = false;
}

unsigned Dkc2KongsActiveActors(void) {
  return (s_actors[0].frame != NULL) + (s_actors[1].frame != NULL);
}
void Dkc2KongsBeginLine(Ppu *ppu) {
  s_palette_active = false;
  if (!ppu) return;
  for (unsigned a = 0; a < 2; ++a) {
    const KongActor *actor = &s_actors[a];
    if (!actor->frame) continue;
    const unsigned base = 128 + ((actor->properties >> 9) & 7) * 16;
    memcpy(s_saved_palette[a], ppu->cgram + base, sizeof s_saved_palette[a]);
    memcpy(ppu->cgram + base, s_pack.palettes[actor->choice - 1], sizeof s_saved_palette[a]);
    s_palette_active = true;
  }
}
void Dkc2KongsEndLine(Ppu *ppu) {
  if (!ppu || !s_palette_active) return;
  for (int a = 1; a >= 0; --a) {
    const KongActor *actor = &s_actors[a];
    if (!actor->frame) continue;
    const unsigned base = 128 + ((actor->properties >> 9) & 7) * 16;
    memcpy(ppu->cgram + base, s_saved_palette[a], sizeof s_saved_palette[a]);
  }
  s_palette_active = false;
}

bool Dkc2KongsRenderOam(Ppu *ppu, int line, unsigned slot) {
  if (!ppu || slot >= 128) return false;
  for (unsigned a = 0; a < 2; ++a) {
    const KongActor *actor = &s_actors[a];
    const KongFrame *frame = actor->frame;
    if (!frame || !actor->slots[slot]) continue;
    if ((int)slot != actor->trigger) return true;
    const int local_y = line - actor->y;
    const int row = ((actor->properties & 0x8000) ? -1 - local_y : local_y) - frame->y;
    if (row < 0 || row >= frame->height) return true;
    const bool flip = (actor->properties & 0x4000) != 0;
    const unsigned palette = 128 + ((actor->properties >> 9) & 7) * 16;
    const unsigned priority = SPRITE_PRIO_TO_PRIO(
        (actor->properties >> 12) & 3, (actor->properties & 0x800) == 0);
    for (unsigned x = 0; x < frame->width; ++x) {
      const unsigned color = frame->pixels[(size_t)row * frame->width + x];
      if (!color) continue;
      const int local = frame->x + (int)x;
      const int px = actor->x + (flip ? -1 - local : local) - ppu->wsPresentationXBias;
      if (px < -(int)ppu->extraLeftCur || px >= 256 + (int)ppu->extraRightCur) continue;
      ppu->objBuffer.data[px + kPpuExtraLeftRight] =
          (PpuZbufType)(palette + color + (priority << 8));
    }
    return true;
  }
  return false;
}

int Dkc2KongsOamVisible(int line, unsigned slot) {
  if (slot >= 128) return -1;
  for (unsigned a = 0; a < 2; ++a) {
    const KongActor *actor = &s_actors[a];
    if (!actor->frame || !actor->slots[slot]) continue;
    if ((int)slot != actor->trigger) return 0;
    const int local_y = line - actor->y;
    const int row = ((actor->properties & 0x8000) ? -1 - local_y : local_y) - actor->frame->y;
    return row >= 0 && row < actor->frame->height ? 1 : 0;
  }
  return -1;
}
