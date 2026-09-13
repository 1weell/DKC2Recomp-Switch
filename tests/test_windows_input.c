#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL.h>
#include "windows_input.h"
#include "desktop_input.h"
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static bool Pressed(int scancode, void *context) {
  return Dkc2WindowsVirtualKeyFromScancode(scancode) == *(const int *)context;
}

int main(void) {
  Dkc2WindowsKeys taps = {0};
  Dkc2WindowsKeyDown(&taps, VK_RETURN);
  Dkc2WindowsKeyDown(&taps, 'Z');
  CHECK(Dkc2WindowsKeyTapped(&taps, VK_RETURN));
  CHECK(Dkc2WindowsKeyTapped(&taps, 'Z'));
  CHECK(!Dkc2WindowsKeyTapped(&taps, 'X'));
  Dkc2WindowsKeysClear(&taps);
  CHECK(!Dkc2WindowsKeyTapped(&taps, VK_RETURN));
  Dkc2WindowsKeyDown(&taps, 256);
  CHECK(!Dkc2WindowsKeyTapped(&taps, 256));
  /* The Win32 host must work before SDL video init and after launcher teardown. */
  const int keys[] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
    SDL_SCANCODE_RIGHT, SDL_SCANCODE_X, SDL_SCANCODE_Z, SDL_SCANCODE_S,
    SDL_SCANCODE_A, SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_RETURN,
    SDL_SCANCODE_RSHIFT};
  const int virtual_keys[] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
    'X', 'Z', 'S', 'A', 'Q', 'W', VK_RETURN, VK_RSHIFT};
  const unsigned words[] = {0x10, 0x20, 0x40, 0x80, 0x100, 1,
    0x200, 2, 0x400, 0x800, 8, 4};
  SDL_SetMainReady();
  for (int phase = 0; phase < 3; ++phase) {
    if (phase == 1) CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
    if (phase == 2) SDL_Quit();
    for (int i = 0; i < 12; ++i) {
      CHECK(Dkc2WindowsVirtualKeyFromScancode(keys[i]) == virtual_keys[i]);
      int key = virtual_keys[i];
      CHECK(Dkc2MapKeyboardBindings(keys, Pressed, &key) == words[i]);
    }
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_0) == '0');
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_1) == '1');
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_9) == '9');
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_F5) == VK_F5);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_F9) == VK_F9);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_F13) == VK_F13);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_F24) == VK_F24);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_KP_DECIMAL) == VK_DECIMAL);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_KP_PERIOD) == VK_DECIMAL);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(SDL_SCANCODE_MINUS) == VK_OEM_MINUS);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(-1) == 0);
    CHECK(Dkc2WindowsVirtualKeyFromScancode(512) == 0);
  }
  puts("Win32 keyboard bindings pass before SDL init, during init, and after SDL Quit");
  return 0;
}
