# First Training actor destruction integration

Source checkpoint,2026-10-21. Only `Maps/00_Training.dx` is the
actual-map target. The full port objective remains; this is not finished crate
destruction, surviving loot, gravity, startup, saves or headset acceptance.

## Authorities and implementation boundary

ORIGINAL-DESTROY-ACTOR.md records the complete original normal Level.DestroyActor
body and ORIGINAL-CONDITIONAL-DESTROY.md resolves the native teardown details.
The latter corrects two earlier assumptions: ConditionalDestroy clears the old
DebugDestroy marker, and missing Super's marker logs but still returns true.
No commercial DLL/EXE is loaded or executed; original binaries are static data.

`quest_destroy_actor.h` supplies the complete normal ordering coordinator.
Its detached test uses an independent assembly-shaped interpreter and explicitly
MOCK lower providers. It does not implement UObject/property/linker destruction.

`portable_actor_destroy.inl` binds real current-Level identities, exact qualified
flags/Role, actual state/probe reads, selected events, SetBase, native hash removal,
live ownership/Touching scans, captured-owner LostChild, a genuine saved-slot hole
and the late pending-delete store. Script279 and Spawn's veto share this service
when the actual native Engine/Level binding exists. No forced startup, fabricated
loot, early pending-delete flag or source-specific distributor exception is used.

Missing native ConditionalDestroy/property/linker teardown and deferred ownership
remain explicit refusals. The callbacks/removal prefix is not a successful full
DestroyActor. Nonnull Audio/NetConnection/editor dependencies are not replaced
with invented no-ops. Native standalone Level services establish absent network/
demo drivers; the actual constructed Engine.Audio pointer, not the separate VR
mixer or a missing script field, determines the Audio branch.

## Ownership is the original service, not a reverse child list

Original Engine SetOwner thunk10301EAB reaches10369B70..10369BFE:
capture old Owner, optionally old.LostChild(source), store RETAINED new Owner,
optionally new.GainedChild(source). No Owner reread after callbacks, same-owner
shortcut, pending-delete/Role/cycle gate or reverse child-array mutation occurs.
Primary static evidence is in ignored `artifacts/training-owner-20261021/`.

The actual runtime now uses that helper, the original Owner Object declaration
flags22/Actor constraint, and selected checked lifecycle events with exact native
packets. FindFunctionChecked validation precedes ordinary ProcessEvent suppression.
Raw native Owner/pending stores consume the same VM write journal without
restarting unrelated animation clocks. Historical wire-format child lists are
not a native ownership authority. The older metadata-only destruction path is
still not the complete native service; it no longer loops on those lists.

## Transaction and lifetime requirements

Clearing a native actor slot is different from an actor's renderer-removed flag.
The host journals the first original pointer for each replaced original Level
item; appended births still roll back by truncation. Rollback restores exact
holes/items without allocating, running scripts or compacting the Level array.
Owning journal capacity and reserve peaks are charged to the existing bound.

Pending deletion is stored only after the original saved-slot assertion/clear.
The weak retail actor mirror is updated under the existing hash root, separately
from VM reference undo. No hash callback is invoked from delayed property undo.
The VM's property/clock/callback/birth before-images and the native hash/RNG root
remain independently owned by the same caller transaction.

Real ConditionalDestroy still requires exact ConstructorLink traversal (including
PickupDistributor.NanoKeyData[8].Description), live linker-export detachment,
one authoritative ObjectFlags view and native deferred-deletion GC ownership.
An actor must retain its UObject lookup/index until actual destructor/cleanup.
ConListItem scalar retirement, erasing immutable source provenance or clearing all
reflected fields would not implement this service. Native archives/cleanup must
be implemented or explicitly refused before a completed publishing claim.

## Actual first-map observation

`portable_training_startup_probe --owned-source-root <owned-root>
--native-work 1048576 --repeat 2 --require-spawn-encroachment` now reaches
the explicit `Original DestroyActor ConditionalDestroy/property/linker teardown
is not implemented` boundary. Both attempts stop in actual
`00_Training.PickupDistributor0`, PostPostBeginPlay slot1195,
`DeusEx.PickupDistributor.PostPostBeginPlay` PC244/opcode97. Each executes
533311 instructions and39740 VM writes, uses448186/1048576 shared native work,
and restores the full save/state/revision/hash/RNG/client/lookup/Actor/GC state.
The actual saved actor slot and late pending-delete store are rolled back too.
All26 Spawn observations repeat exactly, including the real NanoKey's completed
collision/state/base/PPP path. All40 owned source fingerprints remain unchanged.

This is exit2: an observed unsupported startup boundary with verified rollback,
NOT completed or published startup. No other actual map is loaded. The log is
`artifacts/training-destroy-runtime-startup-final-20261021.log` (ignored evidence).

Independent detached original-order controls pass17096 checks/1754 cases,
including318 refusals,167 callback-reentrance controls,105 provider-failure
prefixes and106 borrowed-work prefixes. SetOwner controls pass3186 checks/
403 cases/33 refusals. These use MOCK lower services; they do not prove native
property/linker cleanup or playable crates. Strict ARM64 checks and desktop/
Android native builds are separate from headset acceptance. No backend-only
replacement APK is packaged or installed for this checkpoint.

Focused actual first-map movement/clock/publication controls pass122706 checks/
21 refusals. First-map native conversation binding passes156871/17 with all39
sources unchanged. The isolated original crate regression remains347 checks/
two contents refusals: its explicitly dormant `Game=None` fixture still removes
the spawned Lockpick through original relevance, so this is NOT surviving loot.
Logs share the `artifacts/training-destroy-runtime-*-20261021.log` prefix.

Generated runtime lifecycle controls pass1976 checks/25 rejections and Spawn
controls pass9919/164. Owner coverage includes old/new callback visibility,
retained-NewOwner behavior after LostChild mutation, synchronous GainedChild
reentrancy, self/cyclic/static ownership without invented native guards,
begun/probe suppression, caller write/depth rollback and unchanged reverse lists.
Historical serialized child lists retain independent65,536-entry archive
round-trip coverage; they are never reconstructed by native SetOwner.

The coherent full desktop build passes. Final shared CTest executes161 of186
registered tests with25 explicit owned-data skips and zero failures in49.17s.
The first run identified two incomplete generated Owner schemas in Timer and
BasedActors fixtures; both now carry original Owner/child-event declarations,
with their existing assertions preserved. Timer controls pass1469/47. Strict
ARM64 syntax passes the runtime and all six changed/new test inputs; Android
native compilation/link passes. Those desktop skips are not real-map or headset
evidence. Actual first-map tests above run separately with explicit owned data.
