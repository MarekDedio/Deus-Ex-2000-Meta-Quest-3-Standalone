# Quest OpenXR input observation handoff

Implementation2026-10-28, desktop/Android-native verified. This is a render-owner to
CPU-owner observation connection, not completed original UpdateInput, Input.Init,
ResetInput, ReadInput, FExec, player startup or live crate gameplay.

## Producer and binding

quest_main creates QuestInputSource after validating actual stereo targets and
creating their NativeClientLifetime. That same source/token/client/viewport is
captured by the initial CPU worker and bound to its prepared native viewport.
Matching numeric IDs with a different lifetime token refuse. A bound source
cannot be replaced until client teardown. The runtime binding is not a native
vtable, WinDrv identity, render-device object or OpenWindow capability.

After the SDK synchronizes actions and the port validates head/controller poses,
SyncActionSets publishes the real frame: predicted display time, delta, focus,
mounting/head-valid/head-tracked flags, seven explicitly mapped button states,
independent left/right sticks, trigger/grip scalars, and head/grip/aim poses.
No XR/GL handles or UObject pointers cross the boundary. Raw observations retain
inactive/focus-lost/tracking-lost state rather than hiding it behind a valid-head
gate. Original-key/axis mapping and release dispatch remain future consumers.
Existing presentation controls are unchanged; the source dispatches no actions.
Sticks use SDK normalized axes, triggers/grips use normalized scalars, and poses
remain metre/quaternion observations in the current tracking space, not Unreal
world coordinates. Future aim consumers must bind the corresponding reference-
space/recenter transform and world mapping rather than interpreting these raw
poses directly as original Actor positions or rotations.

## Bounded history and transaction semantics

The shared fixed64-row ring has a mutex and no publication-time heap allocation.
Each accepted observation receives a monotonic sequence; nonfinite/invalid
metadata, axes or tracked poses refuse. Invalid observations advance an explicit
rejection revision, not the sample sequence, making later staging require reset.
A rejected newest physical frame cannot expose an older sample as a valid reset
baseline: another valid observation must arrive first. Rejections saturating
their revision permanently require reset for normal reads.

Stage copies a coherent immutable batch without consuming shared history or
mutating its caller's cursor. The future world transaction must retain its old
cursor on rollback and publish the returned cursor only at its own commit. A
second read with the committed cursor yields no duplicate samples. This API is
not itself an implementation of transactional original Input processing.

An uninitialized cursor or overwritten required history returns NeedsReset;
history loss is never silently treated as complete event delivery. Explicit reset
copies only the newest valid physical baseline and records its source/sequence/
rejection revision. It does not claim to replay lost press/release transitions.
Source identity prevents cursor reuse even if viewport metadata is reused.
Initial data preparation may take longer than ring capacity; this is expected
and requires an explicit original-input initialization baseline later.

NativeClientLifetime revoke invalidates publication, snapshots and staging.
Client teardown releases all runtime bindings before native viewports retire;
the render owner releases its source after worker completion. The independent
binding ledger meters source/control-block and vector storage, and staged batch
copies are admitted through the shared state budget. Sources/cursors do not
enter actor exports, save files, world revision, map collision or render targets.

## Verification gates

The standalone test exercises actual concurrent producer/consumer access,
sequence coherence through wrap, explicit initialization/overflow/reset, retry
with an unchanged cursor, invalid samples and rejected-baseline refusal,
focus/tracking observations, source mismatch and revocation. Its concurrent
observation count can vary with scheduling; every inspected row must be coherent.

The owned viewport/Canvas fixture verifies missing-source refusal, exact shared
lifetime rather than matching IDs, idempotent same-source binding, wrong-viewport
refusal, non-consuming runtime staging and revoked client teardown. Existing
Canvas allocation/GC/save/revision rollback controls run with this bound source.

Desktop/strict ARM64/Android-native compilation passes. The standalone source
test passes15 refusal controls plus deterministic and concurrent row checks
(1481 checks in the recorded standalone run; thread scheduling changes the row
count). The owned binding/Canvas fixture passes150/14, original Console5148/26,
HUD11225, Core SET9548/6, GC155/16, metadata13896/125, Input schema34355/1357
and all eight first-Training regressions. CTest230 executed/277 registered/
47 explicit owned-data skips/zero failures,117.23s.

Integrated two-Fire repeat2 retains271677 instructions/32553 writes and its
exact FExec SET guard/full undo; doors retain239536/19432, both56-unit moves/
two ends/exact ReadInput guard/full undo. Neither diagnostic has this source
consume controller actions or bypass missing original services. All45 original
files and640 frozen compiler inputs match length/UTCmtime/SHA256; all jobs are
terminal. The initial Android button-mask signedness compile error is retained
as discovery evidence, not a pass; the corrected native link succeeds.

A physical headset test is still required to verify the
real producer in operation. No completed UpdateInput/ReadInput, native input
reset, original key dispatch, new APK or playable tutorial is implied. Next is
the actual reset/reconciliation/processing service described in
ORIGINAL-QUEST-INPUT-PROVIDER.md, followed by the committing VR gameplay bridge.
