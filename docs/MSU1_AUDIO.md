# MSU-1 replacement music

In Escape > Settings > Audio, choose the extracted folder containing
`dkc2_msu1-N.pcm`, enable Replacement music (MSU-1), and set Music volume
(0-200%). Master Volume still controls music and SFX together.
`track-N.pcm` and `dkc2_msu-N.pcm` are also recognized. Files need an MSU1
header, little-endian loop-frame index and 44.1 kHz stereo signed 16-bit
samples. The folder needs valid track 1. Archive extraction is external;
BPS patches and `.msu` marker files are unused.

Only the resident checksum-verified US v1.0 ROM receives the two-byte music
scheduler policy. The ROM file is untouched. Original song/sample/SFX uploads,
SPC acknowledgements and gameplay continue. Scheduler validation compares the
surrounding loaded APU code to the verified image. At a mute transition only
music-owned voices are released; the old shared echo tail is discarded.
No blanket DSP mute is used; new effects retain their normal paths.

SPC commands at $B5:81FB drive song selection on generated C and interpreter
paths. $00FF prepares a bank, $xxFB selects a variant, and $00FE starts it.
Track number is primary + 40 * variant. Alternate SFX banks 32, 34, 35 and 37
map to bonus bank 15; bank 33 maps to boss bank 21. These aliases were verified
against the supported ROM song-data pointer table. Looping uses the header;
fanfares, deaths, victories and target jingles play once. Flying Krock, Token
Tango, Screech and Rambi Chase loop. Missing/invalid tracks restore stock music.

`msu1.cfg` remembers enablement, volume and the external path.
`DKC2_MSU1_PATH` overrides the folder; an empty value disables replacement.
`DKC2_MSU1_VOLUME=0..200` overrides volume. `DKC2_MSU1_TRACE=1` logs selection.

PCM position is host-only. Loading a save or rewinding restarts its song and
variant, recovered from the APU sequence-table pointer. Returning to SNES music
mid-song resumes its paused sequence. Exact sample synchronization, seamless
rewind and fade-duration matching remain unverified. Native output stays
32040 Hz with linear resampling. File mappings avoid opening tracks during
playback, although the OS can still page data.

Synthetic tests cover looping, one-shots, resampling, gain clipping, commands,
aliases, fallback and state restoration. `dkc2_spc_music` checks all 256 SFX
ownership masks and rejects mismatched scheduler bytes. For a real pack:

```
python scripts/check_msu1_pack.py --runner EXE --rom ROM --state SAVE --pack DIR --output PRIVATE_DIR
```

This checks headers, equivalent gameplay/video hashes, changed replacement
audio, nonzero stock SFX at zero music volume and exact missing-track fallback.
Full-game transitions and physical listening remain separate acceptance.
