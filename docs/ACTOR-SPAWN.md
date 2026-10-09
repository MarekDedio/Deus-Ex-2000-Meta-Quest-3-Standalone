# Runtime actor births and save restoration

Native Actor.Spawn278 now allocates real portable runtime actors, executes the
original caller's callbacks, and preserves born actors in checkpoints. This is
a runtime dependency for original inventory/player startup, not a claim that
automatic world startup, an authored player inventory or the campaign is
finished. This batch has not been verified on Quest and has not been installed
over the last accepted player-control build. Final host/build evidence is
recorded in [STATUS.md](STATUS.md) and [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).

## Allocation and defaults

Spawn requires an actual loaded, nonabstract script Actor UClass. None and an
abstract class return a null Object without allocating. Invalid class identities
fail the caller's transaction; an Actor instance is not accepted as a UClass.
Arguments are loaded from VM references, including local parameters. Omitted
Location/Rotation retain the spawner's values; Owner defaults to None and a None
Tag uses the concrete class's name.

Placement uses the prospective class-default collision radius/height and
bCollideWorld/bCollideWhenPlacing. The bounded world-only CheckLocation port
queries the current serialized ULevel root Model's collision BSP/hulls, not
rendered triangles, a controller capsule, actors or movers. Exhausted valid
placement returns None; malformed geometry and unsupported layouts fail
explicitly. This placement helper is not a general actor movement solver.

Each birth has its own map-qualified identity and appended Level actor index.
Name selection skips all existing case-folded identities, including authored
actors and deleted UObjects. A failed transaction removes provisional identities
and tail slots, so it consumes no successful birth name. The Level's original
slot order and null holes remain intact; restore orders births by their saved
numeric slot, not lexical name order.

A birth shares immutable authored class metadata and copies its concrete CDO's
mutable patch block once into frozenBirthDefaults. This sparse snapshot is a
complete baseline through structural sharing: unchanged slots use immutable
authored defaults, never a later mutable CDO. Fixed arrays, nested structs and
typed object references retain their own values. Later instance writes are a
separate overlay; later Default writes affect subsequent births but not existing
instances. An empty frozen patch block is valid.

Native initialization establishes Class, Name, transient ObjectFlags, Outer,
XLevel, Level, Tag, bTicked, Instigator, Brush=None, Location, OldLocation,
Rotation and Region.Zone. The born object has no fabricated serialized export:
its export index is a sentinel, while class/cast identity comes from its actual
loaded UClass. Authored Level members receive a native XLevel baseline at map
linking, without creating a script mutation or dirty checkpoint. Level and
XLevel are distinct bindings and are not guessed from the first LevelInfo.

## Original callback sequence

Collision registration precedes synchronous SetOwner and its owner callbacks.
A dormant Level returns the allocated actor without dispatching startup events.
For a begun Level, Spawn executes enum Spawned, PreBeginPlay and BeginPlay,
then checks bDeleteMe once. Self-deletion at that point returns None; the deleted
UObject can still be addressed directly until map teardown.

The surviving actor initializes its actual BSP Region. A non-Projectile in a
water zone changes to swimming and clears its base with the original event
policy. Pawn initialization also computes FootRegion/HeadRegion and updates an
existing PlayerReplicationInfo.PlayerZone. Enum PostBeginPlay and SetInitialState
follow, then the Deus Ex named PostPostBeginPlay event, InitBase and SpawnNotify.
No extra deletion checks or ZoneChange callbacks are invented between these
steps.

AttachTag InitBase iterates current live Level slots and current Tags; matching
actors call SetBase on the newborn, in the original direction. The other
supported branch queries actor overlap for unbased Decoration/Inventory/Pawn
actors with bCollideWorld and Physics None/Rotating. Hits retain the pin's final
stable actor-pointer ordering. The query does not discard Self; SetBase's normal
cycle refusal still applies.

SpawnNotify uses actual ActorClass ancestry by case-insensitive short name,
the original enum event and a nested-notification lock. The pin casts a
replacement to GameInfo, not arbitrary Actor; the portable implementation
preserves that restriction. Invalid chains, cycles and unsafe continuation
after a null replacement fail with bounded rollback rather than hanging.

## Actor collision registration

The actor-only collision registry keeps cached registration boxes separately
from current reflected geometry. Its cells are 256 Unreal units, with the
pin's 10-bit bucket aliasing. Queries deduplicate bucket encounters, use strict
cylinder overlap and exclude every actor with Brush. Visibility-only queries
and queries spanning at least 100 cells on any axis produce no actor hits, as
in the pin. Explicit actor/link/cell/work bounds prevent unbounded queries.

Movers with Brush register their actual transformed serialized Model bounding
box, including Rotation, MainScale.Scale, PrePivot and the 0.1-unit extent
margin. Their collision cylinder is not substituted for the brush box.
Metadata-only fixtures explicitly lacking native Actor collision fields do not
fabricate a collision schema.

Nonfinite geometry, negative registration/query extents, arithmetic overflow and work
exhaustion fail explicitly; these portable safety refusals do not invent valid
placement or clamp malformed bounds. Disabled actors retain finite negative
reflected bounds without manufacturing a registration, matching native283 and
the original AddToCollision early return.

SetCollision262 preserves omitted flags, removes cached membership, updates
bCollideActors/bBlockActors/bBlockPlayers and registers again. SetCollisionSize283
likewise removes, writes the two bounds and registers again; it does not invent
the pin's unimplemented room-fit refusal. Direct property assignments update
live narrow-phase values but do not rehash cached membership. Repeated Add and
Remove retain the pin's duplicate/stale bucket behavior. Native Destroy removes
registration before its Destroyed callback and removes world membership last.

Checkpoint load rebuilds collision registration from restored actor fields,
just as the pinned engine invokes LinkActorsToLevel after loading a save.
Collision buckets and cached registration boxes are native session state, not
serialized by ULevel.Save. Thus stale in-session membership from direct
assignments is deliberately not carried across a load.

This registry and Spawn placement do not implement general physics, movement,
encroachment/Touch dispatch or BSP-node membership. Existing SetPhysics_Deus
remains the pin's narrow reflected Physics assignment, not a simulation.

## Transactions, GC and checkpoint format

Birth allocation, nested callbacks, instance/default/state/animation writes,
ordered native relationship lists and collision mutations share the original
VM caller's transaction. Commit validates aggregate actor/property/value/link
and retained-byte limits before publishing a world revision or rebuilt Tag
index. Rollback restores the cached collision registry and all journals before
removing every provisional born identity and appended slot. It dispatches no
undo callbacks and publishes no partial revision.

Provisional and staged actors are GC-rooted while callbacks or restore
preparation can address them. Garbage collection runs at outer public execution,
state-resume and load boundaries, after the VM host/temporary roots have gone
away, not inside nested callbacks. Rejected Spawn, rejected staged loads and
successful replacement of old births must not accumulate unrooted UObjects.
World deletion and UObject lifetime remain independent.

Commit and restore preflight prepare newly referenced immutable CPU meshes and
brushes before publishing their revision. Existing decoded geometry is reused.
This prepares newly born geometry instead of leaving it absent from the CPU
cache; it does not prove every mesh drawable. Quest detects newly selected
materials, retires old actor chunks/pose jobs and incrementally uploads a
replacement actor atlas before publishing fresh geometry. Case aliases retain
masked-layer bindings. Static BSP resources are retained; unavailable individual
meshes/brushes preserve the existing per-object fallback. Android compilation
passes, but GPU correctness and CPU atlas-build frame cost remain unverified
on-device.

Script codec v5 / runtime checkpoint envelope v8 add a birth manifest containing
path, actual class, numeric Level slot and typed frozen defaults. The instance
overlay separately retains initialized fields, mutable properties, optional
animation/state and native lifecycle topology. Formats v1-v4 remain byte-exact
when no births require the new format. Legacy checkpoint loads replace the
current birth tail with the authored map baseline, rather than merging live
births into an old save.

Restore registers the complete symbolic birth graph before checking values.
Cold references between born actors, native ordered links and nested typed
properties are validated against real class/property metadata. Abstract/nonactor
classes, authored identity collisions, duplicate or noncontiguous tail slots,
missing initialized instance records, wrong concrete kinds, invalid array
indices and malformed struct/object constraints are rejected. A birth and its
overlay share one budgeted actor identity; mutable class-default records count
separately. Restore allocates and prepares every replacement registry before
nonallocating publication, reconstructs Outer/native bindings, and replays no
lifecycle or startup callbacks.
Read-only schema validation does not preflight collision work/resource limits;
extreme typed geometry can pass validation and then fail actual load atomically.

VM, rendering/mesh/material consumers, owner-animation resolution, interaction,
damage and gameplay checkpoint targets resolve runtime identities through the
case-folded registry. Valid mixed-case saved references therefore refer to the
same UObject. Authored package/table identities and file-resolution semantics
remain separate. Explicit empty Tags are not replaced with authored Tags during
restore.

## Verification entry points and fidelity sources

The generated native Spawn integration constructs actual package/property/
function metadata, compiled bytecode, a serialized Level with holes and a
decoded Model. Its controls cover argument handling, frozen defaults,
callbacks/nested failures, deleted-object access, exact appended order,
cold graph restoration, rejected manifests, case aliases and boundary GC.
The overlap prerequisite has independent registry/reference controls; the
structural codec independently tests malformed input and legacy bytes.
Original game-data integration is separate and uses the read-only installed
GOTY packages. None of these host controls proves headset performance or full
campaign startup.

```powershell
.\artifacts\prerequisites-20261009\build\portable_actor_spawn_test.exe
.\artifacts\prerequisites-20261009\build\quest_actor_overlap_test.exe
.\artifacts\prerequisites-20261009\build\quest_spawn_placement_test.exe
.\artifacts\prerequisites-20261009\build\quest_script_state_test.exe
```

Fidelity reference: SurrealEngine revision
`677ee14c5b83486e6634687953779aafb7973ad6`, restored locally under
`third_party/SurrealEngine/SurrealEngine/`:

- `Native/NActor.cpp`: Spawn278 binding and SetCollision optional arguments.
- `Packages/Engine/Actors/UActor.cpp`: Spawn allocation/startup/notification
  sequence and Destroy world removal.
- `Packages/Engine/Actors/UActor_Phys.cpp`: CheckLocation, SetCollision and
  SetCollisionSize remove/write/add ordering.
- `Packages/Engine/Actors/UActor_Zone.cpp` and `Pawn/UPawn_Zone.cpp`: initial
  Region and Pawn region/replication initialization.
- `Packages/Engine/Actors/UActor_Base.cpp`: AttachTag direction, InitBase
  actor overlap and SetBase callbacks/cycle policy.
- `Collision/TopLevel/CollisionSystem.cpp`, `OverlapTest.cpp` and
  `Collision/BottomLevel/OverlapAABBModel.cpp`: cached actor membership,
  cylinder/hash ordering and world placement geometry.
- `Engine.cpp`: LinkActorsToLevel on both map and saved-game load;
  `Packages/Engine/Resources/Level/ULevel.cpp`: serialized fields omit the
  native collision cache.

Original-data tests now execute dormant WeaponPistol allocation and the entire
unchanged InitializeInventory helper through Return PC770. A generated one-item
input creates actual WeaponPistol and Ammo10mm actors, executes GiveTo/base/Idle2
changes and preserves their typed/native graph through cold v8 restoration.
It does not fabricate a player inventory. The next observed StartUp dependency
is native720 GetPlayerPawn in ScriptedPawn.FindTaggedActor at PC61/opcode0x62;
the caller rolls back rather than substituting an NPC or PlayerStart.

The returned born-pistol desktop frames initially failed the chosen 1% coverage
gate with an incomplete-looking silhouette. The asset lies flat, and isolation
incorrectly discarded requested camera pitch. After fixing the orbit camera,
yaw45/pitch-60 shows the textured barrel, trigger and grip: 83 source triangles,
no missing selected material, 2.0434% coverage against the unchanged 1% gate.
The failing edge-on frames are retained. This is one inspected CPU fixture,
not original-renderer equivalence or Quest GPU verification.

Automatic original world/player startup, complete authored player inventory,
timers/AI/latent actions, general physics, campaign travel archives and on-device
validation remain required work. The new primitives remove dependencies; they
do not substitute a portable approximation for those unfinished systems.
