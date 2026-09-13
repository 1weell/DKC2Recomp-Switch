#include "dkc2_kongs.h"
#include "snes/ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void W16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static size_t Pack(uint8_t *p) {
  memset(p, 0, 256);
  memcpy(p, "DKC2KNG1", 8);
  W16(p + 8, 1); W16(p + 12, 2);
  W16(p + 18, 31); W16(p + 50, 31 << 10);
  W16(p + 80, 0x1234); W16(p + 82, 0); W16(p + 84, (unsigned)-16);
  W16(p + 86, 1); W16(p + 88, 1); p[90] = 1;
  size_t n = 91;
  for (unsigned c = 1; c <= 2; ++c) {
    W16(p + n, c); W16(p + n + 2, 1); W16(p + n + 4, 1);
    W16(p + n + 6, 0); W16(p + n + 8, 0x1234); W16(p + n + 10, 1);
    n += 12;
  }
  return n;
}
static size_t MountedPack(uint8_t *p) {
  size_t n = Pack(p);
  memcpy(p, "DKC2KNG2", 8);
  // A second pose has a distinct pixel and origin, so phase is observable.
  memmove(p + 102, p + 91, n - 91);
  W16(p + 91, 0x1238); W16(p + 93, 2); W16(p + 95, (unsigned)-20);
  W16(p + 97, 1); W16(p + 99, 1); p[101] = 2;
  n += 11; W16(p + 8, 2); W16(p + 12, 22);
  for (unsigned c = 1; c <= 2; ++c)
    for (unsigned a = 164; a < 174; ++a) {
      W16(p + n, c); W16(p + n + 2, a); W16(p + n + 4, 2);
      W16(p + n + 6, 0); W16(p + n + 8, 0x1234); W16(p + n + 10, 2);
      W16(p + n + 12, 0x1238); W16(p + n + 14, 2); n += 16;
    }
  for (unsigned c = 0; c < 2; ++c)
    for (unsigned animal = 0; animal < 5; ++animal) {
      W16(p + n, (unsigned)((c ? -7 : 5) + (int)animal));
      W16(p + n + 2, (unsigned)(c ? 9 : -3)); n += 4;
    }
  W16(p + n, 1); W16(p + n + 2, 0); n += 4;
  W16(p + n, 1); W16(p + n + 2, 112); W16(p + n + 4, 0x1000);
  W16(p + n + 6, 0x1238); W16(p + n + 8, 2); W16(p + n + 10, (unsigned)-6);
  W16(p + n + 12, 1); W16(p + n + 14, 3);
  return n + 16;
}
static size_t MovesPack(uint8_t *p) {
  size_t n = MountedPack(p), at = n - 60;
  memmove(p + at + 68, p + at, 60);
  memcpy(p, "DKC2KNG3", 8); W16(p + 12, 27);
  for (unsigned i = 0; i < 3; ++i) {
    W16(p + at, i == 0 ? 1 : 2); W16(p + at + 2, i == 2 ? 175 : 174);
    W16(p + at + 4, 1); W16(p + at + 6, 0);
    W16(p + at + 8, 0x1234); W16(p + at + 10, i == 0 ? 60 : 18);
    at += 12;
  }
  for (unsigned c = 1; c <= 2; ++c) {
    W16(p + at, c); W16(p + at + 2, 20); W16(p + at + 4, 2);
    W16(p + at + 6, 1); W16(p + at + 8, 0x1234); W16(p + at + 10, 22);
    W16(p + at + 12, 0x1238); W16(p + at + 14, 22); at += 16;
  }
  n += 68; W16(p + n, 1); W16(p + n + 2, 0); n += 4;
  W16(p + n, 0x1234); W16(p + n + 2, (unsigned)-7);
  W16(p + n + 4, (unsigned)-54);
  return n + 6;
}
static unsigned R16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static size_t HandoffPack(uint8_t *p) {
  size_t n = MovesPack(p), at = n - 70;
  memmove(p + at + 64, p + at, 70);
  memcpy(p, "DKC2KNG4", 8); W16(p + 12, 31);
  for (unsigned c = 1; c <= 2; ++c)
    for (unsigned a = 176; a < 178; ++a) {
      W16(p + at, c); W16(p + at + 2, a); W16(p + at + 4, 2);
      W16(p + at + 6, 1); W16(p + at + 8, 0x1234); W16(p + at + 10, 22);
      W16(p + at + 12, 0x1238); W16(p + at + 14, 22); at += 16;
    }
  return n + 64;
}
static size_t TeamPack(uint8_t *p) {
  size_t n = HandoffPack(p), at = n - 70;
  const unsigned semantics[] = {29,31,32,33,38,39,40,178,179,180,181};
  const size_t extra = 2 * 11 * 16;
  memmove(p + at + extra, p + at, 70);
  memcpy(p, "DKC2KNG5", 8); W16(p + 12, 53);
  for (unsigned c = 1; c <= 2; ++c)
    for (unsigned i = 0; i < 11; ++i) {
      unsigned first = semantics[i] == 179 ? 0x1238 : 0x1234;
      W16(p + at, c); W16(p + at + 2, semantics[i]); W16(p + at + 4, 2);
      W16(p + at + 6, 1); W16(p + at + 8, first); W16(p + at + 10, 20);
      W16(p + at + 12, 0x1238); W16(p + at + 14, 30); at += 16;
    }
  return n + extra;
}
static void TeamTests(Ppu *ppu, uint8_t *ram, uint8_t *rom) {
  uint8_t pack[2048] = {0}; const size_t n = TeamPack(pack);
  CHECK(Dkc2KongsLoadBytes(pack, n));
  for (size_t i = 0; i < n; ++i) CHECK(!Dkc2KongsLoadBytes(pack, i));
  CHECK(Dkc2KongsLoadBytes(pack, n));
  memset(ram, 0, 0x20000); memset(rom, 0, 0x400000); memset(ppu, 0, sizeof *ppu);
  ppu->bgmode = 1; W16(ram + 0x24, 0x8819);
  for (unsigned s = 0; s < 128; ++s) ppu->oam[s * 2] = 0xf000;
  const uint8_t layout[] = {0,1,0,0,0,1,0,0,128,120};
  memcpy(rom + 0x1000, layout, sizeof layout);
  W16(rom + 0x3c8004, 0x1000); rom[0x3c8006] = 0xc0;
  W16(ram + 0x593, 0xde2); W16(ram + 0x597, 0xe40); W16(ram + 0xd7a, 0xe40);
  for (unsigned who = 0; who < 2; ++who) {
    unsigned a = 0xde2 + who * 0x5e, x = 100 + who * 40, attr = 0x32a0 + who * 0x420;
    Dkc2KongsSetChoice((int)who, (int)who + 1);
    W16(ram + a, 0xe4 + who * 4); W16(ram + a + 6, x + 1); W16(ram + a + 10, 100);
    W16(ram + a + 0x12, attr); W16(ram + a + 0x18, 4);
    W16(ram + a + 0x36, (who ? 32 : 29) + who * 0xa3);
    ppu->oam[who * 2] = (92 << 8) | x; ppu->oam[who * 2 + 1] = (uint16_t)attr;
  }
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 0);
  CHECK(Dkc2KongsActiveActors() == 2);
  // The top comes from the carrier's displayed origin, not the old top OAM.
  CHECK(Dkc2KongsOamVisible(105, 1) == 1);
  memset(&ppu->objBuffer, 0, sizeof ppu->objBuffer);
  CHECK(Dkc2KongsRenderOam(ppu, 105, 1));
  CHECK((ppu->objBuffer.data[92 + kPpuExtraLeftRight] & 255) == 177);
  W16(ram + 0xde2 + 0x36, 31); // Follower still reports native idle 32.
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 1);
  CHECK(Dkc2KongsOamVisible(101, 1) == 1);
  W16(ram + 0x8c2, 0x40);
  for (unsigned t = 2; t < 82; ++t) Dkc2KongsPrepare(ppu, ram, rom, 0x400000, t);
  CHECK(Dkc2KongsOamVisible(101, 1) == 1);
  for (unsigned who = 0; who < 2; ++who) {
    ppu->oam[who * 2] -= 8; ppu->oam[who * 2 + 1] |= 0x4000;
    W16(ram + 0xde2 + who * 0x5e + 0x12, ppu->oam[who * 2 + 1]);
  }
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 82);
  memset(&ppu->objBuffer, 0, sizeof ppu->objBuffer);
  CHECK(Dkc2KongsRenderOam(ppu, 101, 1));
  CHECK((ppu->objBuffer.data[105 + kPpuExtraLeftRight] & 255) == 178);
  W16(ram + 0xd7a, 0); // Dropping immediately abandons the paired origin/pose.
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 83);
  CHECK(Dkc2KongsOamVisible(84, 1) == 1);
  // A missed throw's recovery settles once, holds through a long wait/pause,
  // then releases its final pose as soon as native following resumes.
  W16(ram + 0xe40 + 0x36, 40 + 0xa3); W16(ram + 0x8c2, 0);
  for (unsigned t = 84; t <= 285; ++t) {
    Dkc2KongsPrepare(ppu, ram, rom, 0x400000, t);
    CHECK(Dkc2KongsOamVisible(t < 104 ? 84 : 80, 1) == 1);
  }
  W16(ram + 0x8c2, 0x40);
  for (unsigned t = 286; t < 346; ++t) {
    Dkc2KongsPrepare(ppu, ram, rom, 0x400000, t);
    CHECK(Dkc2KongsOamVisible(80, 1) == 1);
  }
  W16(ram + 0x8c2, 0); W16(ram + 0xe40 + 0x36, 1 + 0xa3);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 346);
  CHECK(Dkc2KongsOamVisible(84, 1) == 1);
  for (unsigned who = 0; who < 2; ++who)
    for (unsigned choice = 1; choice <= 2; ++choice)
      for (unsigned flip = 0; flip < 2; ++flip) {
        Dkc2KongsReset(); memset(ram, 0, 0x20000); memset(rom, 0, 0x400000);
        const unsigned a = 0xde2 + who * 0x5e, partner = 0xde2 + (1 - who) * 0x5e;
        const unsigned release = choice == 1 ? 15 : 18;
        Dkc2KongsSetChoice((int)who, (int)choice);
        Dkc2KongsSetChoice((int)(1 - who), (int)(3 - choice));
        W16(ram + 0x593, a); W16(ram + 0x597, partner); W16(ram + 0xd7a, partner);
        W16(ram + 0x64, a); W16(ram + a, 0xe4 + who * 4);
        W16(ram + partner, 0xe4 + (1 - who) * 4);
        W16(ram + a + 0x36, 38 + who * 0xa3); W16(ram + a + 0x2e, 0x1d);
        W16(ram + a + 6, 100); W16(ram + a + 10, 100); W16(ram + a + 0x12, flip * 0x4000);
        W16(rom + 0x390000 + (38 + who * 0xa3) * 4, 0x5000);
        const uint8_t stream[] = {0x8a,2,0,0,0,0,0,0,0,0,0x81,0xea,0xdc,
                                  0x8b,2,0,0,0,0,0,0,0x81,0xbe,0xd8,
                                  0x83,0,0,0x81,0xd5,0xdf,0x80,0};
        memcpy(rom + 0x395000, stream, sizeof stream);
        for (unsigned t = 0; t < release; ++t)
          CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, t));
        CHECK(R16(ram + a + 0x3c) == 0x500a);
        CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9dcea, release - 1));
        CHECK(R16(ram + a + 0x3c) == 0x5015);
        CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d8be, release));
        CHECK(R16(ram + partner + 6) == (choice == 1 ? (flip ? 110 : 90) : (flip ? 72 : 128)));
        CHECK(R16(ram + partner + 10) == (choice == 1 ? 64 : 114));
        W16(ram + 0xd7a, 0);
        for (unsigned t = release + 1; t <= 50; ++t)
          Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, t);
        CHECK(R16(ram + a + 0x3c) == 0x501b);
        CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9dfd5, 50));
        // Restoring or being hurt must never retain the old attachment.
        Dkc2KongsReset(); W16(ram + a + 0x36, 41 + who * 0xa3);
        CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d8be, 51));
      }
}
static void HandoffTests(Ppu *ppu, uint8_t *ram, uint8_t *rom) {
  uint8_t pack[1024] = {0}; const size_t n = HandoffPack(pack);
  CHECK(Dkc2KongsLoadBytes(pack, n));
  for (size_t i = 0; i < n; ++i) CHECK(!Dkc2KongsLoadBytes(pack, i));
  CHECK(Dkc2KongsLoadBytes(pack, n));
  // Both tag roles use the shared phase even when the outgoing actor says idle.
  for (unsigned lead = 0; lead < 2; ++lead) {
    Dkc2KongsReset();
    memset(ram, 0, 0x20000); memset(rom, 0, 0x400000); memset(ppu, 0, sizeof *ppu);
    ppu->bgmode = 1; W16(ram + 0x24, 0x8819);
    for (unsigned s = 0; s < 128; ++s) ppu->oam[s * 2] = 0xf000;
    const uint8_t layout[] = {0,1,0,0,0,1,0,0,128,120};
    memcpy(rom + 0x1000, layout, sizeof layout);
    W16(rom + 0x3c8004, 0x1000); rom[0x3c8006] = 0xc0;
    W16(ram + 0x593, 0xde2 + lead * 0x5e);
    W16(ram + 0x597, 0xde2 + (1 - lead) * 0x5e);
    for (unsigned who = 0; who < 2; ++who) {
      unsigned a = 0xde2 + who * 0x5e, x = 100 + who * 28, attr = 0x32a0 + who * 0x420;
      Dkc2KongsSetChoice((int)who, (int)who + 1);
      W16(ram + a, 0xe4 + who * 4); W16(ram + a + 6, x + 1); W16(ram + a + 10, 100);
      W16(ram + a + 0x12, attr); W16(ram + a + 0x18, 4);
      W16(ram + a + 0x36, (who == lead ? 73 : 1) + who * 0xa3);
      ppu->oam[who * 2] = (92 << 8) | x; ppu->oam[who * 2 + 1] = (uint16_t)attr;
    }
    const unsigned release = lead ? 26 : 44;
    for (unsigned t = 0; t <= release / 2; ++t) Dkc2KongsPrepare(ppu, ram, rom, 0x400000, t);
    CHECK(Dkc2KongsActiveActors() == 2);
    CHECK(Dkc2KongsOamVisible(80, 0) == 1); CHECK(Dkc2KongsOamVisible(80, 1) == 1);
    W16(ram + 0x8c2, 0x40);
    for (unsigned t = release / 2 + 1; t < release / 2 + 81; ++t)
      Dkc2KongsPrepare(ppu, ram, rom, 0x400000, t);
    CHECK(Dkc2KongsOamVisible(80, 0) == 1); CHECK(Dkc2KongsOamVisible(80, 1) == 1);
    W16(ram + 0x8c2, 0);
    for (unsigned t = release / 2 + 81; t <= release + 80; ++t)
      Dkc2KongsPrepare(ppu, ram, rom, 0x400000, t);
    CHECK(Dkc2KongsOamVisible(84, lead) == 1); // Incoming Kong resumes idle.
  }
}
static void GlideTests(uint8_t *ram, uint8_t *rom) {
  uint8_t pack[1024] = {0}; size_t n = MovesPack(pack);
  CHECK(Dkc2KongsLoadBytes(pack, n));
  for (unsigned who = 0; who < 2; ++who)
    for (unsigned choice = 0; choice < 3; ++choice) {
      Dkc2KongsReset(); memset(ram, 0, 0x20000); memset(rom, 0, 0x400000);
      const unsigned a = 0xde2 + who * 0x5e, vars = 0x16b2 + who * 0x26;
      Dkc2KongsSetChoice((int)who, (int)choice);
      W16(ram + a, 0xe4 + who * 4); W16(ram + 0x593, a); W16(ram + 0x64, a);
      W16(ram + a + 0x2e, 6); W16(ram + a + 0x36, 11 + who * 0xa3);
      W16(ram + 0x66, vars); W16(ram + 0x8e, 0x8000); ram[0x90] = 0xcf;
      W16(rom + 0xf8000, 0x70); W16(rom + 0xf8002, 0x800);
      W16(ram + vars + 8, 0x20); W16(ram + vars + 10, 0x100); W16(ram + a + 0x24, 0xff00);
      CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb8c924, 0) == (choice ? 0xb8c92d : 0));
      Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 1);
      CHECK(R16(ram + a + 0x36) == (choice ? 7 : 11) + who * 0xa3);
      CHECK(R16(ram + vars + 8) == (choice ? 0x70 : 0x20));
      CHECK(R16(ram + vars + 10) == (choice ? 0x800 : 0x100));
      CHECK(R16(ram + a + 0x24) == (choice ? 0 : 0xff00));
      W16(ram + 0x6c, 0xefc);
      CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb8c924, 2));
    }
}
static void MovesTests(uint8_t *ram, uint8_t *rom) {
  uint8_t pack[1024] = {0}; const size_t n = MovesPack(pack);
  CHECK(Dkc2KongsLoadBytes(pack, n));
  for (size_t i = 0; i < n; ++i) CHECK(!Dkc2KongsLoadBytes(pack, i));
  CHECK(Dkc2KongsLoadBytes(pack, n));
  memset(ram, 0, 0x20000); memset(rom, 0, 0x400000);
  const unsigned a = 0xde2, enemy = 0xe9e;
  W16(ram + 0x593, a); W16(ram + 0x64, a); W16(ram + a, 0xe4);
  W16(ram + a + 6, 100); W16(ram + a + 10, 100);
  W16(ram + 0x981, 0x400); W16(ram + 0x983, 0x4000);
  W16(ram + enemy, 0x1e4); W16(ram + enemy + 0x30, 0x20);
  W16(ram + enemy + 0x1a, 4); W16(ram + enemy + 6, 110); W16(ram + enemy + 10, 100);
  W16(rom + 0x3cb602, 0xfa80);
  W16(rom + 0x3cfa80 + 2, (unsigned)-10); W16(rom + 0x3cfa80 + 4, 12); W16(rom + 0x3cfa80 + 6, 10);
  W16(rom + 0x3cfa78, 0); W16(rom + 0x3cfa7a, (unsigned)-12);
  W16(rom + 0x3cfa7c, 30); W16(rom + 0x3cfa7e, 16);
  Dkc2KongsSetChoice(0, 0);
  CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 0));
  CHECK(!R16(ram + a + 0x36));
  Dkc2KongsSetChoice(0, 1);
  CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 1) == 0xb8995f);
  W16(ram + 0x983, 0);
  for (unsigned t = 2; t < 19; ++t)
    CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, t) == 0xb8995f);
  CHECK(!R16(ram + enemy + 0x32));
  W16(ram + 0x8c2, 0x40);
  for (unsigned t = 19; t < 199; ++t)
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, t));
  CHECK(!R16(ram + enemy + 0x32));
  W16(ram + 0x8c2, 0);
  CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 199) == 0xb8995f);
  CHECK(R16(ram + enemy + 0x32) == 8);
  CHECK(R16(ram + enemy + 0x34) == a - 0xd84);
  CHECK(R16(ram + 0xaf8) == 0x400);
  // Hurt state cancels the move without overriding the native response.
  W16(ram + a + 0x2e, 0x15);
  CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 200));
  CHECK(R16(ram + a + 0x2e) == 0x15);
  // Kiddy launches; landing, rather than elapsed presentation time, hits.
  Dkc2KongsReset(); Dkc2KongsSetChoice(0, 2);
  W16(ram + a + 0x2e, 0); W16(ram + 0x983, 0x4000); W16(ram + enemy + 0x32, 0);
  CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 0) == 0xb8995f);
  CHECK(R16(ram + a + 0x24) == 0xfa00);
  W16(ram + 0x983, 0);
  for (unsigned t = 1; t <= 10; ++t) Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, t);
  CHECK(!R16(ram + enemy + 0x32)); CHECK(R16(ram + a + 0x24) == 0x600);
  W16(ram + a + 0x2e, 0x16); // Native successful stomp requests a bounce.
  CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 10) == 0xb8995f);
  CHECK(R16(ram + a + 0x2e) == 1 && R16(ram + a + 0x24) == 0x600);
  W16(ram + a + 0xe, 0); W16(ram + a + 0x24, 0);
  CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 11) == 0xb8995f);
  CHECK(R16(ram + enemy + 0x32) == 8);
  // Mounting and restoring abandon host actions without inventing guest states.
  W16(ram + 0x6c, 0xe40);
  CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, 12));
  Dkc2KongsReset();
  W16(ram + 0x6c, 0); W16(ram + a + 0x2e, 0xf);
  W16(ram + a + 0x36, 20); W16(ram + 0xd7a, enemy);
  W16(ram + 0x983, 0);
  for (unsigned choice = 1; choice <= 2; ++choice) {
    Dkc2KongsReset(); Dkc2KongsSetChoice(0, (int)choice);
    const unsigned release = choice == 1 ? 22 : 18;
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d8ac, 0));
    for (unsigned t = 1; t < release; ++t) {
      W16(ram + a + 0x3c, 0x5003);
      CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d967, t) == 0xb9d9b0);
      CHECK(R16(ram + a + 0x3c) == 0x5000);
      CHECK(R16(ram + a + 0x38) == 0x100);
    }
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d967, release));
    CHECK(R16(ram + enemy + 6) == 93 && R16(ram + enemy + 10) == 46);
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d9e0, release));
    CHECK(R16(ram + enemy + 0x20) == 0xc00 && R16(ram + enemy + 0x24) == 0xff00);
    W16(ram + a + 0x12, 0x4000);
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d967, release));
    CHECK(R16(ram + enemy + 6) == 107);
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d9e0, release));
    CHECK(R16(ram + enemy + 0x20) == 0xf400);
    W16(ram + a + 0x12, 0);
    for (unsigned t = release + 1; t < 44; ++t) {
      W16(ram + a + 0x3c, 0x5003);
      CHECK(Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9dfd5, t) == 0xb9d9b0);
    }
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9dfd5, 44));
  }
  Dkc2KongsReset(); Dkc2KongsSetChoice(0, 2);
  W16(ram + a + 0x36, 20); W16(ram + a + 0x2e, 0xf);
  W16(rom + 0x390000 + 20 * 4, 0x1000);
  rom[0x391000] = 0x81; W16(rom + 0x391001, 0xd8ac);
  rom[0x391003] = 0x8b; rom[0x391004] = 30;
  rom[0x39100b] = 0x81; W16(rom + 0x39100c, 0xd967);
  rom[0x39100e] = 2; W16(rom + 0x39100f, 0x1234);
  rom[0x391011] = 0x81; W16(rom + 0x391012, 0xdfd5);
  CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d8ac, 0));
  for (unsigned t = 1; t <= 18; ++t)
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, t));
  CHECK(R16(ram + a + 0x3c) == 0x100b); // Cursor points at the opcode itself.
  CHECK(R16(ram + a + 0x38) == 0);
  CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb9d967, 18));
  for (unsigned t = 19; t <= 44; ++t)
    CHECK(!Dkc2KongsGameplay(ram, rom, 0x400000, 0xb89616, t));
  CHECK(R16(ram + a + 0x3c) == 0x1011);

}
int main(void) {
  uint8_t bytes[256]; const size_t size = Pack(bytes);
  CHECK(!Dkc2KongsReady());
  CHECK(Dkc2KongsLoadBytes(bytes, size));
  CHECK(Dkc2KongsFrameCount() == 1);
  for (size_t i = 0; i < size; ++i) {
    CHECK(!Dkc2KongsLoadBytes(bytes, i));
    CHECK(Dkc2KongsReady());
  }
  CHECK(!Dkc2KongsLoadBytes(bytes, size + 1));
  bytes[90] = 16;
  CHECK(!Dkc2KongsLoadBytes(bytes, size));
  bytes[90] = 1;
  CHECK(Dkc2KongsLoadBytes(bytes, size));
  Dkc2KongsSetChoice(0, 999); CHECK(Dkc2KongsChoice(0) == 0);
  Dkc2KongsSetChoice(-1, 2); CHECK(Dkc2KongsChoice(-1) == 0);
  Dkc2KongsSetChoice(0, 1);

  Ppu *ppu = (Ppu *)calloc(1, sizeof *ppu);
  uint8_t *ram = (uint8_t *)calloc(1, 0x20000);
  uint8_t *rom = (uint8_t *)calloc(1, 0x400000);
  CHECK(ppu && ram && rom);
  ppu->bgmode = 1;
  W16(ram + 0x24, 0x8819);
  W16(ram + 0xde2, 0xe4); W16(ram + 0xde8, 101); W16(ram + 0xdec, 100);
  W16(ram + 0xdf4, 0x32a0); W16(ram + 0xdfa, 4); W16(ram + 0xe18, 1);
  W16(rom + 0x3c8004, 0x1000); rom[0x3c8006] = 0xc0;
  const uint8_t layout[] = {0,1,0,0,0,1,0,0,128,120};
  memcpy(rom + 0x1000, layout, sizeof layout);
  for (unsigned s = 0; s < 128; ++s) ppu->oam[s*2] = 0xf000;
  ppu->oam[0] = (92 << 8) | 100; ppu->oam[1] = 0x32a0;
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 0);
  CHECK(Dkc2KongsActiveActors() == 1);
  CHECK(Dkc2KongsOamVisible(84, 0) == 1);
  CHECK(Dkc2KongsOamVisible(92, 0) == 0);
  CHECK(Dkc2KongsOamVisible(84, 1) == -1);
  CHECK(Dkc2KongsRenderOam(ppu, 84, 0));
  CHECK((ppu->objBuffer.data[100 + kPpuExtraLeftRight] & 255) == 145);
  ppu->cgram[145] = 0x1234;
  Dkc2KongsBeginLine(ppu); CHECK(ppu->cgram[145] == 31);
  Dkc2KongsEndLine(ppu); CHECK(ppu->cgram[145] == 0x1234);
  CHECK(ppu->oam[0] == ((92 << 8) | 100));
  // Guest position can already have advanced while OAM still shows this frame.
  W16(ram + 0xdec, 120);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 1);
  CHECK(Dkc2KongsActiveActors() == 1);
  CHECK(Dkc2KongsOamVisible(84, 0) == 1);
  W16(ram + 0xdec, 100);
  // A clipped compound sprite may submit only its first piece.
  rom[0x1001] = 2; rom[0x100a] = 136; rom[0x100b] = 120;
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 2);
  CHECK(Dkc2KongsActiveActors() == 1);
  rom[0x1001] = 1;
  // Animal riders live in a separate sprite identified by the mount pointer.
  W16(ram + 0x6c, 0xde2); W16(ram + 0xde2, 0x190); W16(ram + 0xe18, 0xa4);
  Dkc2KongsSetChoice(1, 2);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 2);
  // Legacy packs stay loadable, but cannot replace riders without attachments.
  CHECK(Dkc2KongsActiveActors() == 0);
  Dkc2KongsBeginLine(ppu); CHECK(ppu->cgram[145] == 0x1234);
  Dkc2KongsEndLine(ppu); CHECK(ppu->cgram[145] == 0x1234);
  W16(ram + 0x6c, 0); W16(ram + 0xde2, 0xe4); W16(ram + 0xe18, 1);
  ppu->oam[1] ^= 1; // A different tile cannot be erased as this actor.
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 0);
  CHECK(Dkc2KongsActiveActors() == 0);
  CHECK(!Dkc2KongsRenderOam(ppu, 84, 0));
  ppu->oam[1] ^= 1;
  Dkc2KongsSetChoice(0, 0);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 0);
  CHECK(Dkc2KongsActiveActors() == 0);

  uint8_t mounted[1024] = {0};
  const size_t mounted_size = MountedPack(mounted);
  CHECK(Dkc2KongsLoadBytes(mounted, mounted_size));
  for (size_t n = 0; n < mounted_size; ++n) {
    CHECK(!Dkc2KongsLoadBytes(mounted, n));
    CHECK(Dkc2KongsFrameCount() == 2);
  }
  mounted[mounted_size - 4] = 2; // Invalid explicit-offset flag.
  CHECK(!Dkc2KongsLoadBytes(mounted, mounted_size));
  mounted[mounted_size - 4] = 1;
  CHECK(Dkc2KongsLoadBytes(mounted, mounted_size));
  Dkc2KongsSetChoice(0, 1);
  W16(ram + 0x6c, 0xde2); W16(ram + 0xde2, 0x190); W16(ram + 0xe18, 79);
  W16(ram + 0x593, 0xe40); W16(ram + 0xe40, 0xe4); W16(ram + 0xe76, 94);
  W16(ram + 0xd72, (unsigned)-5); W16(ram + 0xd74, (unsigned)-1);
  W16(ram + 0xd76, (unsigned)-4); W16(ram + 0xd78, 2);
  // Per-animal attachments retain the original bob, for either original slot.
  for (unsigned who = 0; who < 2; ++who)
    for (unsigned animal = 0; animal < 5; ++animal) {
      W16(ram + 0xde2, 0x190 + animal * 4);
      W16(ram + 0xe18, 79 + animal + who * 0xa3);
      W16(ram + 0xe76, 94 + animal + who * 0xa3);
      Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 100 + who * 5 + animal);
      CHECK(Dkc2KongsActiveActors() == 1);
      const int y = who ? 94 : 82, x = (who ? 98 : 110) + (int)animal;
      memset(&ppu->objBuffer, 0, sizeof ppu->objBuffer);
      CHECK(Dkc2KongsOamVisible(y, 0) == 1);
      CHECK(Dkc2KongsRenderOam(ppu, y, 0));
      CHECK((ppu->objBuffer.data[x + kPpuExtraLeftRight] & 255) == 145);
    }
  W16(ram + 0xde2, 0x190); W16(ram + 0xe18, 79); W16(ram + 0xe76, 94);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 200);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 201);
  CHECK(Dkc2KongsOamVisible(82, 0) == 1);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 202);
  CHECK(Dkc2KongsOamVisible(78, 0) == 1);
  // SNES Start pause continues host frames but must freeze the rider phase.
  W16(ram + 0x8c2, 0x40);
  for (unsigned t = 203; t < 383; ++t) {
    Dkc2KongsPrepare(ppu, ram, rom, 0x400000, t);
    CHECK(Dkc2KongsOamVisible(78, 0) == 1);
  }
  W16(ram + 0x8c2, 0);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 384);
  // Reset the test timeline, then restore the second pose.
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 200);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 201);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 202);
  // Repeated presentation does not advance; movement and restore reset phase.
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 202);
  CHECK(Dkc2KongsOamVisible(78, 0) == 1);
  W16(ram + 0xe66, 1);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 203);
  CHECK(Dkc2KongsOamVisible(82, 0) == 1);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 100);
  CHECK(Dkc2KongsOamVisible(82, 0) == 1);
  // Facing left mirrors the attachment around the committed sprite origin.
  ppu->oam[0] = (92 << 8) | 92; ppu->oam[1] = 0x72a0;
  W16(ram + 0xdf4, 0x72a0);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 101);
  memset(&ppu->objBuffer, 0, sizeof ppu->objBuffer);
  CHECK(Dkc2KongsRenderOam(ppu, 82, 0));
  CHECK((ppu->objBuffer.data[89 + kPpuExtraLeftRight] & 255) == 145);
  // A compound jump follows the animal graphic and explicit rider offset.
  ppu->oam[0] = (92 << 8) | 100; ppu->oam[1] = 0x32a0;
  W16(ram + 0xdf4, 0x32a0); W16(ram + 0xde2, 0x19c);
  W16(ram + 0xe18, 82); W16(ram + 0xe76, 112); W16(ram + 0xe58, 0x1000);
  Dkc2KongsPrepare(ppu, ram, rom, 0x400000, 300);
  CHECK(Dkc2KongsOamVisible(69, 0) == 1);
  memset(&ppu->objBuffer, 0, sizeof ppu->objBuffer);
  CHECK(Dkc2KongsRenderOam(ppu, 69, 0));
  CHECK((ppu->objBuffer.data[116 + kPpuExtraLeftRight] & 255) == 146);
  // Holding the animal at an airborne frame must hold the corresponding pose.
  for (unsigned tick = 301; tick < 320; ++tick) {
    Dkc2KongsPrepare(ppu, ram, rom, 0x400000, tick);
    CHECK(Dkc2KongsOamVisible(69, 0) == 1);
  }
  MovesTests(ram, rom);
  GlideTests(ram, rom);
  HandoffTests(ppu, ram, rom);
  TeamTests(ppu, ram, rom);
  Dkc2KongsUnload(); CHECK(!Dkc2KongsReady());
  free(ppu); free(ram); free(rom);
  puts("Project Kongs tests passed");
  return 0;
}
