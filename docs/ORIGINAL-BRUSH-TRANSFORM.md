# Original moving-brush coordinate/Poly transform (2026-10-30)

Static source contract for the coordinate and bounding-sphere operations
selected by the first Training moving-brush tracker's initial Update. This
document is not a compiler/runtime change, test, startup publication, or APK
acceptance. No original commercial DLL/EXE was loaded or executed. No build
or map test was performed. Only this documentation file was added.

Authority: the read-only owned files beneath
`D:/Steam/steamapps/common/Deus Ex/System`:

| Original file | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |

Addresses are original PE32 virtual addresses and offsets, not portable
object layouts. External MinGW objdump was used only for static reads.
`ORIGINAL-MOVING-BRUSH-TRACKER.md` records the enclosing constructor/Update
passes and still-required actual Model/BSP mutations.

## Existing reusable numerical contracts, not a general matrix inverse

`ORIGINAL-BRUSH-COORDINATES.md` already establishes original FCoords
composition, FCoords*=FScale, FCoords*=FRotator, SheerSnap, scalar PC64
rounding, and the actual sine-table contract. `quest_brush_coords.h` exposes
detached value helpers for those operations. `ORIGINAL-BRUSH-NATIVE-OFFSETS.md`
independently authenticates native `+32C=Engine.Brush.MainScale` and
`+340=Engine.Brush.PostScale`; each is a20-byte Scale struct:
Scale FloatXYZ at0, SheerRate Float atC, SheerAxis byte at10.
Rotation is native `+120` Pitch/Yaw/Roll Int32. PrePivot is native `+180`.

These are inputs to the **different** exported ABrush.BuildCoords operation,
not authority to substitute ABrush.ToWorld, a host matrix inverse, renderer
DrawScale, or a reciprocal-scale approximation.

In the notation below, `F` is an explicit binary32 store; operations between
those stores retain original x87 PC64 precision/nearest-even, as in the
existing detached arithmetic profile. `VectorBy(v,C)` does not consume
C.Origin. For each output component/row-axis it evaluates
`(v.x*axis.x + v.y*axis.y) + v.z*axis.z`, then stores one Float. Generic
`PointBy(v,C)` retains its `v-C.Origin` differences until the final component
stores. The FPoly operation below deliberately has earlier difference
stores and must not silently use that PointBy schedule.

## Actual selected ABrush.BuildCoords call

Tracker Update calls thunk `1030202C -> 1032F9E0` with a nonnull first
FModelCoords output and **null second output** at `1037A6B5..1037A6C5`.
FModelCoords is two48-byte FCoords values: PointXform at0 and
VectorXform/normal transform at30. Each FCoords is Origin Float3 at0,
XAxis Float3 atC, YAxis at18, ZAxis at24.

For that selected first-output branch `1032FA1B..1032FB8A`, BuildCoords:

1. Copy actual GMath.UnitCoords (`GMath+18`).
2. Apply `FCoords*=PostScale` via Engine IAT`10599B4C`.
3. Apply `FCoords*=Rotation` via IAT`10599B48`.
4. Apply `FCoords*=MainScale` via IAT`10599B4C`.
5. Copy all48 bytes of the resulting coordinate value into output+0.
6. Independently copy UnitCoords, apply `FCoords/=MainScale` via IAT`10599B40`,
   then `FCoords/=Rotation` via IAT`10599B3C`, then
   `FCoords/=PostScale` via IAT`10599B40`.
7. Transpose this inverse-sequence coordinate value using explicit original
   vector/coordinate construction, then copy all48 bytes into output+30.

The transpose at `1032FB06..1032FB8A` is exactly:

```
origin = -VectorBy(inverse.origin, inverse)
xAxis  = (inverse.xAxis.x, inverse.yAxis.x, inverse.zAxis.x)
yAxis  = (inverse.xAxis.y, inverse.yAxis.y, inverse.zAxis.y)
zAxis  = (inverse.xAxis.z, inverse.yAxis.z, inverse.zAxis.z)
```

VectorBy has its three Float output stores. Unary negation is imported Core
FVector operator-, thunk`10102AE0 -> 10117920..1011793C`: FCHS followed by
Float XYZ stores, preserving signed-zero distinctions. The four-vector
FCoords constructor is thunk`10102AF4 -> 10118990`; it copies the values.
This is not permission to manufacture a positive-zero normal-transform Origin
or drop its evaluation merely because FPoly's later VectorBy ignores Origin.

The second-output branch is skipped by this actual initial Update call.
Its complete nonnull behavior was not audited here and is not claimed.
BuildCoords does not read Location or PrePivot on this selected branch;
the later FPoly.Transform receives those inputs separately.

### Returned orientation is a sign product

At `1032FD13..1032FD79`, the selected call returns:

```
mainProduct = PC64((MainScale.Scale.z * MainScale.Scale.y) * MainScale.Scale.x)
mainSign = mainProduct > 0 ? +1 : mainProduct < 0 ? -1 : 0
postSign = F(PostScale.Orientation())
return PC64(FILD(mainSign) * postSign)
```

Both comparisons use original zero constants, not an epsilon. Main's
three-factor product is not Float-spilled before its sign tests.
Core FScale.Orientation, thunk`1010159B -> 10118880..101188C8`, uses the same
`(z*y)*x` retained product and sign tests and returns FILD of -1/0/+1.
Neither SheerRate nor SheerAxis contributes to this returned sign.
The tracker Float-stores the return at `1037A6C5` before passing it onward.
It is **not determinant magnitude**. Main sign0 times negative post sign
can produce negative zero, which is not less than zero in the later test.

## Newly required original inverse coordinate operations

### FCoords/=FScale

Engine IAT`10599B40` names Core FCoords::operator/=(FScale const&), export
thunk`10102BBC -> 1011C860..1011CBDF`. Its exact order differs from *=Scale:

1. Float-divide all nine axis components by matching Scale components,
   XAxis XYZ, then YAxis XYZ, then ZAxis XYZ (`1011C871..1011C8D1`).
2. Float-multiply Origin XYZ by Scale XYZ (`1011C8D4..1011C8E9`).
3. Compute original SheerSnap of the captured Float SheerRate, using the
   same strict Double thresholds/constants as the existing forward contract.
4. Start a separate UnitCoords shear temporary. For axis1..6, FCHS the
   retained snapped value **before its one Float coefficient store**:

| SheerAxis | Inverse shear coefficient |
| --- | --- |
| 1 | XAxis.y |
| 2 | XAxis.z |
| 3 | YAxis.x |
| 4 | YAxis.z |
| 5 | ZAxis.x |
| 6 | ZAxis.y |
| 0 or7..255 | discard snap, retain UnitCoords |

5. Fully compose the already-divided/scaled receiver with that shear
   temporary: Origin through PointBy, then XAxis/YAxis/ZAxis through
   VectorBy, copying each complete Float vector before the next
   (`1011C9C3..1011CBDF`). No identity/zero-angle shortcut is evidenced.

Preserve the source FCHS-before-coefficient-store schedule, including active
zero snapped coefficients becoming negative zero. A port must not implement
this operation by calling the
existing forward scale helper with Float reciprocal scale components:
original divides before shear, has separate Float divisions, and scales
Origin before the final shear composition. Zero denominators reach active
original division; a bounded finite port should explicitly refuse unsupported
nonfinite results rather than inventing a replacement transform.

### FCoords/=FRotator

Engine IAT`10599B3C` names Core FCoords::operator/=(FRotator const&), thunk
`1010314D -> 1011B690..1011BA9E`. It constructs and composes three separate
positive-zero-Origin coordinate temporaries in this order:

| Stage | XAxis | YAxis | ZAxis |
| --- | --- | --- | --- |
| inverse Roll | `(1,0,0)` | `(0,c,s)` | `(0,-s,c)` |
| inverse Pitch | `(c,0,-s)` | `(0,1,0)` | `(s,0,c)` |
| inverse Yaw | `(c,-s,0)` | `(s,c,0)` | `(0,0,1)` |

Each stage transforms Origin with PointBy and every axis with VectorBy,
including their actual Float stores. Roll consumes Rotator+8, Pitch+0,
Yaw+4. The original16384-Float sine table has index
`SAR(angle,2)&0x3FFF`; cosine uses wrapping `angle+0x4000` first. Negative
coefficients are FCHS of the looked-up sine followed by Float stores.
This is not a call to the forward helper with `(-Pitch,-Yaw,-Roll)`:
stage order differs, and negating an unquantized integer before table lookup
can change the low-bit quantization. All three stages execute even for0.

## Exact FPoly.Transform operation

Export thunk`10301280 -> 10387360..10387754` takes FModelCoords,
PreSubtract, PostAdd and Float orientation. Actual initial Update passes
PreSubtract=mover.PrePivot, PostAdd=mover.Location and the Float-spilled
BuildCoords sign. The coordinate packet and actor fields are separate from
the copied scratch polygon; arbitrary aliasing is not claimed by this audit.

The native FPoly layout and full serialized fields are in
`ORIGINAL-MOVING-BRUSH-TRACKER.md`. Transform mutates, in this exact order:

1. `TextureU = VectorBy(oldTextureU, coords.VectorXform)`
   (`10387381..1038740E`).
2. `TextureV = VectorBy(oldTextureV, coords.VectorXform)`
   (`10387411..10387491`).
3. Transform Base (`10387494..10387543`).
4. Transform each active vertex in increasing index order
   (`10387546..10387619`).
5. If Float orientation compares strictly below Double0 at`10429838`,
   reverse the complete active vertex order (`1038761E..1038769E`).
6. `Normal = Core.SafeNormal(VectorBy(oldNormal, coords.VectorXform))`
   (`103876A0..10387746`). No extra orientation sign is applied to Normal.

The Base/vertex pipeline has three separately stored stages:

```
delta       = (F(old.x-pre.x), F(old.y-pre.y), F(old.z-pre.z))
transformed = VectorBy(delta, coords.PointXform)  // three final Float stores
result      = (F(transformed.x+post.x), F(transformed.y+post.y),
               F(transformed.z+post.z))
```

The **subtractions are Float-spilled before matrix multiplication**. The
point transform's Origin is not consumed here; using generic PointBy,
an affine fused expression, or a host matrix can change cancellation,
signed-zero and rounding behavior. Base computes/temporarily holds some
components in Z/Y/X order, but all deltas and all VectorBy components are
stored before the next stage. Vertices perform the same three-stage values.

Mirrored reversal swaps `vertices[i]` with `vertices[NumVertices-1-i]`
for `i < NumVertices/2`, using Float bit copies. It is the whole reversal,
not a fan-preserving reversal of only indices1..N-1. An orientation of either
zero sign does not select reversal. The stored old Normal remains untouched
until the final step, after both texture vectors, Base and vertices.
No PolyFlags, Actor/Texture/FName references, count, links, or pans are changed.

Core.SafeNormal is the already-audited import`10599B80`, thunk
`101031C0 -> 10144BE0..10144C60`, not a host normalizer. It retains
`(x*x+y*y)+z*z`, stores its Double square, compares the still-retained value
against Double1e-8, returns exact positive-zero XYZ below threshold, otherwise
uses appSqrt of the stored Double and retained reciprocal/root to scale XYZ
into final Floats. There is no length==1 shortcut or Double root reload.
See `ORIGINAL-ACTOR-LINE-TRACE.md` for its existing complete contract.

## Exact FSphere(points,count) used before the BSP prefilter

Engine import`10599E80` names Core FSphere(FVector const*,int), Core export89
thunk`10102D38 -> 10144960..10144A7D`. This is an AABB-center sphere, not
a minimal enclosing/Ritter sphere. It uses runtime FBox constructor export15,
thunk`10101618 -> 10144AB0..10144BD2`.

1. Initialize all four sphere Floats (center XYZ and radius) to positive zero.
   Count==0 returns immediately (`10144983..10144998`).
2. For positive count, initialize AABB Min/Max by bit-copying the first
   vertex, then visit later vertices in input order. Strict oldMin>new and
   oldMax<new comparisons preserve prior equal values, including signed zero.
3. Compute center using the following **asymmetric spill schedule**
   (`101449AB..101449FC`):

```
center.x = F(F(min.x+max.x) * Float(0.5))
center.y = F(F(min.y+max.y) * Float(0.5))
center.z = F(PC64(min.z+max.z) * Float(0.5))
```

   X/Y sums are Float-spilled before multiplication; the Z sum is not.
   Replacing this with `(max-min)*0.5+min` or uniformly evaluated vector math
   changes the original rounding/overflow schedule.
4. Initialize a stored Float squared radius to positive zero. For each
   point, retain `dx=center.x-point.x` and likewise dy/dz, then evaluate
   `(dy*dy + dx*dx) + dz*dz` in PC64. There are no delta/product Float
   spills. Compare the retained candidate strictly greater than the current
   stored Float maximum; Float-store only a greater candidate
   (`10144A05..10144A4A`).
5. Convert the stored maximum to a Double argument and call appSqrt,
   thunk`101032BA -> 10123290`: FLD Double, FSQRT, RET.
6. Multiply the retained root by exact Double1.001, bits
   `3FF004189374BC6A` at`10178070`, then Float-store radius
   (`10144A55..10144A66`). The non-popping Double FST at`10144A5A` is an
   observational spill, **not** a reload/rounding boundary for that multiply.

The native count-zero test is equality, not <=0. For a negative count the
runtime FBox remains zero/invalid, the signed point loop skips, and the later
square-root/multiply path produces zero without point dereference. The
tracker's admitted nonnegative polygon vertex sum does not require inventing
that negative-count input, but the native behavior is not a rejection.

## Integration boundary

The static audit above establishes selected source values and arithmetic
schedules, not runtime publication. The implementation and separate new tests
below now cover the selected coordinate/polygon/sphere operations; earlier
ToWorld tests alone did not prove these different Float boundaries. Live Model
geometry, actual UPolys writes and initial BSP insertion still require the
existing caller journal and retained startup guard. No other-map integration
is required for this increment.

## Implemented selected value operations and actual field bridge

`quest_brush_poly_transform.h` now implements inverse FScale and inverse
Roll/Pitch/Yaw, the selected first BuildCoords output/transpose/orientation,
full copied FPoly transform/reversal/SafeNormal and the allocation-free sphere
operation. Existing integer PC64 primitives implement the evidenced arithmetic;
no host matrix inverse, reciprocal-scale substitute, host sqrt/trig, fused
point pipeline or bounding-box geometry substitute was introduced. Metadata
and unused fixed vertex slots retain their bits. Unsupported nonfinite active
numeric data/division/output domains refuse explicitly; inactive fields and
unknown-axis discarded shear results do not create invented reads.

The runtime `NativeBrushBuildCoords` is a **direct ABrush method** bridge.
It validates the complete bounded original class chain and actual RF_Native
Engine.Brush base, independently of virtual ToWorld selection. Rotation and
MainScale/PostScale use existing exact declaring-property flags, dimensions,
canonical Vector/Rotator/Scale types and field layouts. The selected port-owned
sine numeric profile supplies values; no original GMath UObject, DLL execution,
native default authority or original mutable global lifecycle was fabricated.
`portable_brush_transform.h` exposes only a copied diagnostic packet from an
actual indexed current-map Actor. Generic Actors, class/CDO/non-slot/stale
identities are not accepted as Brushes because they have a Brush primitive.

The new standalone `quest_brush_poly_transform_test` independently expresses
hardware x87 PC64 arithmetic and FSIN/FSQRT instructions. It passes28351 checks,
12735 exact comparisons,2553 matching numeric refusals,31 explicit refusals
and4 effective rounding controls. Controls distinguish reciprocal versus true
division, early point-delta stores, VectorBy/PostAdd stores and asymmetric
sphere midpoint overflow. The finite50000-sample retained-vs-Double-root
search did not find a Float witness and is explicitly informational, not a
claimed proof or a required invented failure. The test's independently built
table is a supplied numeric value, not an initialized original GMath instance.

Extended owned `portable_actor_coords_test` passes89192 checks/325 identity
refusals over the first Training's1308 Actors,1017 Brushes and25 Movers.
Every Brush's two coordinate packets and orientation agree with the separately
tested scalar helper using original declared fields. All25 actual non-static
Mover Brush Models resolve their serialized local Polys sources:150 polygons
and600 vertices transform successfully, preserve nongeometry fields, and fit
their computed spheres. This is composed source-data/adapter evidence, not an
independent arithmetic oracle or live BSP publication. Complete checkpoint,
world revision/slots/native time/GC graph and17 original source fingerprints
remain unchanged, with clean GC after shutdown.

Strict ARM64 and Android-native compilation pass. Wider final regression and
frozen-input/original-file audit results are recorded in STATUS.md. Initial
desktop verification stopped on a range-loop-copy warning in the owned test;
that failure remains recorded, was corrected with a const reference, and is
not a passing gate. No compiler inputs changed during running builds/tests.

These helpers do **not** install a tracker, rewrite live polygon LinkIndex,
append world pools/surfaces, update collision proofs or finish original startup.
Actual Model/UPolys/GC ownership, journaled mutation, sphere prefilter, splitting,
BSP insertion, update/removal/teardown and current-revision query/render views
remain next. The post-partition guard and accepted VR layouts are unchanged;
there is no new APK or live crate/loot/gravity acceptance from this batch.

## Bounded static reproduction

```powershell
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1032f9e0 --stop-address=0x1032fb8c 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1032fd13 --stop-address=0x1032fd7e 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x10387360 --stop-address=0x10387755 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1011c860 --stop-address=0x1011cbe2 'D:/Steam/steamapps/common/Deus Ex/System/Core.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1011b690 --stop-address=0x1011ba9f 'D:/Steam/steamapps/common/Deus Ex/System/Core.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x10144960 --stop-address=0x10144bd3 'D:/Steam/steamapps/common/Deus Ex/System/Core.dll'
```
