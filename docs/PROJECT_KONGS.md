# Optional Donkey and Kiddy characters

The in-game **Characters** pause-menu tab lets either original slot use
Donkey Kong or Kiddy Kong. Choose **Donkey + Kiddy** to select both, or
**Original pair** to restore Diddy and Dixie. Use the game's normal Select
button to swap the active and following Kongs. Choices apply on resume and
persist between launches.

Hold **Down** and press **Y** while grounded and empty-handed to use Donkey's
hand slap or Kiddy's body slam. Donkey stays on the ground and strikes three
times. Kiddy makes a short somersault and impacts on landing. These attacks
use native enemy defeat responses, terrain collision and impact sound.
Mounting, carrying, taking damage or switching characters cancels the attack.

Donkey carries and throws barrels overhead; Kiddy uses his original underhand
style. Hand positions, windup and release timing follow their imported poses.
Donkey and Kiddy do not inherit Dixie's helicopter flight, including when
resuming an older save during a glide. Original Dixie retains that ability.
Select swaps Kongs with paired tag gestures synchronized to the native
control-transfer hop. The reference's DK/Kiddy pair entry is unfinished, so
the host adapts their individual tag poses to both roles and both slots.
When Donkey/Kiddy are paired, team pickup, walking, jumping and throwing use
synchronized carrier/rider poses. The top Kong stays attached to the carrier's
shoulders or hands until release, then follows the native thrown trajectory.
Donkey uses an overhead team throw and Kiddy an underhand team throw. Donkey's
carried pose adapts his seated art because he has no original DKC1 team-up.
After an unsuccessful throw, Kiddy sits up once and waits for the leader;
he no longer repeats the generic hurt animation. Re-import packs created
before this recovery fix, including earlier version 5 packs.
Mixed teams with original Diddy/Dixie still use the original partner placement.
Other movement and collision remain based on DKC2's original slot; this is
not a complete DKC1/DKC3 mechanics transplant. The larger art does not enlarge
the ordinary collision box.
World-map and cutscene characters remain original. Some uncommon poses use a
related replacement pose; the import report lists fallback animations.
Both ordinary and mounted replacement animations freeze during SNES Start
pause and the native Escape menu.

Kiddy's solo body slam adapts his DKC3 somersault/landing art to DKC2; it is not
a port of DKC3's team-throw floor-breaking system. Donkey's attack does not
spawn the reference hack's bonus bananas. Mid-attack save restoration cancels
the host move and resumes a valid original actor state.

## Import your external Project Kongs checkout

The supported reference revision is
`abbb77896e47a23925d7fa0d1a546a4b7a9c5f7e` of
[H4v0c21/DKC-2-Project-Kongs](https://github.com/H4v0c21/DKC-2-Project-Kongs).
It supplies sprite declarations, compound graphics, palette variants, and
animation sequences. The importer reads these as data without executing the
reference's build scripts or assembly callbacks.

On macOS, from this source checkout:

```sh
python3 scripts/import_project_kongs.py \
  --project /private/path/DKC-2-Project-Kongs \
  --output "$HOME/Library/Application Support/Flat2VR/DKC2Recomp/mods/project-kongs.dkc2kongs"
```

On other platforms, use an external output path and select that file with
**Load character pack...**. A pack in `mods/project-kongs.dkc2kongs` relative
to the runtime user-data directory is detected automatically. The menu saves
the chosen path and pair to `kongs.cfg` separately from game saves.

The importer also creates a JSON report with frame/animation counts, fallback
poses and SHA-256. The current reference imports 715 distinct frames, 233
visual animation entries, 172 compound mounted-pose records and 63 hand
attachment records. Pack version 3 adds ground-attack sequences and barrel
attachments to version 2's mounted data. Version 4 adds incoming and outgoing
handoff sequences for both characters. Version 5 adds paired team top poses
and corrects Donkey's borrowed carried pose. Re-run the import command and
restart or reload the pack after updating. Versions 1 through 4 remain loadable.
Re-import for team throws, handoffs, ground attacks and
corrected throws; version 1 also needs re-importing for mounted riders.

Mounted riders use seated poses on Squitter/Rambi/Enguarde, a dedicated Rattly
pose and hanging poses on Squawks. Moving cycles are separate from idle loops;
compound jump/landing poses follow the animal graphic and retain rider offsets.
Kiddy's incomplete borrowed Rattly pose is adapted to his own seated art.
Keep the pack and report private; the pack contains
game art and must not be checked in or distributed with the source-only app.

## Verification

Synthetic importer/runtime tests are included in CTest. Private comparisons:

```sh
python3 scripts/check_project_kongs.py \
  --runner build/macos/dkc2_snesrecomp_headless \
  --rom /private/path/dkc2.sfc \
  --pack /private/path/project-kongs.dkc2kongs \
  --states /private/path/saved-states \
  --output /private/path/kongs-check --frames 120
```

Optional `--input /private/path/recording.input`, `--aspect 4:3` (also 16:10
or 16:9), and `--choices 2,1` exercise motion, aspect and the reversed pair.
The checker compares WRAM, VRAM, CGRAM, OAM and audio fingerprints, rejects
unmatched visible compound layouts, and writes private images and logs.
Version 3 intentionally changes machine state during carry/throw and ground
attacks; v5 also changes team-throw release timing/origin. Disabling inherited
flight also changes active glide states. Use
recordings without those actions for the preservation check.
Those checks establish game-state preservation and sprite ownership, not
visual animation correctness. Inspect mounted idle, motion, jumps, landings
and both facing directions in the resulting images and the rebuilt app.
`DKC2_KONGS_PACK`, `DKC2_KONGS=1,2` (0 original, 1 Donkey, 2 Kiddy), and
`DKC2_KONGS_TRACE=1` support individual deterministic headless/desktop runs.

Credits: H4v0c21 and Mattrizzle (programming/design), BlueImp (additional
programming), Phyreburnz (custom graphics), Rainbow Sprinklez (DKC1 research),
p4plus2 (DKC2 disassembly), and Sticky_Brush (reference hardware verification).
See [provenance](../third_party/project_kongs/README.md).
