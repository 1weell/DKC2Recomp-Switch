#ifndef DKC2_KONGS_H
#define DKC2_KONGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Ppu Ppu;

#ifdef __cplusplus
extern "C" {
#endif

enum { kDkc2KongOriginal, kDkc2KongDonkey, kDkc2KongKiddy, kDkc2KongCount };
bool Dkc2KongsLoad(const char *path);
bool Dkc2KongsLoadBytes(const uint8_t *bytes, size_t size);
void Dkc2KongsUnload(void);
bool Dkc2KongsReady(void);
const char *Dkc2KongsStatus(void);
const char *Dkc2KongsPath(void);
size_t Dkc2KongsFrameCount(void);
int Dkc2KongsChoice(int slot);
void Dkc2KongsSetChoice(int slot, int choice);
void Dkc2KongsInitialize(void);
bool Dkc2KongsSaveSettings(void);

/* Called only by the simulation's verified native instruction hooks. Returns
 * zero to continue, or a native PC to redirect to. Drawing stays read-only. */
uint32_t Dkc2KongsGameplay(uint8_t *wram, const uint8_t *rom, size_t rom_size,
                          uint32_t pc, uint32_t tick);
void Dkc2KongsReset(void);
bool Dkc2KongsUseCallbacks(const uint8_t *wram);

/* Presentation only. Native WRAM, VRAM and OAM are never modified. */
void Dkc2KongsPrepare(Ppu *ppu, const uint8_t *wram, const uint8_t *rom,
                      size_t rom_size, uint32_t tick);
void Dkc2KongsBeginLine(Ppu *ppu);
void Dkc2KongsEndLine(Ppu *ppu);
bool Dkc2KongsRenderOam(Ppu *ppu, int line, unsigned slot);
int Dkc2KongsOamVisible(int line, unsigned slot);
unsigned Dkc2KongsActiveActors(void);

#ifdef __cplusplus
}
#endif
#endif
