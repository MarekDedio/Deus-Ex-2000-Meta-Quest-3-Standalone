# Live original Actor primitive queries

2026-10-17. The native host provider connects actual indexed Actor
values and loaded asset identities to original Mesh/LodMesh collision methods.
This is collision-path integration work for real crate movement, not a render
box shortcut, world-startup bypass, new physics tick or destructible-crate APK.
No commercial executable code is loaded or run.

## Original source contract

ORIGINAL-NATIVE-PRIMITIVE-DISPATCH.md closes original AActor.GetPrimitive's
nonvirtual Brush->Mesh->Engine.Cylinder precedence and each supported actual
primitive's virtual methods. Both UMesh.LineCheck extent branches delegate to
UPrimitive.LineCheck; Mesh/LodMesh collision bounds inherit UPrimitive and use
actual owner collision fields, not animated/render geometry. A Brush Model
never falls back to a cylinder just because its Model query service is pending.

Owned original Engine.u SHA256
1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA
has these scalar original Actor-owned fields (all no property base):

| Declaring field / export | Actual Core property | Flags | Constraint |
| --- | --- | --- | --- |
| Actor.Location / 8 | StructProperty | 23 | Core.Object.Vector |
| Actor.CollisionRadius / 32 | FloatProperty | 23 | none |
| Actor.CollisionHeight / 39 | FloatProperty | 23 | none |
| Actor.Mesh / 136 | ObjectProperty | 21 | Engine.Mesh import-38 |
| Actor.Brush / 1380 | ObjectProperty | 2A | Engine.Model import-51 |

All flags/numbers are hexadecimal except export/import indices. The exact
declaring property schema/ownership matters: same-name script shadows cannot
substitute their values or metadata. Brush and Mesh fields are read lazily, so
a selected Brush does not activate an unused malformed Mesh field. A malformed
nonnull Brush must refuse, not become None and fall through to Mesh.

## Value and asset authority

The provider uses the existing typed Read path for current qualified overlays,
instance/class tags, frozen defaults and zeros. An optional read-origin output
records the actual chosen authored source/tag, without changing selection for
other callers. On authored Object tags the provider validates the original
value import's ClassName against the selected actual asset's class ancestry;
a matching path alone cannot conceal a wrongly declared import. Typed mutable
overlays are already checked on writes. The pinned value-import resolver ignores
ClassPackage for non-Class value lookup; that distinct rule is not replaced with
an exact-package requirement for actor-value tags.

Selected primitives are actual indexed exports, with canonical source/export
identity and a real Class reference for the explicitly audited Engine.Model,
Engine.Mesh or Engine.LodMesh type. Native UClass evidence comes from the audited
static registrations/vtables, not asset RF flags, reflection's unqualified leaf,
render caches, a copied CDO or a fabricated class descriptor. Unknown native
subclasses/SkeletalMesh/foreign lookalikes cannot silently acquire a known method.
If a loaded native UClass wrapper is present, its bounded RF_Native/direct-Super
chain must agree with that actual class reference and registration. Normal
owned classless assets remain supported; no wrapper or CDO is fabricated.

Mesh/LodMesh nonnull-owner bounds and line checks read current qualified Actor
Location/CollisionRadius/CollisionHeight. No DrawScale, mesh vertices, LOD,
animation, texture, native Engine.Cylinder fallback or head tracking is needed
on an already selected Mesh branch. Geometry payloads need not be decoded:
these methods do not read them. Negative/nonfinite/overflow active numeric data
has the existing explicit supported-domain refusal policy; discarded/inactive
inputs are not eagerly reclassified as errors.

`quest_mesh_line_trace.h` implements the complete wrapper before the verified
cylinder arithmetic. All three extents become literal+0 only if every component
is zero or unordered (NaN). Any nonzero ordered component preserves all original
extent bits. Bit classification avoids host signaling-NaN arithmetic. Flags
are forwarded semantically but ignored by the actual underlying UPrimitive body.
Null-owner, partial Normal writes on genuine clear and atomic refusal remain
the shared cylinder's contract; this wrapper is not another numeric oracle.

## Diagnostics and remaining production integration

Read-only diagnostics require an actual indexed current-map Actor, without
presentation-active/begun/delete/movable gates on the internal native provider.
They return detached identity/virtual provenance, bounds or a fixed-seed line
packet view; no native pointer or collision capability is exposed. The line
diagnostic seeds all pointer/Location/Normal fields0, Time1 and Item-1 explicitly.
It does not replace the hash/collector caller's different FCheckResult seed or
the full mutable hit packet retained by the internal provider.

Model identity selection is available; actual Model bounds/BSP query ownership
and generation-keyed data must still join the existing verified owner sweep.
Both-null Brush/Mesh still refuses until genuine Level.Engine.Cylinder lifetime
exists. No fake Cylinder is substituted, no engine constructor/init is implied,
and no render data is authoritative for collision. Shared outer-transaction
work/rollback, collision-hash enable/add/remove/RNG lifecycle, hit lifetime,
collector/SingleLine/Move callbacks/support, startup/player possession and
falling are still required before live crates can be claimed complete.

## Verification

Targeted host build and strict ARM64 syntax pass for the current provider,
owned integration, initial generated fixture and Mesh wrapper. The independent
extent-wrapper adapter suite passes138333 checks/9057 explicit supported-domain
refusals,2237 blocked/8932 clear cases and326 partial-Normal clears. Its expected
packet delegates the separately numerically verified cylinder core; it is an
adapter/canonicalization test, not another independent arithmetic oracle.

Owned-original dormant Training integration passes23126 checks/246 refusals:
all1308 actors,1017 selected Model identities,64 Mesh/LodMesh actors and227
explicit absent-engine fallback refusals. Every selected Mesh actor passes
independent padded bounds formula checks and four fixed-seed line probes
(256 total); expected line arithmetic uses the separately verified cylinder
helper as an adapter oracle. Actual two training crates retain exact original
asset/type/positions/radius34/height24; knife and crowbar are included.
Complete checkpoint/revision/world/clock/GC/dirty and Model-cache identities,
as well as original source bytes/sizes/mtimes, remain unchanged. No engine
classes are prepared by these queries and bBegunPlay stays false.

Generated integration now passes1608 checks/45 refusals. Separate opaque
read-only assets prove queries do not decode rendering data; structurally valid
v68 Mesh/LodMesh bodies support the mutation phase through normal existing
render-publication preparation. No commit guard is bypassed. Literal padded
bounds/central hits/clear/partial-Normal packets, hidden/DT_None mesh selection,
qualified native versus shadow writes, frozen defaults, Mesh/Brush/Location/
dimension overlays, failed writer rollback, source-import ancestry/ignored
ClassPackage and malformed schema/lazy inactive Mesh branches all pass. Valid
loaded native class chains and non-native/wrong-direct-base refusals are also
directly exercised. The final generated test again passes strict ARM64 syntax.
The full coherent host rebuild passes. Fresh CTest:123 passed/141 registered/
18 explicit owned-data skips/zero failures in44.20s. Local ignored logs use
`artifacts/live-actor-primitive-*-20261017.log`. No replacement APK or on-device
acceptance is implied by this checkpoint.

## Selected-Model follow-up

The following constraints informed the subsequent implemented bridge; see
ORIGINAL-SELECTED-MODEL-COLLISION.md for its current contract and verification.
Nonempty zero-extent owner queries and real Engine.Cylinder lifetime remain
unsupported. The notes below preserve the boundary identified at this earlier
Mesh-provider checkpoint.

### Original selected-Model bridge constraints

The existing one-record root ULevel.Model cache is not a selected Brush cache.
The next cache must retain immutable collision geometry under actual current
ULevel/generation plus selected Model UObject/source/export/path identity, never
redirect root Model authority to the first Model or a renderer brush mesh.
Warm restores retain it; unload/shutdown clear all raw keys and byte ledgers
before GC; cold loads never reuse a generation. Limit both entry count and
cumulative retained bytes (all twelve geometry vector capacities, strings,
object/control-block/key/container overhead), not just per-entry raw scratch.
Decode/identity/admission failure must publish no entry or ledger change.

`LoadPortableModel68` currently performs strict whole-render-Model admission,
including finite sphere/render/light data and valid-box ordering. That is not
the original query's active-field contract: empty LineCheck ignores owner,
coordinates, endpoints/extents, box/sphere; nonnull-owner bounds ignore source
box Valid and fold reversed finite endpoints. A collision decoder must preserve
those inactive raw values and separate structural bounds from active kernel
validation, or clearly state any temporary stronger asset-admission boundary.
Do not claim empty-query inactivity after silently rejecting unused fields.

After actual Model data admission, an empty query returns RootOutside with
every caller hit byte unchanged before capturing owner ToWorld. Nonempty
extent queries capture current native ToWorld exactly once and use the verified
packet adapter. Zero-extent Model queries remain a separate owner-point adapter.
Model bounds always transform the original eight corners, even for empty BSP,
with no cylinder padding or source-valid gate. Query work/validation counters
must belong to the same outer host as hash callbacks/Move; the packet adapter
currently discards trace statistics, so expose them without tracing twice.
Refusal must propagate to that root transaction, never become clear/fallback.
