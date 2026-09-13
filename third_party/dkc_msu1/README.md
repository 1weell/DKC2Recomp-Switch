# MSU-1 host audio provenance

PCM reader/mixer and SPC policy restoration adapted from DKC3Recomp
`3a033f19801a2bd3abf784d4b29c4462495d19de`, runner/dkc3_msu1.c,
runner/dkc3_spc_music.c and the synthetic policy test (MIT).
Windows memory mapping adapted from DKC1Recomp
`aa196393ece613d3b04adf437f38f6f2d5364649`, runner/dkc1_msu1.c (MIT).
The corresponding license texts accompany this file.

Local adaptations: DKC2 US v1.0 command observation and scheduler offset,
primary/secondary track mapping, live menu and configuration, missing-track
fallback, Windows support, bounded gain and file-size checks.
The user's MSU patch, music and all ROM data remain external. No patch code
or music assets were copied. Track numbering is format interoperability metadata.
