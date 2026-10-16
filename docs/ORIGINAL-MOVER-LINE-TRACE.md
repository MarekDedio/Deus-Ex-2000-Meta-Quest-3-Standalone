# Retail owner/brush Model line checks

Static, read-only audit of the owned GOTY installation, 2026-10-16. No original
DLL or EXE was executed. This establishes further collision contracts, not an
implemented owner sweep, native Move266, FindBase, falling or live crate gameplay.

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

`UMesh.LineCheck` thunk `1030259F` reaches `103A6D10`. It compares all three
Extent Float components with zero. If **any** is nonzero it forwards the exact
Hit, Owner, End, Start, Extent and flags to `UPrimitive.LineCheck`
(`10301B9F` → `103D8AF0`), returning that result at `103A6DCE`. Only an exact
zero-extent call takes the mesh-specific continuation at `103A6DD1`.

Consequently an ordinary crate Mesh with a nonzero moving collision extent
uses the primitive/cylinder path, not animated triangle collision. This does
not authorize using a cylinder for an actual Brush primitive. The complete
UMesh zero-extent path and UPrimitive cylinder numeric contract are separate
work; this audit does not claim them implemented.

## Owner coordinates are a captured virtual result

`UModel.LineCheck` is `10303E5E` → `103F3C20`–`103F4184`.
An empty Model returns RootOutside before checking extents or Owner. The
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
The property identities MainScale/PostScale are present in original Engine.u;
their exact native-offset-to-name registration is not independently completed
by this audit. Keep the two native blocks distinct until that mapping is
verified; do not copy a pinned engine's MainScale-only transform as retail proof.
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
for these coordinate-building operations remain required.

## Two different owner-plane transforms

The traversal and hull stages use different original arithmetic. They must
not share one pretransformed plane cache unless both distinct results survive.

### BSP traversal plane

At `103F4320`–`103F43A8`, nonnull Owner transforms each current BSP plane:

- Dot authored normal against captured Coords XAxis, YAxis and ZAxis using
  imported FVector dot (`1059A0AC`, Core `101017AD` → `10117710`). Each
  returned component is **Float-stored**.
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

All operations between indicated stores retain original PC64 precision.
In particular the Y/Z/X ordering differs from traversal's imported Z/Y/X
dot. Nearly cancelling nonuniform transforms can distinguish the two.

Only **after** this transform does raw hull reference bit `40000000` invoke
FPlane.Flip (`1059A114`), negating all four Float coefficients
(`103F469D`–`103F46D1`). Component sign masks and bevels consume the resulting
transformed/flipped planes, not untransformed normals. Flip-before-transform
may look geometrically equivalent but is not the audited numeric order.

## Owner hull clipping and outputs

The original owner branch skips the six extra axis-aligned LeafHulls bbox
planes entirely (`103F495D`–`103F4962` jumps directly to `103F51AF`). They are
**null-owner only**. World box support/pushout still uses the original world
Extents against transformed world planes. The ordered original planes,
64-plane limit, strict interval comparisons and axis bevel chain otherwise
remain those already audited for the null-owner kernel.

On accepted hull, `103F564A`–`103F567F` stores Time, the selected **already
world-space Normal**, Actor=Owner, Primitive=Model and the helper-hit flag.
It does not write FCheckResult.Item. The extent wrapper then applies its
existing backoff and computes world Location from original world Start/End
and stored Time. It performs **no second normal transformation or direction
facing correction**. These normal postprocessing operations in the wrapper's
zero-extent path must not be imported into the extent branch.

Trace flags are copied into FBoxCheck, but the inspected extent traversal's
CSG test still uses literal `IsCsg(0)`, and its plane clips/bevels do not use
the incoming flags as a replacement CSG mask. Broader ULevel/actor filtering
remains the collector's separately audited responsibility.

## Remaining implementation gates

Implement/verify captured native coordinates before adding a Model-owner
adapter. At minimum test rotation, translation, PrePivot, both scale blocks,
nonuniform/negative scale, sheer modes, transformed hull flip order, traversal
versus hull cancellation, null-owner-only bbox planes and unchanged Item.
Bound and reject nonfinite/singular coordinate arithmetic explicitly; do not
invent a unit axis or use a fake empty Model on failure.

The collision hash may use transformed brush bounding boxes to collect
candidates; actual owner Model.LineCheck must still perform this narrow phase.
Owner state and primitive selection must come from current native actor
identity and actual fields, with world mutation/callbacks sharing the outer
VM transaction. No source changes, APK, on-device acceptance or completed
movement/startup behavior are established by this document.
