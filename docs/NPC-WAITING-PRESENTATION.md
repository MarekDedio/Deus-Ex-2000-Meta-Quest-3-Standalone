# Original NPC waiting presentation (2026-10-15)

User priority: remove Training NPC T-poses before continuing breakable crates.
The preceding tracked keyring/knife/crowbar and quiet-trigger build received
the user's physical acceptance: "everything works."

## Cause and implementation

Training's NPCs inherit AnimSequence=Fighter, absent from their actual meshes.
The ordinary snapshot sampler consequently falls back to the first All frame,
which is the reference/T-pose. Original world/AI startup is still incomplete;
it has not issued and advanced the usual character animation commands.

A read-only `PreviewPortableActorWaitingPresentation` executes each real
ScriptedPawn's current virtual PlayWaiting helper in a rollback-only VM
transaction. It retains a detached animation/pivot result but restores every
property, native clock, RNG, birth, deletion and world revision. No State,
BeginPlay, possession, equipped NPC weapon or initialized AI is fabricated.
Immutable asset caches may remain prepared. This is deliberately a renderer
bootstrap, not successful original world startup.

The original helpers select BreatheLight for the sixteen Training humans,
with five authored frames and normalized rates0.5 or0.4 (UNATCO troopers).
Robot.PlayWaiting selects its actual one-frame Idle for both robots. Names,
rates and loop flags are returned from the scripts, not a mesh-name table.

`quest_npc_animation.h` prepares exact source-vertex positions from the same
original mesh sampler used offline. The Quest renderer uploads shared float
position textures once per clip and interpolates the two current frames in
the vertex shader. Actor transforms, original source-index remapping, winding,
UVs, material overrides, texture flags and visibility gates remain intact.
Per-frame work changes small renderer-owned uniforms, not CPU meshes, texture
data, scene banks, package data or portable runtime state. Clip resources are
shared by the retained actor banks and released at scene/session teardown.
Spatial scene replacements retain a common presentation time.

Valid authored map/script poses, including sitting, lying and saved tweens,
are preserved; only the unresolved/unusable sequence fallback is replaced.
Only neutral Fatness128, bounded authored spans without additive channels or
gameplay notifies are supported by this bootstrap. Unsupported cases report
their actor/reason and retain the ordinary snapshot route. Snapshot changes
to mesh, animation or pivot revoke the bootstrap rather than overwriting an
original runtime command. Complete original state/notify/AnimEnd scheduling,
walking, combat, speech gestures, facial animation and NPC held equipment are
not established by a breathing presentation. Actor lighting remains the
existing static direct-light approximation, not dynamic per-frame shading.

## Verification

Strict host build succeeds. Synthetic clip/time/refusal/binding checks pass
20,019 controls with eight refusals. The separate original-data run passes
280,531 checks with26 refusals, covers all18 Training mesh NPCs, and verifies
GPU-equivalent source-index interpolation against the original CPU sampler.
Every human has nonzero original breathing motion. Complete checkpoint bytes,
world revision, dirty-state flag, original source bytes and final GC roots
remain unchanged after successful and instruction-limited preview calls.
An additional actual LoopAnim command is preserved unchanged, excluded from
the waiting bootstrap, with exact checkpoint/revision equivalence afterward.

Six720x720 software previews under ignored
`artifacts/npc-waiting-20261015/previews/` show JaimeReyes0, Doctor1 and
UNATCOTroop0 at frames0 and0.35, with original textures and no missing materials.
Inspected front views show normal arms-down standing poses. These software
frames do not prove Quest shader execution or on-device comfort/performance.
The coherent full host build succeeds; CTest has93 passes,14 explicit owned-data
skips, zero failures out of107 registered in22.77s. Final Android build succeeds,
signature verifies and replace-install preserves user data. See
QUEST-TEST-BUILD.md for the exact final APK/checksum and bounded device evidence.

The first installed NPC APK produced an inspected Quest1680x1760 frame with
an arms-down NPC through the Training desk window, original speech/HUD and a
tracked keyring. Original clip textures were uploaded once (GM_Trench_F,
GM_Trench and GM_Suit); steady windows reached72fps/13.89ms, while streaming
and screenshot readback still produced slow frames. The user then confirmed
"all npcs are fine" and requested continuation with destructible crates.
The final APK adds only preservation of already-valid authored map/script
poses; the same18 Training waiting presentations pass unchanged. Its separate
exact-build capture/readiness status is recorded in the build handoff.

Offline verification (no headset):

```powershell
& 'D:\Android\Sdk\cmake\3.22.1\bin\cmake.exe' --build artifacts\prerequisites-20261009\build --target quest_npc_animation_test -j 4
& '.\artifacts\prerequisites-20261009\build\quest_npc_animation_test.exe' 'D:\Steam\steamapps\common\Deus Ex' 'artifacts\npc-waiting-20261015\previews'
```

The commercial installation is read-only; previews/APKs/assets stay ignored
and are not committed to GitHub. Crate destruction remains the next user
priority after the NPC visual replacement is verified.
