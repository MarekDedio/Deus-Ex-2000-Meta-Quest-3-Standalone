# Original actor lookup and synchronous iterators

The portable VM executes original foreach bytecode and Actor.AllActors304 over
the current native Level slots. Actor.GetPlayerPawn720 implements the pinned
current-Level fallback. These are startup dependencies, not an owned player
session, automatic world startup, working AI or campaign completion. The
accepted seated-player Quest build remains installed unchanged.

## Iterator execution and aliases

Iterator `0x2f` creates and pushes a cursor before its first Next. A successful
Next enters the loop body; exhaustion jumps to the actual IteratorPop `0x30`.
IteratorNext `0x31` advances the top cursor, and only Pop removes it. Nested
cursors advance independently. A function Return destroys its local cursor
stack without clearing the current OUT value. Malformed targets and stack
underflow fail explicitly.

Native-index and resolved virtual/global/final/context factories are supported.
Resolved declarations must be synchronous native iterator functions; preparation
and eligibility follow argument evaluation. Arguments evaluate on caller Self,
not the context receiver. OUT references use the existing guarded VM aliases,
write accounting and transaction journal. Local, property, fixed-array element,
struct-member and context-property aliases retain their actual storage identity.
They cannot be used after the enclosing Machine is destroyed.

Cursor stacks, retained storage, instructions and reference writes share the
existing invocation budgets. AllActors scans also use the actor host's aggregate
65,536-step native lifecycle/work bound. Rejected scans roll back all properties,
OUT aliases, state frames, provisional births and native relationships; they do
not publish a partial world revision.

Synchronous loops can execute completely within one state slice. Persistent
iterator storage is not implemented: Stop/Return, latent continuation, null code
or changed code/local storage with a live cursor fail transactionally rather
than silently discarding it. No iterator is serialized in a checkpoint. This is
not support for arbitrary latent AI loops.

## AllActors304 membership

The input must identify a real loaded UClass, not an actor instance or a guessed
class name. Membership follows the pin's `IsA(BaseClass->Name)` comparison along
actual class metadata; it is not qualified-path ancestry. Consequently a valid
non-Actor UClass with the same declaration name can match an actor ancestry.

Each Next resamples the current Level size and scans forward in actual numeric
slot order. Null holes and removed actors are skipped. Births appended by a
previous loop body can be visited by the next call. MatchTag is optional and
case-insensitive, and reads each candidate's current Tag. A direct Tag write is
visible immediately; no stale tag index drives this scan.

Hidden, presentation-inactive, collision-disabled and directly bDeleteMe-marked
actors are still members until their native world slot is removed. Destroyed
callbacks can therefore see their own receiver before Destroy clears the slot.
Exhaustion writes Object None to the guarded OUT binding. Persistent reflected
object bindings enforce their declared class constraint; function-local object
storage currently checks Value kind rather than UClass ancestry, a pre-existing
VM limitation, not an additional validated constraint.
Inactive members exposed an older live-write inconsistency: assignment rejected
their valid UObject identity even though save restoration accepted it. The
activity-only reference-value check is removed; identity and class constraints,
including recursive struct members, remain. Direct inactive receiver execution
and read guards are unchanged. Regression controls cover ordinary scalar/array/
struct assignments, typed iterator OUT, wrong/missing identities, cold saves,
GC and the unchanged receiver guards.
Consequently an original loop body that dereferences an inactive candidate's
properties can still reject at the Context receiver guard. Accepting and storing
the reference does not claim complete inactive-target script execution. The
original nearest-search oracle uses an unchanged dormant, active map baseline.

## GetPlayerPawn720 and remaining possession work

Without an owned native viewport, lookup returns the first current-Level
PlayerPawn with a nonnull typed Player property, or Object None. It does not use
bIsPlayer, UI activity, proximity, the last spawned pawn or a fabricated
PlayerStart/NPC. Deleted UObjects retain their fields but no longer qualify
after world removal. The pinned viewport-owned Actor precedence still requires
a real native player session and is not claimed by this fallback.

Full possession must allocate/root the native Viewport and establish both
Viewport.Actor and PlayerPawn.Player before the actual most-derived Possess
script. Original Extension.PlayerPawnExt.Possess requires InitRootWindow1052;
skipping that call or substituting a base-only Possess would not provide the
authored UI. Fresh login and saved-game possession have different lifecycle and
inventory requirements, and neither is implemented by actor lookup alone.

## Original helpers and fidelity reference

Native221 implements writable Vector-times-Float assignment, and native225 VSize
uses the pinned binary32 vector length. These let actual FindTaggedActor execute
AllActors and nearest-distance selection, and InitializeHomeBase scale HomeRot.
The random-selection branch still requires RNG support and is not covered by
nonrandom lookup tests.

Actual StartUp now reaches native711 AIClearEventCallback in
ScriptedPawn.UpdateReactionCallbacks at PC41/opcode0x62. It fails explicitly
with complete rollback; the pinned AI methods are stubs, not a justification
for successful no-ops. The original native-manager characterization in
[AI-NATIVE-AUDIT.md](AI-NATIVE-AUDIT.md) describes the required deferred
registrations, histories, dispatch and unresolved scheduling/perception work.

The generated package integration exercises original-shaped iterator bytecode,
Level holes/order, all alias types, nested loops/callbacks, live births/deletions/
Tags, cold restores and failure/budget rollback. The separate original-data
actor test compares unchanged FindTaggedActor against an independent oracle
built from serialized Level order and actual Tag/Location properties. It also
checks absent-player None, case/class/empty controls, byte-identical read-only
saves and the actual InitializeHomeBase Return. Results are recorded in
[STATUS.md](STATUS.md).
The oracle filters recognized members through runtime snapshots; its ordering
and nearest-selection calculation are independent of the iterator/tag index,
not an independent validation of all Level publication membership. Generated
slot fixtures test that membership separately.

```powershell
.\artifacts\prerequisites-20261009\build\quest_portable_vm_test.exe
.\artifacts\prerequisites-20261009\build\portable_actor_spawn_test.exe
.\artifacts\prerequisites-20261009\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
```

Fidelity source: locally pinned SurrealEngine revision
`677ee14c5b83486e6634687953779aafb7973ad6`, especially Native/NActor.cpp,
Native/NObject.cpp, VM/Iterator.cpp, VM/Frame.cpp and Engine.cpp. Original helper
bytecode and metadata come from the user's read-only GOTY installation. This
is not verification against the closed-source original native DLL.
