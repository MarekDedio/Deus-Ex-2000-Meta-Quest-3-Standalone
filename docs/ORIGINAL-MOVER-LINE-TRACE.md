# Retail owner/brush Model line checks

Static, read-only audit of the owned GOTY installation, 2026-10-16. No original
DLL or EXE was executed. Captured-value owner sweeps and their verification
are recorded below; native Move266, FindBase, falling and live crate gameplay
remain incomplete.

Primary files under `D:/Steam/steamapps/common/Deus Ex/System`:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Exports were identified with `llvm-readobj --coff-exports`; bounded Intel
disassembly used MinGW objdump. PE sections and ordered import tables were
independently mapped using the existing read-only PE audit script. Addresses
below are original image virtual addresses, never portable object offsets.

## Primitive selection precedes the line check

`AActor.GetPrimitive`, export thunk `10303EC2`, body `1034C9A0`–`1034C9C0`, is:

1. Return nonnull Actor `+178` Brush.
2. Otherwise return nonnull Actor `+174` Mesh.
3. Otherwise return `actor.XLevel(+AC)->Engine(+40)->Cylinder(+40)`.

There is no DrawType, Pawn/Decoration ancestry, visible-model availability or
mesh-render fallback gate in this body. Brush wins even if Mesh is also nonnull.
The actual returned primitive's virtual LineCheck determines the next path.
Returning a brush registration bounding box as the final narrow-phase result
would not reproduce this operation.

`UMesh.LineCheck` thunk `1030259F` reaches `103A6D10`. The complete wrapper
was closed by further read-only inspection on2026-10-17: **both branches** call
`UPrimitive.LineCheck` (`10301B9F` → `103D8AF0`) at103A6DB9. A finite nonzero
extent forwards all three original extent bits. When all three comparisons
have x87 C3=1, `103A6DD1..103A6E34` explicitly replaces each extent DWORD with
literal0 and jumps back to that same call. Thus finite all-zero extents
canonicalize signed zeros to+0; there is no mesh triangle continuation.
Unordered/NaN comparisons also set C3=1, so the exact wrapper predicate is
zero-or-unordered, not ordinary C++ equality or an eager finite check.

Consequently actual Mesh/LodMesh queries use the primitive/cylinder path for
both extent branches, not animated triangle collision. Their native bounding
box method also inherits UPrimitive's actor collision bounds, not render/mesh
frame bounds. This does not authorize a cylinder for an actual Brush Model
or an unaudited native subclass. UPrimitive numeric verification remains in
ORIGINAL-ACTOR-LINE-TRACE.md and STATUS.md; actual object/provider integration
and the Mesh wrapper implementation/tests are still separate work. Full
virtual/registration/wrapper evidence is ORIGINAL-NATIVE-PRIMITIVE-DISPATCH.md.

## Owner coordinates are a captured virtual result

`UModel.LineCheck` is `10303E5E` → `103F3C20`–`103F4184`.
An empty Model returns RootOutside before checking extents or Owner, with no
caller hit writes (`103F3C48..103F3C50` → `103F416E`). Coordinates, endpoints
and extents are inactive on this original branch. The
nonzero-extent branch constructs FBoxCheck through `103034EF` →
`103F18E0`–`103F197F`.

With nonnull Owner the constructor calls Owner virtual `+88` exactly once,
copies its returned **48-byte FCoords** into helper `+0C`, and retains Owner
at helper `+08`. Null Owner instead copies `GMath.UnitCoords`. The helper
also retains original world-axis Extent and full world End/Start. It does not
transform the moving box to a local axis-aligned box or split its segment.

Actual native vtable mapping:

| Native class | ToLocal `+84` | ToWorld `+88` |
| --- | --- | --- |
| AActor | `10303369` → `1030D750` | `10303AC6` → `1030DAB0` |
| ABrush | `103026A3` → `1030EF90` | `103023AB` → `1030F310` |
| AMover | same ABrush entry | same ABrush entry |

This was read from each exported vtable, not inferred merely because Mover
inherits Brush. A generic Actor rotation matrix is not the AMover transform.

The ABrush ToWorld body reads Location `+114..11C`, Rotation `+120..128`,
PrePivot `+180..188`, and two complete FScale blocks at `+32C` and `+340`.
The exact native name/offset mapping is now independently established in
ORIGINAL-BRUSH-NATIVE-OFFSETS.md: +32C=MainScale, +340=PostScale. Keep the two
blocks distinct; do not copy a pinned engine's MainScale-only transform as
retail proof.
DrawScale is not read anywhere in this ABrush ToWorld body.

Recoverable original operation order, retaining the native blocks by offset:

1. Start from UnitCoords; subtract Location from all three Origin components,
   storing each Float (`1030F353`–`1030F389`).
2. Apply Core FCoords operator `*=(FScale)` to the `+340` block
   (`1030F38F`–`1030F39D`).
3. Apply Core FCoords operator `*=(FRotator)` to actual Rotation
   (`1030F3B9`–`1030F3C7`).
4. Build the `+32C` block's original sheer-coordinate operation, compose it
   with FCoords `*=(FCoords)`, scale each axis's corresponding components
   through the Core FVector operation, and separately divide Origin XYZ by
   that block's scale components (`1030F3CD`–`1030F4B5`).
5. Subtract the earlier Float-negated PrePivot components from Origin,
   each Float-stored (`1030F4B9`–`1030F4E6`), and copy all 48 output bytes.

Relevant imported Core identities are `FCoords*=(FCoords)` at IAT `10599B20`,
`FCoords*=(FRotator)` at `10599B48`, `FCoords*=(FScale)` at `10599B4C`, and
`FCoords/=(FScale)` at `10599B40`. The Core bodies contain explicit Float
stores between products/divisions, rotation lookup-table quantization and
piecewise sheer-rate behavior. A collapsed host-double matrix inverse has not
been demonstrated numerically equivalent. Complete independent numeric tests
for these coordinate-building operations are now recorded in
ORIGINAL-BRUSH-COORDINATES.md. Real native-object/property/LUT integration is
still distinct from the supplied-value builder service.

## Two different owner-plane transforms

The traversal and hull stages use different original arithmetic. They must
not share one pretransformed plane cache unless both distinct results survive.

### BSP traversal plane

At `103F4320`–`103F43A8`, nonnull Owner transforms each current BSP plane:

- Dot authored normal against captured Coords axes using imported FVector dot
  (`1059A0AC`, Core `101017AD` → `10117710`). Execution order is **ZAxis first**
  (`103F4331`, Float store `103F4337`), then **YAxis** (`103F4341`, Float store
  `103F4347`), then **XAxis** (`103F4351`, Float store `103F4371`). The resulting
  plane still stores its normal in ordinary XYZ field order.
- That imported dot's retained summation is `(zProduct+yProduct)+xProduct`.
- Compute captured `Coords.Origin.TransformVectorBy(Coords)` through
  `10599B30` (Core `10102225` → `10119E40`), yielding a Float vector.
- Compute imported dot of that vector with the stored transformed normal;
  retain the result until `authoredW - dot`, then Float-store the new W.

The resulting world plane enters the same full-segment, padded-world-extent
near-first BSP tests documented in ORIGINAL-MODEL-EXTENT-TRACE.md. There is no
normal normalization or local extent rotation in this stage.

### Ordered hull plane

At `103F466D`–`103F469A`, each original hull plane is copied, transformed by
imported `FPlane.TransformPlaneByOrtho`, and copied back. The import is
`1059A118`, Core thunk `101015EB`, body `1011A000`–`1011A0F0`.

For authored normal N and captured coordinate axes, that body computes:

```
N'.x = Float((AxisX.y*N.y + AxisX.z*N.z) + AxisX.x*N.x)
N'.y = Float((AxisY.y*N.y + AxisY.z*N.z) + AxisY.x*N.x)
N'.z = Float((AxisZ.y*N.y + AxisZ.z*N.z) + AxisZ.x*N.x)
O'   = Float3(Coords.Origin.TransformVectorBy(Coords))
W'   = Float(W - ((O'.y*N'.y + O'.z*N'.z) + O'.x*N'.x))
```

The normal components execute/store **X, then Y, then Z** in Core
`1011A000..1011A056`. The transformed Origin then Float-stores XYZ before the
retained Y/Z/X W calculation (`1011A0B1..1011A0EA`). All operations between
indicated stores retain original PC64 precision.
In particular the instruction and component-store schedule differs from
traversal's imported Z/Y/X dot. The two dot formulas only exchange their first
two products; for ordinary finite PC64 nearest-even arithmetic that exchange
alone is commutative and is **not** a finite cancellation witness. Keep the
audited stages explicit without claiming an unverified numeric distinction.

Only **after** this transform does raw hull reference bit `40000000` invoke
FPlane.Flip (`1059A114`), negating all four Float coefficients
(`103F469D`–`103F46D1`). Component sign masks and bevels consume the resulting
transformed/flipped planes, not untransformed normals. Flip-before-transform
may look geometrically equivalent but is not the audited numeric order.

## Owner hull clipping and outputs

Before the first primary-plane clip, the helper copies, transforms, flips and
computes sign masks for **all** admitted authored hull planes
(`103F462D..103F4783`); primary clipping begins at `103F4813`. A later plane's
transform has therefore already executed even if the first plane will reject
the interval. Transforming each plane lazily immediately before its clip is
not the same staging. Sign masks and all pair bevels use these already
transformed/flipped planes.

The original hull parser stops on literal `-1` or at its64-plane cap. At the
cap it does not require the following word to be `-1`: the loop's count check
is at `103F4630..103F4633`. At `103F4789..103F47B2` it selects the six bound
words starting at `collisionBound + admittedPlaneCount + 1`, skipping one
word whether it is the actual terminator or the cap's following word. A
portable bounded format admission can explicitly require a genuine literal
terminator and reject oversized/nonterminating hulls, but that stricter
structural validation must not be described as a retail parser check.

Even with nonnull Owner, `103F47BA..103F47DC` **unconditionally copies all six
bound words** with integer MOV instructions into helper `+5C..70`. The owner
path therefore needs those words structurally present. Their contents are
not Float-loaded, compared or used arithmetically on the owner path; NaN,
infinite or numerically reversed bound words alone are not a reason to reject
an otherwise supported owner hull. Null-owner bounds clipping separately
uses and must validate the active numeric values. Structural availability is
not permission to eagerly apply null-owner numeric validation to owner-unused
data.

The original owner branch skips the six extra axis-aligned LeafHulls bbox
planes entirely (`103F495D`–`103F4962` jumps directly to `103F51AF`). They are
**null-owner only**. World box support/pushout still uses the original world
Extents against transformed world planes. The ordered original planes, strict
interval comparisons and axis bevel chain otherwise remain those already
audited for the null-owner kernel. The bevel loop is i outer, j<i inner,
axis X/Y/Z (`103F51AF..103F5617`); no additional Owner gate is applied there.

For a nonempty Model entering the extent branch, the wrapper writes caller
**Hit.Time=Float2** at `103F3FA9`, before constructor/captured ToWorld work.
If no hull is accepted, `103F4032` branches directly to the clear return at
`103F4111`. Caller Time stays2 and caller Location, Normal, Actor, Primitive,
Next and Item remain untouched. Original clear is not inferred from Time==1
on this no-hit branch. The earlier empty-Model branch, by contrast, performs
no hit writes at all.

On accepted hull, `103F564A`–`103F567F` stores Time, the selected **already
world-space Normal**, Actor=Owner, Primitive=Model and the helper-hit flag.
It does not write FCheckResult.Item. The extent wrapper then applies its
existing backoff, Float-stores Time with FSTP at `103F408A`, and computes world
Location from original world Start/End and that stored Time. It returns clear
if the resulting stored Time equals Double1 (`103F40F3..103F410F`), even
though the accepted-hull packet has already been written. It performs
**no second normal transformation or direction
facing correction**. These normal postprocessing operations in the wrapper's
zero-extent path must not be imported into the extent branch.

Trace flags are copied into FBoxCheck `+48`, but the complete inspected
extent traversal/hull body `103F42F0..103F568C` never reads that field. Its CSG
test uses literal `IsCsg(0)` (push0 at the calls `103F4533` and `103F45B2`),
and its plane clips/bevels do not use the incoming flags as a replacement CSG
mask. Broader ULevel/actor filtering
remains the collector's separately audited responsibility.

## Captured-value implementation and verification (2026-10-16)

The captured-coordinate service uses integer PC64 arithmetic, not host matrix
operations. PointBy retains Point-Origin differences; VectorBy ignores Origin.
Traversal planes preserve Z/Y/X component execution and Z/Y/X dots, while hull
planes preserve X/Y/Z execution and Y/Z/X dots. For finite numbers the first
Y/Z sum is commutative; a fabricated cancellation difference between those two
literal schedules is not claimed. Tests instead distinguish the actual YZ/X
grouping from incorrect XY/Z grouping, premature Float differences and premature
W-dot spills. Model bounds fold all eight transformed endpoint combinations in
native X/Y/Z corner order, ignoring source validity and preserving first equal
Float bits; finite reversed endpoint pairs are legal in this owner operation.

The owner sweep shares the complete bounded BSP/clip/bevel kernel with the
null-owner sweep. It transforms every primary hull plane before clipping,
flips afterward, retains world extents, skips six bbox planes and returns the
already-world-space normal. The detached caller-packet adapter preserves Next
and Item, leaves empty-model packets untouched, writes only Time2 on misses,
and publishes accepted packets even when backed-off Time clamps to1 and the
native return is clear. Explicit safety failures publish no prefix writes.
The strict finite decoded-geometry admission, literal hull terminator and
fixed work limits remain explicit portable policies, not claimed retail checks.

Captured scalar tests pass104445 checks/271 deliberate nonfinite or overflow
refusals,43652 exact independent hardware-x87 comparisons and16832 matching
finite-store refusals. Owner sweep/packet tests including the owned map pass
12387 checks/23 deliberate refusals and3062 exact independent recursive-x87
sweeps (218 accepted hits). Controls include finite singular/zero axes,
nonuniform/negative/sheared coordinates, all256 CSG flag bytes, a2048-node
iterative chain, tied competing last-parent hulls, accepted-Time1 packet writes
and later-plane overflow before an earlier clip would reject.

The actual Training root Model36 (9524 nodes,3183 hulls) supplies24 additional
captured-fixture-coordinate sweeps,24 exact comparisons and21 hits. This is
real decoded BSP data with explicit test coordinates, not an assertion that
these coordinates came from a live Actor.ToWorld provider. Existing null-owner
regression also passes59971 checks/39 refusals and96/96 exact owned BSP hits.
Map bytes/mtime and original Engine/Core/Engine.u hashes remain unchanged.
The coherent host build and strict ARM64 syntax checks pass. CTest116 passed/
131 registered/15 explicit owned-data skips/zero failures in38.79s. Local
ignored verification logs are `artifacts/owner-model-*-20261016.log`.

## Remaining implementation gates

The current detached implementation accepts an explicitly supplied, already
captured `OriginalModelCoords` value. `quest_model_owner_coords.h` provides
the audited point/vector/plane and model-bounds scalar operations;
`quest_model_extent_trace.h` now includes the owner sweep, and
`quest_model_line_hit.h` supplies the caller-seeded extent packet adapter.
The independent owner test covers supplied translation, rotation,
nonuniform/negative/shear and finite singular/zero coordinate fixtures,
transformed hull flips, owner-unused bbox contents, null-owner-only bbox
clipping, unchanged Next/Item and atomic refusal. Build/test verification above
does not by itself establish live native object authority or gameplay.

Detached native **Actor/ABrush/AMover ToWorld coordinate construction now
exists in `quest_brush_coords.h`**, using explicit native field values and
a supplied16384-Float sine table. ORIGINAL-BRUSH-COORDINATES.md records its
independent numeric/rotation verification. The current live provider and
separate identity/layout/profile evidence are recorded in
ORIGINAL-LIVE-ACTOR-COORDINATES.md, ORIGINAL-TOWORLD-VTABLES.md,
ORIGINAL-BRUSH-NATIVE-OFFSETS.md and ORIGINAL-SINE-PROFILE.md. Collision must
still establish actual primitive lifetime/ownership and capture that current
live ToWorld value once per nonempty Model query.
Supplying guessed identity axes, a host inverse, renderer geometry or the
pinned engine's simpler transform is not that provider. Finite captured
singular/zero matrices are deliberately accepted: the captured-coordinate
operations contain no inverse/determinant gate. Refuse active nonfinite or
overflow arithmetic explicitly, not a merely singular finite captured value.

Runtime integration also remains: the native collision hash must select the
actual current Brush/Mesh/Cylinder primitive and own the captured ToWorld
value for the entire query; an owner Model call must perform the real narrow
phase, not return its registration bounds. Actor/world authority, shared work
budgets, temporary hit lifetime, callbacks and failure rollback must join the
same outer VM transaction. This captured-value implementation does not add
the Model zero-extent branch, full native Move266/FindBase/falling, world startup,
an APK or on-device acceptance.
