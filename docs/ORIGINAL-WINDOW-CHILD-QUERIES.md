# Original Window child queries and client-object link (2026-10-29)

Source contract for the actual next Training startup dependency. The original
queries/client-object store are now implemented; verification below is still
in progress, not a claim of completed startup. Commercial DLLs
and EXEs are never loaded or executed; the port's package inspector and static
PE disassembler supplied the evidence.

## Original declarations and wrappers

Extension.Window GetTopChild1455, GetBottomChild1456, GetHigherSibling1457 and
GetLowerSibling1458 each have flags401/logical5: optional Bool bVisibleOnly90
and Object ReturnValue580 constrained to Extension.Window, dimension1. The
NativeParm program consumes the actual authored input declaration.

Wrappers10052D40/10052DB0/10052E20/10052E90 initialize the Bool temporary to
TRUE1, Step/copy the optional expression, consume EndFunctionParms and return
the actual Window pointer. Omitted and NoWrite input must preserve TRUE, not
the Bool declaration's false zero. Inputs are caller-frame copies, not aliases
that can be reread after later evaluation.

GetTopChild1004C6B0 starts at lastChild+140; GetBottomChild1004C740 starts at
firstChild+13C. GetLowerSibling1004C650 follows prevSibling+144; higher1004C680
follows nextSibling+148. With visibleOnly false, return that exact first link.
Otherwise skip candidates whose LOCAL bIsVisible bit0 at+30 is false. Do not
test effective ancestry, initialization, sensitivity, selectability or layout.
An empty/exhausted real chain returns None; an unavailable provider is not an
empty chain. Resource/cycle controls must fail explicitly rather than fabricate
an exhausted chain.

SetClientObject1487 is flags401/logical5, one Object newClientObject80 constrained
to Core.Object and no result. Wrapper100525E0 seeds ObjectNone0, Steps/copies,
then writes actual clientObject+118 at10052618. Native SetClientObject10003390
is also a direct pointer store. No ownership transfer, tree reparenting,
destruction, reconfiguration or callback is part of this method.
The actual clientObject field is ObjectProperty, dimension1, flags2, Core.Object
reference, base0, outer Window. NativeParm refs are313/316/319/322 for the four
queries and402 for SetClientObject; sourceText and function header base are empty.

## Actual script order

DeusEx.HUDActiveAugsBorder.ClearAugmentationDisplay84 bytes executes:

1. PC15 GetTopChild() on winIcons, with default visibleOnly TRUE.
2. While currentWindow is nonnull, PC38 Hide1414, PC50 SetClientObject(None).
3. PC69 reads currentWindow.GetLowerSibling() AFTER those operations/callbacks.
4. PC75 stores iconCount0 after the chain is exhausted.

This is a live walk, not a captured vector. Hide does not destroy, unlink or
reorder the icon. Existing LinkNewWindowChild appends at lastChild/top and
Lower1412 prepends at firstChild/bottom; actual traversal links differ from
immutable UObject Outer and parentOwner. Preserve all of these identities.

## Integration and verification requirements

Authenticate each original function/field schema and source. Queries must read
actual constructed native Window links, using the existing shared work budget.
Route SetClientObject through NativeWindowWrite/Execution.WriteReference so
Touch beforeimages and nativeWindowPropertyRoots retain/undo its exact GC edge.
Do not append a permanent reference or write an arbitrary script overlay.

VM input-copy/seeding coverage must include optional/default/NoWrite BoolTRUE
and initialized ObjectNone. Owned tests should cover top/bottom/higher/lower,
mixed local visibility beneath a hidden parent, explicit false, live reads
after Hide and None store, nonempty and empty chains, exact Object identity,
invalid receiver/declaration/input and bounded refusal/full rollback. Passing
the real empty initial HUD path alone is not proof of nonempty traversal or
completed original Training startup.

## Implementation checkpoint (2026-10-30)

The runtime now dispatches1455/1456/1457/1458 and1487 against authenticated
original declarations, NativeParm programs and scalar fields. The shared pure
QueryNativeWindowRelative helper allocates/writes nothing, reads live physical
links, visits/adopts the enclosing work budget before every receiver/candidate
access and never substitutes None for a failed provider or hidden cycle.

SetClientObject uses the existing typed Window GC-edge/journal service. VM
ordinary inputs are copied once in caller context, including native-result
aliases; initialized BoolTRUE/ObjectNone seeds and supplied EX_Nothing handling
retain original wrapper behavior. A declared direct optional call's detached
Written-Nothing trailing marker also retains TRUE. This omission case was
identified in review and fixed before freezing the build inputs.

The isolated original-data probe constructs a genuine base Window under actual
Training LevelInfo, then original NewChild children. It does not spawn a pawn,
replace a configured HUD, begin the world or fabricate native flag authority.
It checks all four directions, local visibility under a hidden parent, explicit
false, live hide/default/NoWrite/zero-fill, exact client GC edges/None clears,
original Hide callbacks followed by the live lower-sibling walk and unchanged
physical/Outer topology. Whole-root save/lookup/index/name-counter/GC/world undo,
invalid calls, late failure and shared work/write cuts are required gates.

Pure tests cover long chains beyond64, every provider failure boundary, hidden
cycles, live provider mutations, all directions and unfiltered visibility-read
avoidance. Generated VM tests cover indexed/final/coordinated entry, exactly-one
caller-frame/alias reads, NoWrite/zero/full/partial clearing and None receiver
versus None argument Context distinctions. They do not pretend to provide a
native Window graph. Strict ARM64 and Android-native builds pass against1861
frozen compiler inputs. The owned navigation fixture passes112 checks/11 refusal
controls, using1272 shared native-work units and594 writes for the full round
trip. The pure helper passes285 checks/78 refusals. The focused desktop build
has built the runtime/owned/pure targets and is still compiling the generated
VM test; actual Training continuation and full regressions are not yet verified.

The first generated-VM run fails its new final empty-receiver assertion. Source
review identifies a test mistake, not permission to alter production dispatch:
RunEvaluation's existing self.empty() gate returns NoWrite BEFORE seeds/body,
and CallFunction exposes that as Nothing while an earlier valid prefix may
commit. The test incorrectly expected an Unsupported error from a seed callback
that cannot run. After all live jobs end, correct the test to verify this
ineligible-call absence/no-read behavior and separately reject a forged NONEMPTY
receiver. Preserve the failed run as NOT passing; rerun after a fresh freeze.

The original-data discovery now returns from TravelPostAccept (previously
unreached), then stops in Engine.PlayerPawn.ClientSetMusic PC0/opcode15 at
266846 instructions/37929 writes: the VM cannot resolve the actual imported
Training_Music.Training_Music object. The original Music/Training_Music.umx
exists; actual package/asset identity must be integrated, not returned as None.
This discovery takes319.221s, exits2 and verifies full save/state/revision/hash/
RNG/client/lookup/Actor/GC rollback,42 unchanged sources and independent cold
Model ownership. PostLogin/tail return and completed startup remain unproven.

The first full desktop build passes. Its CTest run reports1 failure (the exact
new generated empty-receiver test mistake above),296 registered,52 explicit
owned-data skips,133.94s. That run is NOT a passing regression gate. No original
commercial native binary was executed; no new APK is packaged.

After correcting the generated empty-receiver expectation, the next desktop
build reports GCC13 -Werror=maybe-uninitialized on the anonymous aggregate input
inside the coordinated caller-frame test's conditional CallFunction expression
(test line5651). This build is NOT passing; VM/CTest did not run from it. Use an
explicit initialized Evaluation/vector and an explicit branch, retaining the
same one-read/alias test, rather than disable the warning. The concurrent
original Training repeat2 still owns the frozen snapshot; no compiler inputs
may change until its process is terminal. Runtime/Android sources are unchanged
between these two snapshots, whose only changed input is the VM test file.

Both expected-boundary Training repeats now terminate successfully AS BOUNDARY
DIAGNOSTICS ONLY, at the exact same ClientSetMusic PC0/object-admission error,
266846 instructions/37929 writes (286.221s and271.414368s). Both return from
TravelPostAccept, keep PostLogin/tail incomplete, and verify full rollback/
42 unchanged sources plus cold Model replacement. No processes remain from
that repeat. The test-only explicit-initialization correction is now frozen in
window-navigation-verified-frozen-20261030.json (1861 inputs). Its VM/full desktop/
CTest gates are running; native sources still match the passed ARM64/Android
and owned Window tests. The earlier failed gates remain NOT passing evidence.

The named initialized packet/explicit-branch correction now builds with the
same -Wall/-Wextra/-Werror settings. Generated VM702510 checks/1096 refusals
passes, including the new Window input1230/57 matrix. The full desktop build
also passes on this snapshot; its final CTest run is still live. No production
VM null-self behavior was changed to satisfy the corrected fixture.

Final regression gate passes244 executed/296 registered/52 explicit owned-data
skips/zero failures,108.14s. The1861 frozen compiler inputs and45 original game
files match length/UTC timestamp/SHA256, and no compiler/test job remains live.
The separately checked original Music UMX hash matches the next source contract.
All previous failure/discovery logs remain retained and are not reclassified as
passing gates. No new APK, live Quest crate/loot/gravity acceptance or completed
startup is claimed. Next actual dependency: ORIGINAL-TRAINING-MUSIC-ASSET.md.
