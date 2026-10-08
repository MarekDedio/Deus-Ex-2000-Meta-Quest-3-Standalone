# Authored state metadata foundation

This document records the metadata-retention step, not automatic actor startup
or working AI. Subsequent [script selection and eligibility](SCRIPT-DISPATCH.md)
now interprets supported stopped headers for read-only lookup and suppression.
The portable runtime now retains named-state descriptors, the state header of
serialized classes, and the exact loaded HasStack header of map actors. None
of these fields is interpreted as a live state frame, automatic callback,
resumable latent call, or completed lifecycle phase.

The fidelity reference is the locally pinned SurrealEngine commit
`677ee14c5b83486e6634687953779aafb7973ad6`. The contracts below describe that
implementation, not a verification of the original closed-source engine DLL
and not later Unreal Engine lifecycle conventions.

## What is retained

`PortableStateDescriptor` preserves UField/UStruct/UState fields: base/next,
script text and children references, friendly name, line/text position, logical
script size, raw and normalized script bytes, ProbeMask, IgnoreMask, label-table
offset and state flags. Actual `Core.State` exports use
`LoadPortableStateDescriptor`; classes retain the same header in
`PortableClassDescriptor.state`. The authored uint16 label offset, including
`0xffff`, and unknown flag bits are retained rather than assigned guessed
execution semantics. The legacy class `stateBytecode` view remains available.

Map actors retain `PortableObjectStack`: function reference, state reference,
uint64 mask, uint32 latent action and an offset only when the function reference
is nonzero. References remain indices into the original source package. The
runtime adds resolved metadata links for GC; it does not create a state frame.
The subsequent dispatch layer interprets supported dormant headers and restores
their disabled probe names for read-only eligibility, without entering a state.

The read-only queries are `ReadPortableRuntimeAuthoredStateDescriptor` and
`ReadPortableActorSerializedStack`. They return copies and use case-insensitive
runtime object lookup. Their supported indexed path is the vector-package
`InitializePortableRuntime` overload, followed by map loading for actor stacks.
The single-package lifecycle helper is outside this indexed query path. Map
loading currently retains actor stacks but does not populate descriptors for
map-local Class/State exports. The separate audit found zero such exports in
all 88 original maps; modified maps containing them are outside this scope.

State/Class decoding checks export offsets against the source before allocation,
limits each payload and logical script to 64 MiB, rejects token nesting at depth
64, and bounds identity outer chains to 32 nodes and 64 KiB before concatenation.
Names/references and exact payload termination are validated. Subsequent typed
graph selection and terminal label analysis are described in
[SCRIPT-DISPATCH.md](SCRIPT-DISPATCH.md); label execution and live-state
validation remain separate work.

Primary implementation references:

- `native/surreal_portable_package_tables.h`: `PortableStateDescriptor`,
  `PortableClassDescriptor`, `PortableObjectStack`.
- `native/surreal_portable_package_tables.cpp`: `ReadStateHeader`,
  `LoadPortableStateDescriptor`, `LoadPortableClassDescriptor`,
  `LoadPortableExportProperties`.
- `native/portable_unreal_runtime.cpp`: `RuntimeObject`, `PopulateRuntime`,
  vector-package `InitializePortableRuntime`, `LoadPortableRuntimeMap` and the
  two authored-metadata queries.

## HasStack is not a startup command

The pinned loader reads the function/state references, mask, latent action and
optional offset before actor properties. If both referenced objects exist and
the offset is not -1, it looks up a statement and restores a latent status. With
offset -1 it installs a stopped frame from the state reference instead; the
latent number does not select a runnable wait in this branch.

`UClass` derives from `UState`. A class-backed offset -1 header can therefore
produce a stopped Class-backed frame in the pinned loader, and its
`GetStateName` returns that code object's class name. A future faithful
bootstrap must distinguish this from a genuine named-State continuation;
neither resuming it as AI nor unconditionally normalizing its name to None is
justified by the pinned code.

The header's field called `probeMask` is also distinct from compiled
`UState.ProbeMask`: pinned loading restores its set bits through `DisableEvent`.
Pinned package saving writes the currently selected state's dynamic disabled
probe bits into this field. The original foundation only retained those bytes;
the subsequent stopped-context query interprets them as disabled probe names,
not permission to run events.

Primary references under `third_party/SurrealEngine/SurrealEngine/`:

- `Packages/Core/UObject.cpp`, `Load` (58-120), `Save` (135-175),
  `GetStateName` (484-486).
- `Packages/Core/UClass.h`, `UClass : public UState` (37).
- `Packages/Core/UState.cpp`, serialized compiled masks/flags (6-20).

## Startup is a level-wide sequence

For a fresh level, the pinned engine sets TimeSeconds to zero and bBegunPlay to
true, freezes the loaded actor count, sets bStartup, and calls GameInfo.InitGame.
It then performs separate actor-array passes:

1. PreBeginPlay for all surviving loaded actors.
2. BeginPlay for all surviving loaded actors.
3. PostBeginPlay for all surviving loaded actors.
4. SetInitialState for all surviving loaded actors.
5. Deus Ex PostPostBeginPlay for all surviving loaded actors.
6. Native InitBase, then clear bStartup.

The count is captured before InitGame; actors appended by spawning are outside
these loaded-actor passes. Startup is conditional on bBegunPlay initially being
false. Calling each actor's entire sequence in turn, or calling SetInitialState
alone, is not this ordering.

Spawn during begun play has its own sequence: Spawned, PreBeginPlay, BeginPlay,
deletion check, InitActorZone, PostBeginPlay, SetInitialState, Deus Ex
PostPostBeginPlay, InitBase, then spawn notifications. State entry positions
code but does not immediately tick the state block.

Primary references: `Engine.cpp` (730-760) and
`Packages/Engine/Actors/UActor.cpp`, `Spawn` (67-108).

## Reached original startup dependencies

The existing `script_bytecode_inspect` read original Engine.u/DeusEx.u functions
from the owned installation. The following are compiled-path audit findings,
not newly implemented natives or evidence that automatic startup has run.
The base Actor.BeginPlay and Actor.PostBeginPlay bodies are each just Return
Nothing; their emptiness must not be confused with unsupported NPC overrides.

- ScriptedPawn.PreBeginPlay calls Pawn.PreBeginPlay, whose first operation is
  native 529 AddPawn, before Actor.PreBeginPlay. The latter requires native 283
  SetCollisionSize for pawns; its relevance path can call GameInfo.IsRelevant
  and native 279 Destroy. Pawn initialization also reaches missing Tan (189),
  FRand (195), FClamp (246), and conditionally Rand (167), Spawn (278) and the
  unsupported ObjectToString conversion (0x56).
- ScriptedPawn then calls CreateShadow (conditional Spawn), SetAlliance
  (scripted assignment), and UpdateReactionCallbacks. The last chooses either
  AISetEventCallback (710) or AIClearEventCallback (711) for every reaction.
  Its PostBeginPlay style branch needs SetCollision (262). SetInitialState
  needs GotoState (113), and PostPostBeginPlay calls ConBindEvents (2102).
- StartUp.BeginState calls SetMovementPhysics (SetPhysics, 3970),
  SetDistress(false) (AIEndEvent, 715), and reaction setup. Standing.BeginState
  disables AnimEnd through native 118; explicitly calling Standing.AnimEnd or
  PlayWaiting does not establish genuine state entry or event eligibility.
- SetOrders begins with unsupported Switch/Case (0x05/0x0a), with DynamicCast
  (0x2e) on a hostile-order branch. FindTaggedActor uses Iterator/IteratorNext/
  IteratorPop (0x2f/0x31/0x30), native AllActors (304), VSize (225), optional
  FRand, and GetPlayerPawn (720) for a None tag. Parsing these tokens is not
  equivalent to executing their control flow or maintaining iterator frames.

The pinned native implementations show why property-only approximations are
insufficient: AddPawn prepends self to Level.PawnList and links nextPawn.
Collision size/flag changes remove and reinsert collision membership; omitted
SetCollision arguments preserve existing flags. SetCollisionSize's pinned
always-true return has an explicit room-check TODO, not proof of original-DLL
placement semantics. Spawn requires real class-default actor allocation,
level/owner/collision membership and its nested lifecycle. Destroy enforces
bStatic/bNoDelete, sets bDeleteMe, sends destruction/untouch/ownership events,
detaches relationships and clears the actor slot without immediate deallocation.
ConBindEvents matches mission conversation owners against BindName and
BarkBindName, creates transient ConSys.ConListItem links and sets ConListItems.

AISetEventCallback, AIClearEventCallback and AIEndEvent are especially important
limits of this reference: their pinned bodies are only LogUnimplemented stubs.
They do not specify callback registration, filtering, scoring, clearing or AI
event delivery. Copying those stubs as successful no-ops cannot establish
faithful NPC reactions; additional authoritative evidence and real semantics
are required before claiming support. Current missing behavior remains an
explicit transactional failure.

Primary pinned references: `Native/NPawn.cpp` (73-78);
`Packages/Engine/Actors/UActor_Phys.cpp` (46-53, 131-143);
`Packages/Engine/Actors/UActor.cpp`, Spawn/Destroy (18-167), AI stubs (358-384);
`Native/NActor.cpp` (817-844); `Native/NScriptedPawn.cpp` (23-27);
`Packages/Engine/Actors/UActor_Conversation.cpp` (11-57).

## Event existence is not event eligibility

Pinned event dispatch first checks IsEventEnabled, then requires the actor's
level to have begun play and suppresses deleted actors. The enum-event overload
permits Destroyed after deletion; the arbitrary-name overload has no such
exception. An absent event function produces Nothing rather than an error.

For probe event indices below 64, the exact eligibility rule is:

- If a frame has a code object, its IgnoreMask bit must be 1. A zero bit denies
  the probe, despite the field's name.
- The current code object's ProbeMask bit or the receiver Class.ProbeMask bit
  must be set. This check does not walk class ancestors to reconstruct masks.
- The event must not be dynamically disabled for the current state name.

Known non-probe events and arbitrary names use the dynamic disabled-name check
without the compiled probe-mask test. Enable/Disable modifies sets keyed by
current state name, and switching states does not clear those sets. Frame.Call
also checks eligibility for ordinary function calls; the level/deletion gates
are additional behavior of CallEvent, not of every function call.

Event and virtual lookup searches same-named states through the receiver class
hierarchy first, then class functions. Global bytecode calls skip states; Final
calls use their referenced function. The running code can be a parent state's
block while event lookup still begins with the derived same-named state.

Primary references:

- `Packages/Core/UObject.cpp`, `IsEventEnabled` (370-420).
- `Packages/Core/UObject.h`, `EnableEvent`/`DisableEvent` (316-336).
- `VM/ScriptCall.cpp`, `CallEvent` and `FindEventFunction` (68-136).
- `VM/ScriptCall.h`, the fixed probe/event index order (8-35).
- `VM/Frame.cpp`, `Call` (202-216).
- `VM/ExpressionEvaluator.cpp`, virtual/final/global calls (617-679).

## State transitions and execution

Pinned GotoState resolves Auto from the first Auto-flag state encountered in
the derived class's state map, then parent maps. Named resolution is likewise
derived-class-first. None or an unresolved name selects null state. Omitted
native arguments mean current state name and an empty label.

For a changed nonnull old state, GotoState synchronously calls EndState, changes
the frame's code object/local storage, then positions the new label. Empty/None
label means Begin. A nonnull new state sets HasStack. A changed nonnull new
state then receives synchronous BeginState. Same-state transitions reposition
code without EndState/BeginState. Clearing the state retains a frame with null
code; SetState does not independently reset PC or latent status.

GotoLabel searches same-named states derived-to-base and may select parent
code. Success sets PC and Continue; a missing label sets Stop. An in-code goto
has its own inherited-label fallback and throws when no label is found. These
two failure behaviors must not be conflated.

The persistent state frame increments PC before evaluating a statement, since
nested GotoState may replace both code and PC. Stop sets latent Stop; execution
returns when that state frame becomes latent. Ordinary function calls remain
synchronous frames. UActor.Tick advances animation first, then script Tick,
latent waits and state execution, then physics and timers. Tick and state code
have Role >= SimulatedProxy gates. Level ticking handles owner-first order,
deleted actors and once-per-tick bookkeeping.

Primary references:

- `Packages/Core/UObject.cpp`, `GotoState` (489-546).
- `Native/NObject.cpp`, native registration and `GotoState` (907-909).
- `VM/Frame.cpp`, `SetState` (30-34), `GotoLabel` (433-450),
  `Run` (482-607).
- `VM/Bytecode.h`, statement/offset/label mapping (15-42).
- `Packages/Engine/Actors/UActor.cpp`, `Tick` (169-217).
- `Packages/Engine/Resources/Level/ULevel.cpp`, `TickActor`/`Tick` (58-118).

## Priorities before automatic gameplay

1. Add a persistent state-frame model and mutable event eligibility.
   Preserve code identity, next logical statement offset, latent status,
   owned local storage and state-keyed disabled names. Retaining metadata is
   necessary but does not provide these runtime semantics. Read-only authored
   eligibility/resolution now exists, without state entry or ticking.
2. Add VM-internal synchronous EndState/BeginState dispatch within the current
   transaction. Public Execute cannot simply be called recursively from a host
   native: it owns Begin/Commit/Rollback, and the actor host rejects nested host
   transactions. Delaying these events until after the caller returns would
   also change authored behavior.
3. Extend transactional journals, state-presence guards and persistence before
   committing state-only mutations. Current journals/save records cover actor
   overlays and animation clocks, not state frames or disabled-event sets.
   Validation must bind code/state identities and offsets to original schemas,
   and restoration must not rerun startup for an already-started level.
4. Stage genuine level startup only after GameInfo/player/spawn identities,
   actor-list membership, zones/bases and reached authored operations are
   supported. Do not leave a partially initialized world after a later actor or
   phase fails. A staged-world transaction or explicitly persisted resumable
   phase policy is a portable safety requirement, not a pinned-engine feature.
5. Connect bounded state/latent slices and animation/event boundaries to the
   live simulation, with synchronous callbacks and fresh state reads. Add
   timers, RNG, spawned actors and per-map campaign archiving as required by
   reached behavior, without treating unavailable natives as successful no-ops.

Current blockers remain concrete: the VM executes immutable synchronous
Function frames, rejects latent declarations, treats state Stop as an
unsupported continuation, and exposes label entries only as read-only structural
analysis, not an executable state program. Authored state flags do not remove
these limitations. The v4
save codec stores properties/clocks only; legacy restoration resets supported
live overlays/clocks while authored metadata remains immutable. Map travel
still requires a per-map archive when committed script state exists.

Primary portable references: `native/quest_portable_vm.cpp`, Machine.Run,
Parser and Execute; `native/portable_unreal_runtime.cpp`,
PortableActorVmHost.Begin/Rollback/Touch and GetPortableRuntimeScriptStatePresent;
`native/quest_script_state.h`, ScriptSavedObject/ScriptSavedState. See also
[bounded script execution](PORTABLE-SCRIPT-EXECUTION.md) and
[scoped saves](SCRIPT_STATE_SAVE.md).

## Evidence boundary

The asset-free descriptor test passed 365 checks, including 97 rejection
controls. The separate read-only original audit passed across 38 System
packages: 261 actual States and 1,502 serialized Classes, with no empty Class
exports. All States and 47 Classes contained script. Their combined retained
script spans contain 23,037 raw bytes and 30,167 normalized bytes; the largest
logical script is 1,356 bytes. There are 102 State and 1,502 Class `0xffff` label
sentinels, and zero other label offsets outside their declared script spans.
The all-map table audit found zero Class/actual State exports in 88 maps.

Original header/reference/mask/flag fields and raw spans are compared with a
separate serialized-field reader. Normalized size is checked across the corpus;
exact normalized operand content has synthetic fixtures, not an independent
full-corpus disassembler. These are retention checks, not execution validation.

```powershell
.\desktop\build\portable_state_descriptor_test.exe
.\desktop\build\portable_state_descriptor_test.exe --audit-original 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\portable_runtime_state_test.exe --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

Only the first command needs no commercial assets. The audit reads the owned
installation; synthetic fixtures/checkpoints go to isolated temporary folders,
not the original game directory. Generated logs live in
`artifacts/state-metadata-20261008/` and are ignored by Git.

The original-data integration test passed checks for exact State/Class
metadata and raw actor stacks, case-insensitive lookup, wrong identity
rejection, legacy restore preservation, rejected startup preservation and
retired-map lookup. The descriptor tests exercise malformed payloads as well
as authored metadata. The separate original runtime/map/save regression also
passed; see [status](STATUS.md). Those original retention checks do not by
themselves establish callback eligibility; the separate later dispatch controls
are documented in [SCRIPT-DISPATCH.md](SCRIPT-DISPATCH.md). Neither establishes NPC movement,
automatic startup, headset rendering or full campaign completion.

This is subsequent offline source work, not a replacement for the installed
`25e38b3` test APK. No Android build or headset deployment was performed for this
metadata batch; the user's frozen build/data/saves remain untouched.
