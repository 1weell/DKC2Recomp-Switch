# DKC2Recomp v0.0.8 - Co-op carrying and rope fixes

The Windows update restores nearby partner carrying and fixes independent
rope climbing, animations and movement across rope nets in simultaneous TEAM
mode. The latest published **macOS arm64 v0.0.6** archive is included unchanged;
it does not contain the new simultaneous co-op or MSU-1 integration.

## Windows changes

- Walk up to the other Kong and press the team-up button (SNES A, keyboard X
  by default) to carry them. Either player can initiate pickup when close
  enough. Native carrying, jumping, dropping and throwing remain available,
  and a thrown partner regains independent movement after landing.
- Player 2 grabs the rope they touch, without moving Player 1 or changing
  camera ownership. Each player can climb up and down with the proper
  animation, including when moving in opposite directions.
- Single/double-rope junctions finish their transitions normally. Existing
  saves already stuck at a junction recover in place, and saves taken during
  unfinished transitions preserve the remaining animation.
- Player 2 can turn and cross the entire rope net to either outer rope,
  instead of repeatedly turning in place. Horizontal-rope animation callbacks
  are also enabled for independent players.
- Includes synthetic policy/emitter checks and private input-driven carrying
  and rope regressions. The Windows snapshot check now supports clean trees.

The previous CRT controls, Reconstruct options, Donkey/Kiddy character-pack
support, MSU-1 music and other co-op fixes remain included.

## Downloads

- **Windows x64:** extract `DKC2Recomp-v0.0.8-Windows-x64.zip` and run
  `DKC2Recomp.exe`. `DKC2RecompSDL.exe` is the alternate SDL host. Keep the
  bundled DLLs and assets beside the executables.
- **macOS arm64:** `DKC2Recomp-v0.0.6-macOS-arm64.zip` retains its original
  filename, version and ad-hoc signature. It is copied byte-for-byte from the
  [previous Mac release](https://github.com/elliotttate/DKC2Recomp/releases/tag/v0.0.6)
  and has not been rebuilt or run on a Mac for this release.

Supply your own headerless USA v1.0 ROM (SHA-256
`35421a9af9dd011b40b91f792192af9f99c93201d8d394026bdfb42cbf2d8633`).
ROMs, saves, character art, music packs and personal settings are excluded.
Character-import and MSU-1 setup guides are included in the Windows download.

## Validation and limits

The completed feature build passed all **86 configured tests**. Private
replays cover Diddy/Dixie and Donkey/Kiddy carrying, independent movement,
animated rope climbing, save/load, jumping and reattachment, stuck-junction
recovery and full net traversal. Live Windows testing used Player 1 on the
keyboard and Player 2 on a gamepad; the reported rope behavior was confirmed
working before release preparation.

Full-game co-op, true horizontal-rope gameplay, unusual animal transitions
and non-Windows builds of these changes remain unverified. Replacement
characters retain original DKC2 collision boxes and some movement rules.

Carried Mac archive SHA-256:
`20947ee9ee4bc64df13b2aeae68440288811f8e6b5b263bc8c4c8258352a6094`.
