# Quest physical input reconciliation (2026-11-02)

Following verified update: ORIGINAL-SCRIPT-COMMAND-CALL-IMPLEMENTATION.md now
records actual original ParseLeftClick|Fire from a tracked trigger observation,
nonzero physical baseline, bFire/bJustFired and same-sample reset Release1/Press1,
with whole rollback. The earlier blanket digital-command blocker below is
historical; general imports/menus and initialized Input/ReadInput still need
work. This is not physical headset or committed gameplay acceptance.

Verified native/owned-runtime integration, not an installed APK or physical
headset acceptance. Immediate integration remains first Training. No account/player-
creation UI, other-map integration or accepted presentation change is included.

## Port mapping version1

The runtime resolves IK_LeftMouse, IK_RightMouse, IK_I, IK_JoyX and IK_JoyY
against its ACTUAL registered Engine.Actor.EInputKey Enum, not guessed numeric
key constants. Current native bindings/aliases and actual InputExt.Process
remain authoritative; the bridge never writes a Pawn input property directly.

Right trigger strictly above0.75 maps to LeftMouse; right A maps to RightMouse;
left Menu maps to I. Left stick uses the accepted radial0.18 deadzone and
clamped diagonal, then15000*actual-poll-elapsed port units at full excursion on
JoyX/JoyY. Owned Speed2 and original Action4*.01 produce300*elapsed; the original
ReadInput20/elapsed matches digital full-forward magnitude6000 before later
PlayerInput processing. Regular polls require their real CPU input-frame
elapsed, not a render row's period. Initial reset explicitly uses the newest
physical period because original Init's UpdateInput(1) has no elapsed argument.
These are PORT units, not Windows joystick units or headset-calibrated speed.
B, grips, X/Y, stick clicks and right stick remain reserved to existing VR
presentation/debug/save/load/posture/keyring/snap-turn/dialogue controls. The
later original-frame handover must prevent duplicate movement/fire/UI actions.
This does not claim complete original menu navigation or pose/aim integration.

Only focused, mounted, valid AND tracked head state admits controls. Each
controller additionally requires its own tracked flag. Inactive SDK control
values remain observations; they cannot keep a corresponding action held.
Raw metre/quaternion poses are NOT copied into original actor aim coordinates.

## Transaction and event order

Native Input now owns a separate physical held baseline/mapping identity beside
its source cursor. Deep native Input COW includes all of them. Input.Keys is
not the physical baseline: actual Root consumption legitimately clears those
gameplay keys while a physical trigger remains held.

Normal polls preserve every admitted button transition, releases before presses
within each row. Axes are sampled levels: only the newest row's pair is offered
once per poll, not accumulated for every queued render frame. Original ReadInput
will separately supply Hold/scaling. Empty history resamples the last admitted
normalized, gated analog level at the NEW actual CPU elapsed, with rows0 and
unchanged cursor/physical state. It is not tracking loss or a fresh hardware
row. A valid loss row replaces retained levels with0; source revocation refuses.
Lost/overwritten/rejected history requires an explicit NEW valid reset baseline.

Reset releases the OLD physical holds, then presses the newest current holds,
even when it is the SAME already-seen sample. Original ResetInput has cleared
gameplay fields, so merely comparing unchanged physical state would lose holds.
Reset accepts only the newest valid source baseline; it never claims to replay
overwritten transitions. Mapping changes require an explicit reset too.

Every dispatch uses real InputExt -> Root/current binding execution, the same
native output collector, outer VM root and cumulative work/scratch admission.
Input/Pawn/viewport/source lifetime is checked around callbacks. Physical state
and cursor are staged only after all dispatch succeeds; outer rollback restores
native payloads and ordinary script writes. The helper does not manufacture its
own transaction, swallow callback failure, fake a missing handler or mark Init
complete. Teardown clears the transient cursor/mapping/held state.

## Current verification and honest boundary

Generated reconciliation tests pass73849 checks/30 refusal controls, including
all128 button masks across valid head/focus/mount/hand gates and trigger
thresholds, Root-consumed-held simulation, same-sample reset, latest-only axes,
reserved controls, mapping changes, overflow/rejection, late callback failure,
shared work exhaustion, mid-dispatch/empty-poll lifetime revocation, mismatched
baseline/cursor, actual CPU-vs-render elapsed and rate invariance at50/72/90/
100/120Hz before PlayerInput, retained analog on faster-CPU/empty polls and its
loss/revocation/invalid-level controls. The independent
observation-source concurrency/revoke suite also passes. These generated models
are NOT actual native-owner or hardware evidence. The first direct GNU test
compile failed on misleading indentation; that test formatting was corrected
before the frozen runtime builds, without warning suppression. First-candidate
desktop/Android-native builds passed; static ReadInput inspection then exposed
the need for elapsed weighting. All jobs terminated before the correction.
Both elapsed-corrected native builds passed before the retained-level edit.
The first retained-level generated run failed because the old test expected
Empty to emit no axis events; its fixture now clears those legitimate events
before testing the next trigger transition. Failure log is retained. The final
retained-level native builds and actual runtime gates now pass, as below.

The actual source consumer is now connected to the real Init-prefix diagnostic.
Its owned-test source supplies a non-neutral tracked left stick, and the probe
requires actual current JoyX/Y bindings to change aStrafe/aBaseY to1.8/2.4,
stage cursor/mapping/normalized levels and return from the original Init body.
A second empty actual-owner poll at0.02 must retain the same physical/cursor
state and accumulate the real bound axes to5.4/7.2. The expected next
boundary is initialized-state/ReadInput PUBLICATION, not a fake initialized bit.

The actual owned Training gate passes in406.105s:266870 instructions/37976
writes,13957887/16777216 native work,19 action checks/10 earlier player stores,
28 cached INPUT properties, sourceRows1/axes2/cursor1/mapping1, actual axes
1.8/2.4, normalized levels.6/.8, emptyPollVerified=1 and initReturned=1. It ends
at the explicit `Original completed Input.Init initialized-state/ReadInput
publication is not prepared` boundary, then verifies whole save/state/revision/
hash/RNG/client/lookup/Actor/GC rollback, all45 unchanged originals and cold
immutable-Model replacement. That is EXPECTED-BOUNDARY evidence, NOT successful
or playable startup, initialized-state publication or surviving physical input.

The desktop harness now owns a genuine scoped ProcessLoopControl, bound after
lookup preparation and before its checkpoint, with Ready->Executing entry,
identity/state rollback checks and RAII revocation. Nonempty actual bindings
require that capability. It does not force a running bit or claim PC-launcher
assignment timing. The Quest app already binds its actual app-loop capability.

Final desktop/Android-native builds and strict ARM64 runtime/test syntax pass.
CTest passes260 executed/319 registered/59 explicit owned-data skips/zero
failures in185.44s. Separate current owned Root300/10, alias80/10 and viewport
Input10315/24 pass. An earlier positional-root viewport invocation failed Usage;
that failure is retained, and the corrected --game-root invocation passes.
Final audit verifies1928 frozen compiler inputs,45 originals, three assets and
four statically read native DLL sources unchanged, with zero live build/test
jobs and no new APK. Both independent read-only reviews found no blocker in
the final retained-level/ownership/scoped-loop deltas.

Production still needs initialized-state authentication, original ReadInput,
remaining startup/render/audio services, reference-space/world aim conversion,
original-frame ownership handover, committing Training frames and live
crates/loot/gravity/on-device acceptance. No new APK has been packaged.

Independent review found the honest DEFAULT DIGITAL blocker:
portable_viewport_exec_host.inl ScriptConsoleOperations::Match deliberately
refuses the missing original ScriptConsoleExec parameter/call service. Genuine
LeftMouse ParseLeftClick|Fire, RightMouse ParseRightClick and I
ShowInventoryWindow cannot yet execute through it. No binding override or
no-op replaces those commands. Current actual Training source evidence is
axes/retained-level-only; digital physical-baseline/Root-consumption remains
generated-model plus separate Root evidence until a combined owned gate runs.
