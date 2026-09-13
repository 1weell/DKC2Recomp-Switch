# DKC3Recomp menu provenance

Source: https://github.com/elliotttate/DKC3Recomp
Revision: `3a033f19801a2bd3abf784d4b29c4462495d19de` (clean local checkout).
License: MIT, reproduced in LICENSE.

Adapted `runner/desktop_menu.{c,h}`, `runner/windows_menu.{c,h}` and the
corresponding synthetic tests into DKC2. Changes: DKC2 names, native HWND
creation shared by Win32 and SDL, queued commands, focus/menu input gating,
capability flags, player source/co-op choices and paused checkmark.
No game assets or generated game code were copied.

Display/controller follow-up at the same exact revision: compared all desktop
settings and color presets; adapted 21:9 constants/labels and shared the existing
identical GLSL reconstruction source between DKC2 SDL and native WGL. Native
GL procedure loading, capability reporting, maximum host allocations, and GPU
comparison tests are local adaptations. Stomp rumble follows DKC3's 55 ms
0x2800/0x5000 pulse and 500 ms 0x5000/0x7000 test pulse; DKC2 obtains ownership
from its accepted guest reaction and routes both players independently. DKC3's
WRAM probe and Apple-specific Metal/host code were not copied.
