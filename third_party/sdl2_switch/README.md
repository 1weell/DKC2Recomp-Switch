# SDL2 font helper on Switch

The Switch menu links SDL2_test's SDLTest_DrawString and cleanup helper from
installed devkitPro package switch-sdl2 2.28.5-4 (SDL 2.28.5). SDL2 itself was
already a host dependency. No SDL source or font data is copied into this tree.
The helper renders through the existing SDL renderer; no desktop windowing
or TTF dependency is introduced. Local adaptations: none to SDL.

Upstream: https://github.com/libsdl-org/SDL/tree/release-2.28.5
Package: devkitPro switch-sdl2 2.28.5-4. License notice copied from the installed
SDL_test_font.h is preserved in LICENSE.txt. Build manifests record the actual
installed package revision; updating it requires another build/UI check.
