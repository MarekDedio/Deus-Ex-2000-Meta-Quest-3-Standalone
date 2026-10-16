# Original Actor.GetMeshTexture1013 (2026-10-16)

This is an original-script prerequisite for crate fragments, not a claim of
working live melee, debris physics, surviving contents or world/player startup.
No original executable or DLL is loaded or executed. Only package metadata and
static PE/disassembly are inspected; commercial assets are not redistributed.

## Original authority

Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
The exec thunk at10302f9a reaches103e17c0..103e18bb. Native1013 has function
flags0x401, optional Int texnum0x90 and Texture ReturnValue0x580, all scalar.
Its NativeParm bytecode references texnum. The wrapper initializes texnum0
before evaluating the argument and writes Object None to the return destination
after argument evaluation, before checking Mesh. None is a written null result,
not PlaySound's distinct no-write return behavior.

Actor.Mesh, Skin and MultiSkins are actual declared ObjectProperty fields,
flags0x21 (Edit|Net), dimensions1/1/8, constrained to Engine.Mesh/Texture/Texture.
No renderer convenience field or closest same-name redeclaration substitutes
for these inherited native fields.

The retail return order is:

1. Mesh None returns None even with Skin or MultiSkins present.
2. Nonnull MultiSkins[texnum] wins for the unsigned eight-slot index range.
3. A positive, valid, nonnull raw Mesh.Textures[texnum] wins before Skin.
4. Otherwise nonnull Skin wins, including negative/out-of-range indices.
5. Otherwise a valid nonnegative raw mesh slot returns its value, possibly None.
6. Otherwise return None.

No Actor.Texture, DefaultTexture, last-MultiSkin or material-ordinal fallback
exists. The O(1) selector receives the actual slot count and one checked slot;
the span convenience wrapper delegates to that same core.

## Virtual/native identity

All47 audited exported Engine Actor-family vtables have virtual+0x80 pointing
to103016f9 ->10367eb0. Its unsigned index<8 path reads Actor+1a4[index]. The
eight audited DeusEx native Actor classes import that same Engine.GetSkin
through1001af26/IAT1002721c. Extension.PlayerPawnExt imports it through
100545a6/IAT1005925c. The runtime contains the explicit audited class allowlist,
not an Engine.* prefix match. Native ancestry uses UClass RF_Native0x04000000;
CLASS0x80 is RuntimeStatic and is not a native-constructor registration.

DeusEx.dll SHA256:
`B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04`.
Extension.dll SHA256:
`93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
Training's script-only CrateBreakableMedGeneral -> Containers inherits the
audited native DeusExDecoration implementation. An unknown nearest native
override refuses when Mesh is nonnull; a missing Mesh needs no virtual query.

## Portable runtime boundary

Native1013 is a read-only query on actual Actors, including concrete class-default
objects and retained removed Actors. Native/default/context dispatch bridges
all reach the same audited handler. UClass metadata itself is not an Actor.
Exact declaring property reads retain indexed inheritance, explicit None,
frozen instance/default blocks and hidden native fields.

Early MultiSkin and Skin branches do not decode mesh payloads. A mesh-slot
query requires exact Engine.Mesh or Engine.LodMesh class identity; the current
decoder's partial SkeletalMesh prefix is not authoritative for its skeletal
tail and is explicitly refused on that path. Foreign same-leaf Mesh/Texture
classes cannot establish native asset identity.

Geometry/animation/texture display paths share an immutable CPU-owned cache.
Only its raw texture reference at the selected index is resolved with checked
package outer/import/class provenance and exact Engine.Texture ancestry before
returning. Render materialTextureIndices never remap this query. Cache preparation
may survive transaction rollback; actor/default/inventory/RNG/world revision
changes may not. No mesh decoding or UObject access is added to the XR thread.
Assets still need CPU preparation before live interactions to avoid cold-decode
hitches; this backend checkpoint does not claim that gameplay publication path.

## Verification

The pure selector exhausts presence masks, signed/extreme bounds and raw-slot
priority. Generated v68 integration packages contain genuine decoded Mesh and
LodMesh geometry with deliberately different material and raw-texture indices.
They exercise production raw/reflected/optional calls, actual CDO queries,
frozen/default/hidden-field storage, argument writes and complete refusal rollback.
Owned-original tests explicitly load the real mesh packages and check both
Training crates without modifying original package data. Terminal results and
the current fatal-damage boundary are recorded in STATUS.md.

Terminal results: pure12697648 checks; generated982 checks/32 expected refusals;
owned468/4; existing begun Destroy279 contents347/2; actual NPC280519/26.
Both crates' isolated dormant-world None-instigator fatal hits now complete
Frag/nativeDestroy with six WoodFragments each, original selected Skin and
bit-exact stored velocity/scale/RNG. Warm/cold replacement, repeated Destroy,
final-return budget rollback, source bytes/timestamps and GC pass. Dormant
bBegunPlay=false suppresses Destroyed/Contents normally; do not infer usable
loot or live VR damage from this fixture. At this earlier checkpoint true
Pawn-instigated fatal damage still refused MakeNoise512:428, with complete
rollback. The subsequent implementation in ORIGINAL-MAKE-NOISE.md now
completes that isolated Pawn-instigated path too; it is not a VR-player claim.

The coherent full host build and strict ARM64 syntax checks pass. CTest102
passes/117 registered/15 explicit owned-data skips/zero failures in42.06s.
Local ignored logs are `artifacts/crate-mesh-texture-*-20261016.log`. The initial
fixture run exposed a missing CPF_Net replication-offset field in generated
test data; the corrected final fixture supersedes it. The initial damage probe
failed its obsolete expected1013-refusal assertion because the original fatal
script now succeeded; expanded final success/rollback controls supersede it.
No new APK was produced; the accepted NPC device build remains unchanged.
