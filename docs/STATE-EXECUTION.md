# Explicit persistent actor-state execution

The portable runtime now has an explicit bounded state-slice API, transactional
GotoState/Enable/Disable, and runtime-v5 state-frame persistence. This is offline
source work, not automatic level startup, working NPC AI, a campaign playthrough,
or a replacement for the installed, accepted seated-player Quest test APK.

The original [Sleep continuation increment](SCRIPT-SLEEP.md) supersedes the
earlier unsupported-Sleep and latent-preserving Goto behavior described below.
It adds committed Waiting, explicit polling and codec8/envelope11 native timers.
It cancels waits on supported transitions/labels according to the original DLL;
remaining missing-label/selected-StateNode approximations are stated there.

The initial state-control contract follows the locally pinned SurrealEngine commit
`677ee14c5b83486e6634687953779aafb7973ad6`, especially `VM/Frame.cpp`,
`Packages/Core/UObject.cpp`, and `Native/NObject.cpp`. It is not verification against
the closed-source original engine DLL.

## State control and callbacks

Native GotoState (113) uses actual named/Auto selection from the authored graph.
Omitted state means the current state name; omitted/None transition label means
Begin. EndState runs synchronously when the captured old code differs from the
selection, then state replacement/label positioning, then BeginState. These
callbacks share the caller's interpreter, transaction, instruction/write/retained
budgets and call depth. They retain the receiver's exact Level/begun-play/deletion
and event-eligibility gates.

The pinned outer transition still replaces state after a reentrant EndState;
a reentrant BeginState selection remains in effect. No generation-abort policy
is substituted for that behavior. Same-code relabeling does not call entry or
exit callbacks. SetState replaces local storage only when the captured old code
differs; it does not reset the PC. Supported state changes now clear latent
status before callbacks. Selecting no state clears code/local storage while
retaining PC, the native actor timer and the HasStack flag.

Transition-label lookup starts from receiver-derived same-named states and walks
parent classes. An in-code goto tries current code first, then that hierarchy.
Only terminal top-level LabelTable entries are used, with first matching authored
entry and validated statement-boundary targets. A transition-label miss sets
Stop and retains PC; an in-code miss is a transactional failure.

Enable (117) and Disable (118) maintain separate case-insensitive sets keyed by
current state name, including None/class-backed dormant names. Sets survive
transitions. Restored names are folded before mutation. Callable resolution
retains identity/native flags only; full typed function preparation follows
caller argument evaluation and a fresh eligibility check, so an argument that
enables a callee does not leave it stuck with an earlier suppressed stub.

## Persistent slice and local storage

`ResumePortableActorState` invokes `QuestVr::Vm::ResumeState` explicitly. No tick,
time advance, physics, world initialization or animation scheduling is implied.
`ReadPortableActorStateObject` returns a detached copy of the portable override;
the original serialized HasStack record remains immutable and separately readable.

Each frame stores qualified running-code and local-owner paths, the next top-level
statement ordinal, latent status and typed fixed-array locals. An inherited label
can change running code without recreating locals; their declaration owner remains
distinct. Property references resolve against the executing code's own package.
The next PC advances before expression evaluation, and execution re-reads the
current frame afterward. Old statement Jump/GotoLabel/Stop/Return results apply
to the newly selected code, matching the pinned control path.

A state Stop commits the slice and returns `Status::Stopped` with `committed=true`.
Ordinary function Stop remains a rollback failure. State Return commits without
changing Continue, including the next ordinal exactly at code end; its next resume
fails explicitly instead of inventing Stop. A stopped/null-code frame can retain
a stale ordinal. Unsupported waits and required native/opcode behavior fail and
roll back the entire slice, including callbacks, properties, clocks, disabled sets,
PC, code/local ownership and local values.

State views include a nonserialized local-storage revision so A-to-B-to-A
replacement does not reuse stale references. This discriminator is not a
transition-generation abort. A local alias still used by an old expression after
its storage is destroyed fails explicitly; recreating guessed local values or
writing through the alias into new storage is not supported.

Synchronous foreach loops now run within a slice, using the shared
Iterator/IteratorNext/IteratorPop interpreter. A slice cannot retain a live
iterator across Stop/Return, latent continuation, null code or replacement of
code/local storage: these paths fail and roll back explicitly until persistent
iterator storage exists. Completed loops can commit their normal state-local
and property writes. See [actor lookup](ACTOR-LOOKUP.md).

Switch/Case selection now follows the live state frame after selector and
Case-expression callbacks. Matching preserves a callback-selected position;
mismatching resolves the old label's absolute next offset in current code.
Same-state/inherited-label changes retain local storage; state replacement,
including A-to-B-to-A, invalidates old aliases. Live-iterator replacement is
refused before searching a new Case list. See [Switch/Case](SWITCH-CASE.md).

## Narrow physics-mode native

SetPhysics (3970) writes the actor's reflected scalar Physics byte inside the
same transaction. This matches `Packages/Engine/Actors/UActor_Phys.cpp` and
`Native/NActor.cpp` at the pin: the optional Deus Ex floor argument is unused
there. This implementation does not change Base or Velocity, start movement,
or supply walking/falling collision. Unsupported argument types fail before
writing. Property-only saves remain v4; changing a physics mode alone does not
invent a state frame or clock.

## Persistence and bounds

Runtime v5 appends codec-v2 state data to the unchanged v3 gameplay prefix.
Properties/clocks without state still use byte-compatible runtime v4/codec v1;
untouched runtimes still use v3. See [script-state saves](SCRIPT_STATE_SAVE.md).
Pure dynamic-disabled records can preserve dormant raw context without inventing
native class locals. Actual state entry materializes the supported typed state
schema. Runnable raw serialized continuations and map-local script definitions
remain unsupported.

Save preparation validates original map actor/class ownership, state definitions
within receiver ancestry, same-named code/local owner, typed local identities and
dimensions, HasStack, supported latent states and PC. Continue admits ordinals up
to code end; Stop retains stale PC. Current runtime restore supports Continue/Stop
and Sleep with its native timer, not the other latent handlers, although the structural codec represents all twelve
pinned latent ordinals. Load never reruns startup. Legacy v1-v3 restoration clears
portable overrides as well as properties/clocks.

The existing VM defaults include 100,000 instructions, depth 32, 16,384 local
elements, 16,384 writes, 8,192-byte strings and 32 MiB retained data per invocation.
The write counter covers interpreter/out-reference writes, not every host-side
native mutation; those still share the host journal and invocation boundaries.
State schemas and child/base chains have additional ownership/count/cycle bounds.
Persistent state objects are also cumulatively measured without copying using
the codec's conservative accounting with the runtime's 16 MiB cap and aggregate
count limits before transactions,
after allocating state mutations and at commit. This state-only bound is separate
from the combined save capture/envelope budget. Dynamic disabled sets additionally
cap each state's names at 4,096. Budget rejection rolls back; it is not truncation.

## Verification commands and remaining work

```powershell
.\desktop\build\quest_state_execution_test.exe
.\desktop\build\quest_state_frame_codec_test.exe
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\portable_runtime_state_test.exe --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

The first two tests are asset-free synthetic control/codec tests. The last two
read the user's original installation and generate isolated temporary saves.

The asset-free controls pass 75 checks/16 rejections for state execution and
1,251/621 for the structural state codec. The 32-entry host suite has 30 ordinary
passes and two original-data integrations skipped unless their root is supplied.
The separately run final-source runtime integration passes state-only v5 and
paired v3/v5 composition, gameplay semantics, exact canonical script blob,
read-only wrong-map rejection, legacy reset and guarded travel. Its legacy
gameplay prefix retains unordered collection order and is not byte-canonical.

The separate final-source original actor integration also completed exit 0.
The following results describe the state-execution batch before
[authored struct support](AUTHORED-STRUCTS.md); its InventoryItem refusal is a
historical boundary, not the newer struct decoder's verification result.
It checks all 1,502 Classes/261 States and terminal labels, existing helpers and
reference/Level gates, Physics-only v4/legacy restoration, state-only and selected
v5 frame roundtrips, twelve malformed-state controls, stopped/end-ordinal and
mixed-case Enable behavior. Actual StartUp slicing refuses the unsupported
InventoryItem struct at InitializeInventory PC 42; actual begun-play BeginState
refuses required native AIEndEvent715 at SetDistress PC 46. Both preserve the
entire gameplay/property/clock/state payload on failure. This tests atomic
refusal, not working inventory initialization or AI.

An actual 512x512 desktop CPU close-up of `00_Training.Doctor1` after compiled
`Engine.Actor.SetInitialState` was generated and visually inspected. The helper
ran nine instructions, and the textured mesh has 603 triangles with no missing
Doctor materials or cube placeholder. The pose remains the static `All` fallback,
not a ticked idle animation. The capture is albedo-only without world BSP; it
verifies neither lighting nor Quest GL/stereo/controllers. Evidence is in ignored
`artifacts/state-execution-20261008/doctor-prestartup.bmp` and its JSON/log.

Pre-begun-play SetInitialState can select actual StartUp/Begin while entry callbacks
are gated; that is not evidence that world startup or an NPC initialization ran.
The actual begun-play StartUp entry and executable state slice still reach missing
required behavior and must refuse atomically.

Full level-wide startup, remaining FinishAnim/movement handlers, world Sleep
scheduling, AI/physics
natives, remaining iterators/timers/actor behavior, live animation/event boundaries,
per-map archives and campaign progression remain unfinished. Current-map state
presence still guards travel/unload, even after saving. Engine-global
[random state](SCRIPT-RANDOM.md) now persists independently and alone does not
activate that per-map guard. No new GL/stereo/controller
or device performance verification is established by this source batch.
The separate [original AI-native audit](AI-NATIVE-AUDIT.md) records why pinned
log stubs and a null-manager shortcut cannot establish faithful AI startup.
