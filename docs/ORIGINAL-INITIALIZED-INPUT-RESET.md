# Original later ResetInput on an initialized native Input

Implementation2026-11-02 after verified/pushed479845c Init/ReadInput batch.
Focused desktop, Android-native and strict ARM64 runtime/helper checks pass.
Actual-owner validation, full desktop and coherent test gates now pass.
This is not completed startup, gameplay or an APK release.

## One reset body, distinct entry authority

RunNativeViewportInputReset shares the audited actual field/iterator/update
providers for initial Init and later Reset. InitializeNativeViewportInput
retains strict constructor-only entry, genuine Console prerequisite, private
lexical Init command lease, Viewport pointer store, actual reset, initialized
log and only-then completion receipt. Its previously verified primitive
diagnostic remains separate and unchanged in purpose.

ResetNativeViewportInput instead requires the actual currently initialized
Input/Pawn/Viewport authority. It calls ResetOriginalInput, not Init. It neither
requires Console nor installs an Init lease. Init-only native Viewport stores
and logging explicitly refuse in this mode; no receipt is created/replaced.
All seven original receipt members are compared explicitly after the body,
not by padding-sensitive memcmp. Native payload COW and script reference undo
remain in the same actual outer root, work/state/write budgets and teardown.

Both modes perform original255 live key-byte clears, independent inherited
Byte then Float property walks with flags4, one actual selected store per
declaration, then native Action/Delta clears and real UpdateInput(1). No Bool/
Int clearing, ArrayDim sweep, guessed axis-name list or no-op polling is added.
The independent physical-held baseline is retained while original Keys clear,
so platform reset releases old controls and represses current ones even for
the same observed physical sample.

## Specific regression evidence

Generated strict desktop tests currently pass114818 checks/363 refusal
controls. Added assertions require later Reset to work WITHOUT Init's
writeInputViewport/initializedLog providers, and prove that a real platform
callback sees cleared keys/accumulators while its re-press state survives after
return. Existing exact inherited declaration/store order, current Actor/Next
rereads, exception prefixes, every work cutoff and lexical cleanup remain.
These fixture checks alone do not establish the actual native service.
Fresh owned-data regressions pass viewport Input10315/24, original alias80/10
and Root300/10. Those separate unchanged-constructor/command/Root checks do not
stand in for the new actual initialized-Reset primitive gate below.

The extended owned Training diagnostic calls the new Reset primitive only
after genuine Init and pre/post/rate ReadInput checks in its existing rollback
root. It requires255 key/13 Byte/15 Float resets, unchanged initialized receipt,
physical baseline and source cursor, actual release1/press1/axes2 on the same
source row, and source-period axis contributions approximately1.8/2.4. Its
ordinary Init/ReadInput observations are kept separate, not overwritten by
later-reset counters.

The actual first-Training gate passes in659.343 seconds:267089 original
instructions/38087 writes,14254760/16777216 native work, no exhaustion.
The observed laterReset1/resetKeys255/resetBytes13/resetFloats15 accompany all
previous Init/trigger/pre/post/rate checks. Actual non-Entry brush construction
still completes25 movers/150 polygons/150 nodes/600 vertices/600 points, then
the root reaches the SAME existing post-partition client/player/detail refusal.
Full save/state/revision/hash/RNG/client/lookup/Actor/GC rollback,45 unchanged
owned sources and cold immutable Model replacement pass. This is actual
primitive evidence in the original world, NOT published production startup,
physical Quest testing or a benchmark of a live render/input frame.

The full desktop build passes. CTest passes262 executed/321 registered/59
explicit owned-data skips/zero failures in135.18 seconds. The relevant owned
gates above ran separately, not through that environment-free CTest run.
Final audit verifies1934 frozen inputs,45 originals, three selected assets and
four static native source DLLs unchanged, zero live jobs and no new APK.
Independent read-only review finds no blocking COW/authority/ordering issue.

## Not a misplaced production Init/reset

The normal production startup call sites remain incomplete. The explicit
primitive diagnostic does not insert Init or Reset ahead of LoadMap's required
audio provider or claim to execute the initial GameEngine driver. That driver
must distinguish raw XR target registrations from actual native Client viewport
objects and preserve its real initial Console/Spawn/Init/OpenWindow/detail/
audio ordering. Later LoadMap retains brush ->Audio.GetViewport/SetViewport ->
live viewport ResetInput ->conditional render-device operation ->DetailChange.
See ORIGINAL-QUEST-INPUT-PROVIDER.md and
ORIGINAL-POST-PARTITION-AUDIO-CONTINUATION.md for those concrete next seams.

No resource limit, save format, binding, accepted HUD/hands/body/NPC or
other-map integration is changed. Full campaign scope remains open; first
Training is the immediate gameplay target. A linked native library is not a
new APK, and a rollback-only test is not a committed playable frame.
