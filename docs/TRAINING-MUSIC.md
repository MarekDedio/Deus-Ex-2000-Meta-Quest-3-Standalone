# Original map music

User priority one is restoring Training's background music. The first music
implementation reads the loaded map's actual slot-zero LevelInfo Song and
SongSection through qualified typed fields. It loads the referenced Engine.Music
export from the user's Music directory, respecting original UMusic serialization
(format name, version-dependent lazy offset, compact byte count, bounded payload).
No commercial UMX/module/audio is copied into source control or the APK.

The existing pinned SurrealEngine dependency supplies libopenmpt under its
BSD license. The standalone decoder does not load the original Windows game or
DLLs. Original module bytes are decoded on a producer thread into a fixed
65,536-stereo-frame SPSC ring (256KiB). Eight blocks are prepared before
publication. AAudio only mixes ready PCM, without decoder calls, file reads,
allocation or waiting on the producer. The audio mutex already used by the
existing ambience/dialogue mix protects stream replacement; old decoder thread
destruction is outside that lock. Map-worker results publish with the map, and
focus/pause gating preserves the cursor when the app is not focused.

The baseline mix gain is 0.35, combined with existing ambience and original
dialogue. It is not yet the original player's settings/volume graph. Authored
None remains silence. Invalid sections fail visibly instead of silently
clamping to another track. Dynamic combat/death sections, script music events,
original transition fades and saved playback position are not yet implemented;
this is map-authored baseline background music, not the full player audio engine.

Generated tests use a newly composed minimal ProTracker waveform, never game
music, and exercise input/rate/section refusal, initial PCM, wrap/refill and
callback constraints. Owned-data integration selects Training through the actual
map metadata and checks its decoded PCM. Headset audibility and underrun checks
remain required after installation. MUSIC diagnostics report stream presence,
playback focus gate, sample rate and accumulated missing PCM frames.

libopenmpt, miniz, minimp3 and stb_vorbis notices are included in APK
assets/licenses. The original source notices are retained in the pinned
third-party checkout. The newer tracker Vorbis implementation has prefixed
symbols, isolated from the older Meta sample-framework decoder; neither is
silently linked against the other's opaque handles or declarations. The
generated host test also links the actual Meta decoder as a collision check.

The owned-data test passes: actual 00_Training.LevelInfo0 references
Training_Music.Training_Music, format it, section 0, decoded duration 123.726s,
title Training Room. Nonzero PCM is verified and map/UMX fingerprints remain
unchanged. The final codec-isolation host build and 82-test run pass 72 ordinary
tests with 10 explicit owned-data skips, zero failures, 11.37s; separate original
music integration passes. The generated test consumes past its song duration
to verify repeat playback, as well as circular-buffer wrap/refill. Diagnostic
mock regressions pass 25/0. Explicit final AppShutdown cleanup also closes
AAudio defensively if no session reached its normal SessionDestroying cleanup.

Final signed ARM64 APK is 32,918,766 bytes, SHA256
8673B16C3F37B5771DC0470299786D4834BF2DCD6062B426C66388416FC536C1;
APK v2 signature verifies. Automatic installation was attempted and skipped
because the known Quest is absent from USB. Physical audibility, mix balance,
pause/resume and sustained zero-underrun checks still require the headset.
