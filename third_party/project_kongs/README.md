# Project Kongs reference provenance

- Repository: https://github.com/H4v0c21/DKC-2-Project-Kongs
- Exact revision: `abbb77896e47a23925d7fa0d1a546a4b7a9c5f7e`.
- Licensing evidence: on 2026-09-05 the user relayed the author's explicit MIT
  license declaration and authorized use. This revision contains `credits.txt`
  but no LICENSE file; the declaration is user-supplied rather than independently
  verified in the checkout. The applicable standard MIT terms are reproduced
  in `LICENSE-MIT.txt` with that qualification.
- No upstream assembly/source files or binary assets are vendored here.
- `scripts/import_project_kongs.py` reads the user's external assembly data
  declarations, projects their visual loops/calls/carry/rider frames, decodes
  the compound 4-bpp graphics and the first palette variant, and writes a
  private pack. Game-code animation callbacks are omitted. Original DKC2
  remains the gameplay authority. Several missing/borrowed poses use related
  character poses; the output JSON lists fallback entries.
- Authors credited by the reference: H4v0c21, Mattrizzle, BlueImp, Phyreburnz,
  Rainbow Sprinklez, p4plus2, and Sticky_Brush (see the external credits file).
- Game-derived art is not licensed for redistribution by this provenance
  note. It stays in the user's external pack, outside the source and bundle.

Mounted-animation adaptations read `objects/source/kong_animal_offsets.asm`
and the external animation declarations as data. Version 2 keeps compound
`$85/$86` animal/rider pairs and offsets, and chooses poses from the live
animal animation. It separates Donkey's movement branch and Kiddy's unlabelled
movement tail from seated idle cycles. Kiddy on Squitter uses his seated art;
the reference's mount label otherwise cycles his crouch because its transition
callback is not executed. The reference still borrows Diddy's `$1F08` for
Kiddy/Rattly: the local presentation uses Kiddy's `$4100` with a seven-pixel
frame-bottom adjustment. These adaptations retain original DKC2 gameplay.

The companion PPU integration is project-owned:
`cmake/ProjectKongsPpu.cmake` makes a guarded build-local adaptation of
`snesrecomp/runner/src/snes/ppu.c` at submodule revision
`3a929cd30336f2b3a077df17912719be63142299`. It adds original OAM visibility and
raster callbacks before normal background/window/color-math composition. The
upstream submodule is unchanged; its license/provenance remains under
`third_party/snesrecomp/` and `snesrecomp/LICENSE`.

## Ground attacks and barrel handling (September 5 follow-up)

Version 3 imports the hand-slap sequence and carry-command hand offsets as
private data. The host adapts the hand-slap hitbox (0, -52, 53, 60), three
impact times (18/26/32), target-type eligibility and native defeat-bit contract
from the reference's `bank_BC.asm`, `bank_BE.asm`, `bank_42.asm`, and
`kong_special_animations.asm`. Donkey/Kiddy throw callbacks are retimed to
22/18-frame releases, and forward velocity is 0x0c00/-0x0100. These are local
behavior adaptations under the user-relayed MIT authorization recorded above.
Kiddy's solo body slam is a local adaptation of his thrown-body and landing
sequences, not a verbatim DKC3 floor-breaking behavior port. No ROM routines,
comments or game bytes are copied into the repository.

The reference's address comments do not all match US v1.0. The implementation
uses ROM-verified PCs B8:9616 (actor dispatch), B8:995F (physics continuation),
B3:9FE7 (carried placement), B9:D8AC/D967/DFD5 (throw callbacks), B9:D9E0
(velocity before terrain adjustment), and B9:D9B0 (callback RTS). Its extra
hand-slap clipping entry is absent from the base ROM and is represented by the
host hitbox above. Native enemy clipping still comes from the private ROM.

## Flight and tag follow-up

Pack v4 projects each participant of the reference's paired `$8A` tag frames,
stopping at its control-transfer callback. Its direct DK/Kiddy pair entry is
unfinished. The local adaptation uses Donkey's DKC1 incoming/outgoing tag
gestures and Kiddy's DKC3 incoming gesture for both roles, synchronized with
DKC2's original release/hop. Original Dixie retains her flight action; the
replacement gate at B8:C924 skips only its flight-specific portion.

Additional read-only references supplied by the user:

- DKC1Recomp `ee6d662b75021acfdd0592324aab0acf01344f57`: the Baby Kong
  animation map distinguishes `Kiddy_KiddyTakesLead` and `Kiddy_DixieTakesLead`.
  Its frame registry independently identifies the DKC3 `$D7:3323` tag art
  already supplied by Project Kongs. Its optional velocity tuner is a local
  approximation and was not treated as original Kiddy physics or copied.
- DKC3Recomp `5afdacbd284144a92521d5219250d39ddca167c3`: generated Kiddy
  entry B8:9F34 confirms a separate character control block and animation
  dispatch. Its ingester points to DKC3-Disassembly
  `bed96892f5e85eabd5c920306f00b361c2e1f34c`; bank FF's interleaved
  Dixie/Kiddy constants distinguish zero Kiddy glide values from Dixie's.

Both recomp repositories declare MIT licenses. No source was copied from
these additional references, and none of their generated game data is vendored.


## Paired team animation follow-up

Pack v5 reads the reference's team-top idle/walk/air/throw sequences as data.
DK's borrowed Diddy top idle is replaced with an adaptation of DK's seated
and tumbling art. The local renderer synchronizes the two participants and
uses contact points for shoulders and throw hands. Native US v1.0 team
callbacks are verified at B9:DCEA (prepare) and B9:D8BE (release), distinct
from the barrel callback. Their instruction streams include paired $8A
commands, which the bounded seeker now recognizes.

The already recorded DKC3Recomp ingester/disassembly provenance was followed
to DATA_F92750 in DKC3-Disassembly bank F9: its paired Kiddy windup confirms
five successive hand offsets (24,-8), (18,-3), (12,2), (20,0), (28,-2), then
the B9:B0C3 release callback at 18 elapsed frames. These numeric observations
inform the local contact adaptation; no assembly, comments or generated code
from that reference are copied. DK uses the supplied team throw's 15-frame
release with locally adapted contact positions. The native DKC2 throw path
still controls trajectory, collisions and recovery state.

The post-throw recovery correction isolates Kiddy sit-up graphics
$3F40/$3F44/$3F48/$3EE0 with source durations 16/4/4/4 from the supplied
`kiddy_death` declaration, then holds the seated frame. Death, crying and CPU
callbacks are excluded. This local waiting-pose adaptation replaces the
borrowed Diddy team-stunned entry's incorrect generic hurt fallback. No new
graphics are extracted or vendored, and the pack remains version 5.
