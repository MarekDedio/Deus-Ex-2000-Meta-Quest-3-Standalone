# Authored script selection and eligibility

The initial offline source batch added bounded, read-only state/function selection,
terminal label-table analysis and call eligibility to the existing synchronous
interpreter. It does not enter states, run startup, tick AI or connect animation
callbacks to the live game. Its evidence below is historical; the current
[accepted device build](QUEST-TEST-BUILD.md) and
[original dispatch increment](ORIGINAL-STATE-DISPATCH.md) have separate evidence.

The subsequent [explicit state-execution layer](STATE-EXECUTION.md) now adds
bounded transitions, mutable disabled sets, state slices and runtime-v5 saves.
The selection/layout layer itself remains read-only; no live AI tick is implied.

The initial fidelity reference was the locally pinned SurrealEngine commit
`677ee14c5b83486e6634687953779aafb7973ad6`, not the closed-source original DLL
or a later Unreal Engine version. Subsequent
[original GOTY dispatch corrections](ORIGINAL-STATE-DISPATCH.md) supersede its
Auto ordering and same-name state callback inheritance. The earlier
[metadata foundation](AUTHORED-STATE-FOUNDATION.md) remains the retention and
startup audit; this document describes the subsequent interpretation boundary.

## Graph identity and lookup

`QuestVr::ScriptDispatch::Graph` owns a validated snapshot of qualified Class,
State and Function paths. The runtime constructs it lazily from actual
UStruct.Children/UField.Next chains, rather than assuming that an export with
the right outer name is callable. The common UField prefix of `Core.Struct`,
`Core.Enum` and `Core.Const` siblings is decoded only to follow those chains;
their remaining payloads are not assigned invented execution semantics.

Names use ASCII case-insensitive identity, matching the pinned NameString
comparison table. Empty names and `None` share the null-name identity. State
names also retain their actual global `NameString.GetCompareIndex()`: a package
name index, alphabetical rank or freshly assigned graph order is not a substitute.

Current selection follows the independently audited original contract:

- Named states resolve derived class first, then parents. `None` and unresolved
  names select no state.
- `Auto` finds the first Auto-flag (`2`) candidate in actual Children/Next order,
  then superclass order. It returns that exact state path without re-resolving
  its name or falling back to a non-Auto state literally named Auto.
- Virtual/event function lookup at an exact selected state follows explicit
  SuperField ancestry, then receiver-class functions derived-to-base. It does
  not manufacture same-name state inheritance. Raw authored and explicitly
  modern runtime contexts use exact selected identity; legacy overrides retain
  the literal current-name query. Running code never masquerades as StateNode.
  See [state-control provenance and remaining authoring](STATE-CONTROL-MODEL.md).
- Global calls skip states. Final calls use the function reference from the
  executing function's own package; a fully qualified diagnostic target likewise
  selects an identity but does not bypass eligibility.

`ResolvePortableActorState` and `ResolvePortableActorFunction` return detached
path values without entering the proposed state or running its callbacks. The
latter can query a hypothetical literal current state name (including Auto),
not an Auto selection request; it is not proof that the actor has
ever occupied that state. Absent selection returns no path. Invalid receiver
metadata, ownership, references or bounded graph construction fail explicitly.
`ReadPortableRuntimeDispatchClass` exposes a detached read-only copy of actual
child order, explicit state parents and callable members for source audits.

Primary pinned references: `Packages/Core/UClass.cpp`, `UState.cpp`,
`UObject.cpp` (`GotoState`); `Package/NameString.h`; `VM/ScriptCall.cpp`
(`FindEventFunction`); `VM/ExpressionEvaluator.cpp` (virtual/global/final calls).

## Probe masks and dormant HasStack records

The 64 probe indices are the exact fixed order in pinned `VM/ScriptCall.cpp`;
for example AnimEnd is index 24 and Tick is 36. Other known events and arbitrary
names do not use compiled probe bits. Spelling and case do not create a second
probe identity.

Original IsProbing consults a frame's live positive bit; no frame permits all
probes. Nonprobe names are always eligible at this layer. Level/deletion and
other callback gates remain separate. `IsEnabled` now accepts this exact live
mask; its authored class/code-mask fallback remains only for detached callers.
Portable transitioned frames still approximate original independent live-mask
updates with per-state negative records. They are not a completed original
FStateFrame implementation. See [landing/probe corrections](SCRIPT-LANDING.md).

`ReadPortableActorDispatchContext` interprets only supported dormant headers.
When both authored function/state references exist and the logical offset is
`-1`, the portable query retains a stopped context whose code is the referenced
State or Class. The portable query validates those identities and derives its
name and masks without allocating a live frame. A class-backed Doctor record
therefore reports `Doctor`, not a guessed initial state and not `None`.
GetStateName (native 284) and IsInState (281) read this same context. Runnable
serialized continuations explicitly remain unsupported.

The HasStack field called `probeMask` is **not** compiled UState.ProbeMask and
is not an inverted disabled mask. Original Serialize retains the positive
enabled bits directly. The portable stopped context exposes those exact live
bits, derives negative names from unset bits and preserves actual Enable/Disable
overlays. The latent action number does not start a wait in the offset-`-1`
branch. Raw headers remain immutable and queryable. This supersedes the pinned
loader interpretation used by the initial batch.

This is enough to correctly suppress a manually selected callback in that
dormant context. In particular, selecting
`DeusEx.ScriptedPawn.Standing.AnimEnd` explicitly does not establish Standing
entry or override the receiver's current eligibility. Earlier execution-only
fixtures that ran this target did not prove a faithful callback; the corrected
fixture expects Nothing, zero instructions and an unchanged pose when disabled.

Primary pinned references: `Packages/Core/UObject.cpp` (`Load`, `Save`,
`IsEventEnabled`, `GetStateName`); `UObject.h` (`EnableEvent`, `DisableEvent`);
`UClass.h` (`UClass : public UState`); `VM/ScriptCall.cpp`.

## Eligibility precedes callee work

`Host::CanCall` is a read-only hook, permissive by default for existing generic
hosts. The actor host applies authored eligibility. `Machine::Run` checks it
before callee depth bookkeeping, latent-declaration rejection, argument loads,
typed-local setup, native invocation or bytecode parsing. A suppressed call
returns Nothing with no callee instructions or writes. It is not reported as an
unsupported latent/native/local-schema operation that the pinned engine would
never reach.

The actor host avoids eagerly constructing unsupported typed locals during
resolution. It always returns a temporary identity-only descriptor, retaining
native flags/index needed for lazy argument operators. `PrepareFunction` loads
the executable descriptor only after fresh eligibility; the stub is never cached.
A public root
execution still owns its normal Begin/Commit transaction even when suppressed.

Caller argument expressions retain the pinned evaluation order: they execute
on caller Self before Frame.Call, and any effects already performed by them
are not retroactively cancelled just because the callee is suppressed. The
actor context can now change during argument evaluation through Enable/Disable
or GotoState. Fresh gating and full callee preparation therefore occur after
those expressions. Synthetic state-execution controls cover disable/enable
arguments, including a target becoming eligible after early identity resolution.

`ExecutePortableActorEvent` is an explicit event wrapper, not an automatic
scheduler. In addition to eligibility it requires the receiver actor's Level
to have bBegunPlay and applies bDeleteMe. Only explicit enum dispatch grants
Destroyed's post-deletion exception; the name-dispatch path does not. A blocked
or absent event returns Nothing without entering the VM. Existing actor-host
object-lifetime restrictions are not removed by that exception, and successful
post-deletion callback execution is not claimed.

Eligibility short-circuits before reading Level. For an eligible event, the
wrapper reads the actor's exact overlay-aware reflected Level reference and
validates LevelInfo identity; a null, unresolved or wrong-class binding fails
explicitly. It never substitutes the first map LevelInfo. The pinned loaded
Actor.Level is an authored/default property: Engine.LinkActorsToLevel sets
XLevel, not Level. There is no ULevel.InitActor rebinding in this pinned source.

Primary pinned references: `VM/Frame.cpp` (`Call`);
`VM/ExpressionEvaluator.cpp` (`Call`); `VM/ScriptCall.cpp` (both `CallEvent`
overloads). Portable references: `native/quest_portable_vm.h/.cpp` and
`native/portable_unreal_runtime.h/.cpp`.

## Read-only terminal label analysis

`Vm::AnalyzeProgram` structurally parses normalized bytecode and returns
top-level statement offsets plus a terminal top-level LabelTable's entries.
It preserves authored order and duplicate labels and consumes, but omits, the
None terminator. Nonterminal or nested tables do not become state labels.
Reported targets must identify top-level statement boundaries. Identity/name
references are validated using the program's own source package.

Analysis can call only the host's identity/name resolvers. It does not begin a
transaction, read typed variables, resolve executable callees, invoke natives
or evaluate the expressions. Syntactically recognized unsupported execution
tokens can therefore be inspected without claiming that their control flow
works. `ReadPortableRuntimeStateProgram` exposes this analysis for retained
State/Class code as a detached result.

The serialized uint16 LabelTableOffset remains retained metadata. It is not
assumed to identify the LabelTable opcode: pinned `Bytecode::FindLabelIndex`
inspects the last top-level statement instead and does not consult that header.
This distinction corrected an unproven original-corpus test assumption; it is
not permission to weaken reference or target-boundary validation.

The analysis result itself is not a current PC or persistent continuation.
The separate state layer consumes validated labels for GotoLabel/GotoState,
inherited label control, committed Stop and synchronous EndState/BeginState.
Latent resumption handlers remain unfinished.
Primary pinned references: `VM/Bytecode.h/.cpp`, `VM/Frame.cpp`.

## Limits, lifetime and evidence

Graph defaults bound 65,536 classes, 131,072 states, 524,288 functions,
256-class inheritance depth, 8,192-byte identities/queries and 64 MiB of
conservatively accounted retained data. Construction rejects missing parents,
cycles, case-insensitive duplicates, ownership mismatches and inconsistent
global comparison indices. Runtime child chains additionally reject wrong
outers, cycles or more than 8,192 fields per owner. Its source-table cache admits
at most 64 packages, with a 512 MiB source-file cap per package.

Program analysis uses the existing VM limits: by default 1 MiB of normalized
code, 32,768 nodes/label entries, 64 expression levels, 8,192-byte strings,
256 arguments and 32 MiB of conservative retained layout/tree storage. This is
separate from the descriptor loader's 64 MiB payload limit; retaining a large
descriptor does not mean it is executable or analyzable under VM defaults.

The runtime graph is lazy and contains path values, not additional UObject GC
roots. Authored frame references remain outward GC links from map actors to
their code identities. Shutdown clears graph/index state; map retirement must
not leave callable aliases for removed actors. Queries use the indexed
vector-package initialization path, not the single-package lifecycle helper.
Modified map-local Class/State definitions remain outside the supported path.

Initial asset-free evidence (historical): the 30-entry host suite had 28 passing tests and
two optional original-data skips. The VM controls pass 285 checks with 66
rejections, dispatch controls 899 checks with 20 rejections, and descriptor
controls 386 checks with 101 rejections. Dispatch fixtures cover all 64 probes,
mask polarity/provenance, disabled names, event lifetime gates, inherited/state/
global lookup, the former CompareIndex ordering, invalid identities and resource budgets.
VM fixtures cover suppression and malformed/terminal label analysis.

Both separate original-data integrations passed. The graph covered 1,502
Classes, 261 States, 7,511 class functions, 697 state functions and 357 common
UField siblings. Program inspection covered all those Classes/States: 2,820
top-level statements, 355 labels and 159 terminal tables. An independent fixed-
width reader compared each terminal table's source names, order and targets;
this is layout/identity validation, not execution of all the state programs.

Doctor1, RepairBot0 and Pigeon0 retained stopped class-backed contexts and
suppressed disabled callbacks. Generated v4 overlays verified the exact
receiver-Level gate: disabled events never read a null binding, eligible null
bindings fail instead of using a map fallback, and begun-play/deletion flags
control name dispatch. Every readonly call preserved the full saved state.
Existing native helpers, 15 malformed-save controls, legacy restore, retired
map lookup and script-state travel guards also passed. The separate runtime
test passed Training/Combat replacement and paired v3/v4 save/asset-retention
checks, with synthetic progress effects rather than full campaign scripts.

Inventory and Health CPU previews using original artwork/fonts were rendered
and visually inspected. They use fixture items/sample values and do not verify
GL blending, stereo display or controller interaction. Generated evidence is
under ignored `artifacts/state-dispatch-20261008/`.

```powershell
.\desktop\build\quest_script_dispatch_test.exe
.\desktop\build\quest_portable_vm_test.exe
.\desktop\build\portable_state_descriptor_test.exe
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\portable_runtime_state_test.exe --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

Only the last two commands require the user's commercial game data. The tests
read that installation; generated fixtures/checkpoints are isolated elsewhere.
No original assets are committed or bundled in the APK.

Full startup, latent continuations, AI natives, spawned actors, timers and live
animation/event scheduling remain unfinished. Version-4 saves still cover
supported properties/clocks; the subsequent v5 model retains explicit portable
state frames, locals and state-keyed disabled sets.
Read-only selection/suppression does not change save/travel guards or establish
campaign playability, Quest controller behavior, stereo rendering or performance.
See [bounded execution](PORTABLE-SCRIPT-EXECUTION.md) and
[scoped persistence](SCRIPT_STATE_SAVE.md).
