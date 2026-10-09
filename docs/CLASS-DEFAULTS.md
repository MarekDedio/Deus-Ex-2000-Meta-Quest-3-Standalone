# Concrete class defaults and weapon startup

The portable interpreter now supports original `DefaultVariable` writes on
loaded script Actor classes. This is a campaign-startup prerequisite: the
original `DeusExWeapon.PreBeginPlay` copies `default.PickupAmmoCount` into
`default.mpPickupAmmoCount` when the latter is zero. It must not be replaced by
an instance write or skipped callback.

## Storage and isolation

The pinned engine's `VM/ExpressionEvaluator.cpp` targets the context's concrete
class default block (or the class itself when the context is a UClass).
`Packages/Core/UObject.cpp`, `PropertyDataBlock::Init`, copies the base default
block into each loaded derived class and the concrete default block into each
instance. Later CDO writes therefore do not modify existing instances, siblings,
or already-loaded descendants.

The portable runtime preserves its immutable, source-qualified authored tags
for those existing copies and stores mutable defaults separately on the concrete
class. Default reads first inspect that concrete class's mutations; ordinary
instance reads never inspect mutable class patches. Parent mutable patches do
not leak into already-loaded derived defaults. Structs and fixed-array slots
retain detached, typed values and their actual declarations' object constraints.
The read-only `ReadPortableClassDefault` getter accepts only an actual loaded
script Actor UClass and does not write to packages or actor state.

All default writes participate in the root VM transaction, including nested
functions. An unsupported later operation restores both previously present and
previously absent default patches along with the actor property/state/clock
journal. Persistent default/state storage has combined retained-byte and
typed-value-node caps; saves additionally use combined gameplay/schema/codec
budgets. The loaded export registry roots all referenced objects, and dirty
default state participates in the existing map-unload/replacement guard.

This is not dynamic actor creation: a future birth must copy the concrete
class's *current* mutable defaults into its own detached instance storage.
No dynamic actors, delayed class loading after mutation, or arbitrary native
CDO wrappers are fabricated here. VM `ClassContext` remains unsupported.

## Collision-size native

Native 283, `Actor.SetCollisionSize`, accepts two required numeric, finite
arguments, assigns the original scalar Float `CollisionRadius` and
`CollisionHeight`, and returns true. Both writes are journalled and saveable.
The pinned `UActor_Phys.cpp` does not implement a room-fit refusal or clamp;
the portable host does not invent either. Malformed/non-finite input fails
without leaving either assignment committed.

The native's reflected assignment/return contract is supported, not full actor
physics. The portable runtime does not yet have the pin's actor collision hash
to remove/reinsert the actor or simulate those bounds. Static world collision
and the VR player movement path are separate.

## Persistence

Runtime checkpoint v6 binds to script codec v3. A nonempty concrete-class
default section is required; actor records retain the codec-v2 layout, so actor
properties/clocks/state and class defaults compose in one bounded blob. Each
default record has only a qualified class identity and typed property records,
not a clock or state frame. Existing v3/v4/v5 runtime formats and codec-v1/v2
output remain unchanged when no defaults were mutated.

Validation checks actual script class identity, declaration key/name/index,
recursive struct shape and object/class constraints before clearing any live
state. A successful v6 load replaces the entire default timeline; omitted
classes reset to their authored defaults. All successful v1-v5 loads clear
mutable defaults. Failed validation/load leaves the previous actor/default
timeline intact. No lifecycle callback is replayed by a restore.

See [script-state saves](SCRIPT_STATE_SAVE.md) for the wire format, budgets,
legacy rules and paired durable checkpoint workflow.

## Acceptance scope

Generated packages exercise actual compiled bytecode and metadata in the
portable host, including concrete-class isolation, nested structs, fixed arrays,
typed references, rollback and replacement saves. Original-package integration
must be invoked explicitly with the user's read-only game root; its labelled
checkpoint preconditions are test inputs, not original world startup.

Final generated integration passes 1,291 controls, including 37 rejection
controls, saved CDO-only map guards, GC retention/shutdown and explicit runtime
reinitialization. The structural codec passes 3,920 controls / 3,793 rejections;
the unchanged state-frame codec passes 1,251 / 621. The full ordinary CTest run
passes 44 tests, with three optional original-data entries skipped by default.

The separate original-data callback test passes 563 controls on the 101,375-
object script runtime. It runs the *complete* original
`02_NYC_Underground.WeaponAssaultGun0.PreBeginPlay`, with explicitly generated,
schema-validated `bGameRelevant=true` and `Level.NetMode=0` (singleplayer) or `2`
(listen server). Original `Engine.Actor.PreBeginPlay` subtracts 0.75 from the
1.1 collision height, retaining radius 15; the concrete AssaultGun default
`mpPickupAmmoCount` changes 0 to 30 while the existing instance, base and sibling
defaults remain unchanged. The exact inherited property identity, codec-v3/v6
roundtrip, legacy restoration and instruction-budget failure after the original
nested CDO write are checked. The unmodified dormant map state still refuses
`Destroy279` at `Engine.Actor.PreBeginPlay:199`, with full rollback.

Final-source original actor and runtime regressions also pass. Ignored evidence
is under `artifacts/prerequisites-20261009/`: `class-defaults-final-ctest.log`,
`class-defaults-generated-final.log`, `class-defaults-original-actors-final.log`,
`class-defaults-original-runtime-final.log`, and
`class-defaults-original-callback-final.log`. The ARM64 APK compiles and verifies
with signature scheme v2; its new runtime is in the linked `deusex_data_probe`
library. It is not installed over the accepted player-controls build.

To repeat after building the desktop targets:

```powershell
.\artifacts\prerequisites-20261009\build\portable_class_defaults_test.exe
.\artifacts\prerequisites-20261009\build\portable_original_defaults_test.exe 'D:\Steam\steamapps\common\Deus Ex'
```

`Spawn`/`Destroy`, lifecycle publication, automatic world startup, AI, actor
physics and full campaign progression remain unfinished. Passing these checks
does not prove campaign playability or on-headset gameplay acceptance.
