# Live native Actor ToWorld provider

2026-10-16. `PortableActorVmHost::NativeActorToWorld` now connects actual
current runtime actors to the verified original coordinate builders. It is
an internal value provider for the pending collision path, not a renderer
transform, world-startup bypass or implementation of Move/FindBase/falling.
No original executable code is loaded or called.

## Native virtual and field authority

The provider walks the actual loaded UClass ancestry, with128-element bound,
cycle/metadata ownership checks and nearest RF_Native selection. The explicit
56-class admitted set comes from ORIGINAL-TOWORLD-VTABLES.md, whose original
package-native classes match exported native vtables one-to-one. Only actual
Engine.Brush/Mover virtual implementations use the brush builder. A nonnull
Brush asset on another Actor does not change its native virtual method.
Unknown nearest native classes refuse rather than falling through to a known
earlier ancestor. Full ancestry validation continues after selecting a method.

The provider reads exact declaring fields through the runtime's typed
`Read(actor, qualifiedProperty,0)`, not unqualified leaf lookup, render
snapshots, raw portable object offsets or `objectPropertyPaths`. That path
preserves current qualified overlays, same-name shadow separation, instance
tags, birth-frozen defaults and actual class defaults/typed zero.

| Original declaring field | Kind | Flags | Canonical constraint |
| --- | --- | --- | --- |
| Engine.Actor.Location | Vector | 23 | Core.Object.Vector |
| Engine.Actor.Rotation | Rotator | 23 | Core.Object.Rotator |
| Engine.Actor.PrePivot (brush only) | Vector | 20 | Core.Object.Vector |
| Engine.Brush.PostScale | Struct | 1 | Core.Object.Scale |
| Engine.Brush.MainScale | Struct | 1 | Core.Object.Scale |

Dimensions, property ownership, actual Core StructProperty/Struct classes,
canonical struct owner/base and channel schemas are checked. Rotator requires
ordered Pitch/Yaw/Roll Int fields. Scale requires its original ordered Vector,
Float and enum-constrained Byte members with flags4001. It does not reinterpret
some arbitrary three-member struct as a native FScale or guess malformed
values as identity. All required finite/overflow rules come from the verified
scalar builders; no determinant/orthonormality or positive-scale gate is added.

ORIGINAL-BRUSH-NATIVE-OFFSETS.md establishes MainScale+32C/PostScale+340 from
actual authored link order, native Actor/Brush sizes and exported Brush copy
instructions. Thus the live brush call supplies PostScale first, then rotation,
then MainScale and PrePivot. TempScale and DrawScale do not participate.
Rotation explicitly consumes `OriginalSineProfile::Table()`; its complete
provenance and measured CPU/precision limits remain ORIGINAL-SINE-PROFILE.md.

## Read-only diagnostic seam

`ReadPortableRuntimeActorToWorld(actorPath)` exposes only a detached snapshot:
canonical actor path, selected native class, native implementation label, and
the twelve Float coordinate values. It accepts actual indexed current-map
Actor instances, never a UClass, CDO alias or arbitrary path-as-identity.
It returns no native pointers, cache-validity capability, collision registration
or movement success. Runtime-owner-thread rules apply. Errors throw without
publishing a partial snapshot or gameplay state.

Read-only here means no GC objects/roots, saved properties, world revision,
clock, RNG or file writes. First queries may decode original metadata from
read-only files and retain immutable StructDescriptor/common-field-link
caches on already loaded metadata objects; host-local schemas are temporary.
It is not a blanket promise of zero allocation/I/O or bitwise-const loader
metadata, nor a claim that per-frame callers should recreate a host for each
actor. The future collision consumer should share the original outer host.

The internal provider intentionally has no presentation-active, bBegunPlay,
bDeleteMe, bMovable or renderer-model gate absent from native ToWorld. The
diagnostic's explicit current-world-slot requirement is not a replacement for
the original query's owner lifetime rules. In particular empty native Model
queries return before calling ToWorld; future collision adapters must preserve
that inactive branch rather than eagerly demanding an owner snapshot.

## Remaining collision integration

The current native collision service still needs actual engine/cylinder and
Brush/Mesh/Model primitive ownership, bounded generation-keyed brush Model
geometry, original hash enable/add/remove lifecycle and process RNG phase,
shared work/rollback lifetime, collector/SingleLine checks, callbacks, support
and the complete Move/Tick/startup/player path. The existing pinned overlap
registry is not the original collision hash and must not acquire that name.
Live native Location writes must not automatically rehash: original explicit
Remove/Add sites and stale/current-bound behavior are observable.

The provider alone does not make crates destructible, supply gravity, move
doors through original encroachment or establish on-headset acceptance.

## Verification checkpoint

Generated v68 metadata/VM tests pass734 checks and17 explicit refusals. They
exercise native Actor/Decoration/Brush/Mover selection, script-only inheritance,
instance tags, qualified native setters versus same-name shadow setters,
committed live Location/Rotation/PrePivot/Main/Post values, frozen instance and
derived defaults after CDO changes, zero-scale and malformed schema rejection.
Generic Actor still succeeds when unused brush Scale metadata is malformed.
Successful and failed reads preserve the complete save bytes, world revision,
GC object/count accounting and package bytes. Expected simple transforms are
literal independent results, not calls to the implementation's scalar builder.

The owned-original dormant Training integration passes52213 checks and17
identity refusals across all1308 actual actors, including1017 Brushes and25
Movers. Nearest native provenance is independently derived from original
package RF_Native exports and superclass references; twelve Float results are
compared with qualified field reads feeding the independently tested scalar
builder. This is an adapter test, not another independent numeric oracle.
Complete checkpoint/revision/native-clock/world-slot/GC and native Model-cache
snapshots remain unchanged. Original source byte fingerprints and mtimes are
unchanged. Actual CDO aliases, UClasses and invalid/foreign identities refuse.

The coherent full host build passes. Fresh CTest reports121 passed out of137
registered, with16 explicit owned-data skips and zero failures in45.80s. The
owned-original adapter above was run separately with an explicit game root;
its standard no-root CTest invocation intentionally skips rather than inventing
original data. Independent read-only review found no actionable provider/test
or CTest-wiring issues.

Both the provider and tests pass strict ARM64 syntax checks. Local ignored
logs are `artifacts/live-actor-coords-*-20261016.log`. No commercial binary
was executed, world begun flag altered, relevance gate bypassed, render state
used as collision authority or player possession fabricated.
