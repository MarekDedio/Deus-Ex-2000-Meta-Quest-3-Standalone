# Original blend animation commands (2026-10-10)

Actor native1010 PlayBlendAnim and native1012 TweenBlendAnim are implemented
as real four-channel clock mutations, not successful stubs. Their contracts
are characterized from the user's original GOTY Engine.dll, SHA-256
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
The original binary was inspected read-only, never loaded or modified.

## Primary evidence

Preferred-image VAs (PE32 base10300000) are not portable memory layouts:

- execPlayBlendAnim thunk10302374 resolves to body103e0d80. Rate defaults
  to1 at103e0db3; TweenTime defaults to-1 at103e0dd7; BlendSlot is a signed
  Int with a0..3 guard at103e0e1e–103e0e2c. Original Engine.u declares Int,
  unlike the pinned NActor wrapper's Byte parameter.
- execTweenBlendAnim thunk10302a6d resolves to body103e1300, using a required
  Float Time and optional Int slot. Positive Time produces a negative
  -1/NumFrames at103e140d–103e141a, not the pinned positive frame.
- Both use exact UMesh.GetAnimSeq lookup: thunk10301edd →1031c050, which
  returns null for a missing sequence instead of falling back to sequence0.
- Play first suppresses tween when the prior blend name is None
  (103e0e54–103e0e5e). Single-frame targets retain frame-1 and stop/reset
  rate, oldRate and minRate. Explicit positive tween uses1/(frames*time).
  Automatic TweenTime=-1 retains the negative frame, selecting the previous
  OldBlendAnimRate (not MinRate): positive old rate directly; negative old
  rate uses max(speed*-oldRate,newRate*.5); zero uses1/(frames*.025).
- SimBlendAnim Plane fields are X=frame*10000, Y=rate*10000,
  Z=tweenRate*1000, W=last*10000 (Play103e1053–103e10d5;
  Tween103e1433–103e1494). Repeating an identical Play increments W by1;
  Tween does not apply that repeated-play signal. The pinned field ordering
  and positive TweenBlend frame are not original behavior.

No LoopBlendAnim declaration/export was found; no invented native is added.
This static characterization is not a rendered original-DLL equivalence test.
The existing main clock still follows the pinned implementation, and its
bounded blend elapsed driver is not newly certified against original Tick.

## Runtime state

Arguments are evaluated before original invalid-slot/no-mesh/missing-sequence
return branches. Slots are not masked to Byte. A successful command journals
the actor, captures portable render tween history, synchronizes all four blend
property arrays and the actual Plane values, then publishes through the
existing transactional boundary. Failure in enclosing script restores the
clock, properties, native edges and world publication together.

Authored and script-written SimBlendAnim values are imported into the clock.
New checkpoints retain their reflected Plane view as well as the existing
four native floats. Older portable saves without the reflected Plane records
derive them from the already-saved clock; partial/disagreeing records reject.
Before staged GC allocation, the derived records are charged against the same
cumulative property, value-node, retained-byte and encoded-byte limits as the
original trailer. An older save at the property limit cannot bypass that limit
by omitting the reflected view. This is a bounded, non-copy codec measurement.
No wire codec version or original commercial save compatibility is introduced.
Render tween history is portable metadata, not a claimed original pointer/cache
layout. Live frame scheduling, world startup, possession and full NPC AI remain
separate unfinished work.

The original Engine.Pawn.PlayTurnHead helper uses slot3 for HeadLeft/Right/Up/Down.
The DeusEx override adds view rotation using a Switch at logical PC52. Explicit
In this blend increment, StartUp reached that unsupported Switch opcode5 and
rolled back the entire enclosing transaction. The subsequent
[Switch/Case increment](SWITCH-CASE.md) completes the unchanged override and
advances explicit StartUp to FRand195 at PC9; automatic campaign startup is
still unfinished.

## Cold mesh material cache

An actual isolated TweenBlendAnim capture exposed a separate shared loading
bug: animation natives cached mesh geometry without its qualified texture
paths. Later map decoding reused that incomplete cache, so material selection
rejected every triangle even when the actor had valid MultiSkins overrides.
Read-only HasAnim/GetAnimGroup and the earlier main animation commands could
also take this lazy path. A single loader now resolves geometry, animation and
all texture references locally before publishing the immutable mesh to either
the animation or map-decoding path. A failed script transaction may retain a
complete immutable asset cache, but cannot publish a partially hydrated mesh.

## Verification

- The final host suite passes all 48 ordinary tests; three optional original-data
  entries require an explicit game root and are verified separately. All three
  original actor/runtime-save/class-default integrations pass on the user's
  read-only installation, including the 996 class-default/lifecycle checks.
- Pure animation command/clock tests pass 200 controls, including first-None,
  automatic old-rate branches, negative tween frames, repeated Play's W signal
  and invalid signed slots. These are not original-rendered-frame comparisons.
- Actual original Engine.Pawn.PlayTurnHead returns successfully for directions
  1–4 on slot3. Main/other-channel rates, flags and history remain unchanged.
  Tween1012, reflected Plane import without a clock, partial/disagreeing Plane
  refusal, enclosing-command rollback and current-session save roundtrips pass.
  The blend roundtrip is not a process-restart test.
- A true-cold original-data regression runs before any mutation/map-wide
  decode: Doctor1 HasAnim loads GM_Trench, while JaimeReyes0 TweenBlendAnim
  loads the independently cold GM_Trench_F. Both resolved texture-path arrays
  match a separate original package-table oracle. Drawable poses, every
  vertex's selected material layer (3,618 checks), read-only actor/save state,
  enclosing command rollback and complete caches after legacy reset are checked.
- Post-fix 720×720 Doctor1 and JaimeReyes0 captures each render 603 triangles
  with seven original material overrides and zero missing selections. Their
  coverage is 7.8808% / 8.3084% (unchanged 1% gate), hashes
  `387352f2f629ce78` / `b90136ab0658dcca`. Both textured full-body frames were
  inspected. They explicitly invoke TweenBlendAnim Still/0.3 before decoding;
  no clock elapsed tick occurs. These CPU fixtures establish cache/render
  availability, not original-renderer equivalence or headset animation fidelity.
- Generated legacy import controls prove a legal input with 65,536 properties
  and 16,450,159 retained bytes fits its 16,777,112-byte trailer cap but rejects
  adding four derived Planes before publication. 65,532 input properties allow
  read-only validation with the four derived records. Rejections leave saved
  bytes, script state, actor index and GC allocations unchanged.
- The final ARM64 APK builds and verifies with signature scheme v2. It is
  archived locally, not installed over the accepted seated-player build; its 16
  ZIP entries contain no original commercial game packages. Hash and path are
  recorded in [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).

Ignored local evidence is under `artifacts/blend-animation-20261010/`:
`clock-final.log`, `inventory-budget-final.log`, `original-actor-cold-cache.log`,
`original-runtime-cold-cache.log`, `original-defaults-cold-cache.log`,
`ctest-final-v4.log`, `host-build-cold-cache.log`, `android-build-cold-cache.log`,
`apk-signature-cold-cache.log`, `tween-blend-visual-fixed.log`,
`tween-blend-jaime-fixed.log` and their BMP/JSON pairs. The initial CTest failure
retains the missing generated Plane schema; it was corrected, not bypassed. The earlier original
run retains the stale native1010 gate; that increment's final test expected the
actual Switch dependency (subsequently implemented in [SWITCH-CASE.md](SWITCH-CASE.md)).
Failed pre-fix captures retain the incomplete-cache material
omission; their coverage gate was not relaxed. No unsupported execution was
treated as successful startup.
