# Original Actor and Brush ToWorld coordinates

Read-only static audit of the user-owned Deus Ex GOTY binaries, 2026-10-16.
No original DLL or EXE is loaded or executed. Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Core.dll SHA256 is
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Preferred PE32 virtual addresses below are original evidence locations, not
portable actor layouts or permission to execute the commercial binaries.

This consolidates the independent Actor/rotator and Brush/FScale audits.
The current `native/quest_brush_coords.h` implements detached scalar APIs from
this contract; build and independent test results are recorded below.
It is not a native identity/property bridge, Model point trace,
Move266, FindBase, falling, original world startup or headset acceptance.

## Actual native virtual identities

Each vtable and both slots were inspected, not guessed from a class name or
the pinned reference engine's inheritance:

| Actual exported vtable | ToLocal, slot+84 | ToWorld, slot+88 |
| --- | --- | --- |
| AActor `104268DC` | `10303369` → `1030D750` | `10303AC6` → `1030DAB0..1030DE70` |
| ABrush `10426C9C` | `103026A3` → `1030EF90` | `103023AB` → `1030F310..1030F4F6` |
| AMover `10426E08` | same ABrush entry | same ABrush entry |

A live native adapter must establish the actual UObject/UClass/virtual
dispatch identity. Choosing a generic Actor transform for a Mover based only
on its visible mesh, or choosing a brush path because a name contains Mover,
does not establish the operation. Model extent FBoxCheck captures the actual
owner ToWorld result once; its use is documented in ORIGINAL-MOVER-LINE-TRACE.

FCoords is48 bytes: Float3 Origin, then Float3 XAxis, YAxis and ZAxis. The
Core GMath constructor `10144130..10144386` explicitly initializes UnitCoords
at GMath+18 with positive-zero Origin and identity axes. It is not a matrix
inverse inferred by a host graphics library.

## Composition and Float stores

Engine IAT `10599B20` names Core `FCoords::operator*=(const FCoords&)`, thunk
`101027F7` → body `1011A710..1011A91E`. It mutates the receiver in this order:

1. Origin = oldOrigin.TransformPointBy(Other), final Float XYZ copies at
   `1011A787..1011A79F`.
2. XAxis = oldXAxis.TransformVectorBy(Other), copies at `1011A806..1011A81C`.
3. YAxis = oldYAxis.TransformVectorBy(Other), copies at `1011A883..1011A899`.
4. ZAxis = oldZAxis.TransformVectorBy(Other), copies at `1011A900..1011A917`.

PointBy first subtracts Other.Origin from all three components, retaining the
PC64 differences. VectorBy does not subtract Origin. For each output row-axis
the arithmetic is `(dx*Axis.x + dy*Axis.y) + dz*Axis.z`, with retained PC64
products/additions and one final Float component store. There are no earlier
Float product or point-difference stores. Each complete result vector is
copied back before the next source vector is transformed.

The native Other pointer is reread for each stage. General in-place aliasing
Other==receiver can therefore observe earlier writes. The audited ToWorld
callers supply **separate constructed coordinate temporaries**, so the pure
`ComposeOriginalModelCoords(lhsValue, rhsValue)` contract deliberately models
that nonalias value case rather than claiming an arbitrary native alias API.

All scalar arithmetic uses ordinary x87 PC64 nearest-even; marked Float
stores are IEEE binary32. A host-double matrix multiply/inverse or regrouped
affine transform is not this storage schedule. The portable helper uses
integer PC64 arithmetic, not host long-double width.

## Generic Actor ToWorld and ordered rotation

Generic Actor.ToWorld reads Location at native+114/+118/+11C and Rotation at
+120/+124/+128. It copies UnitCoords, then Float-stores each
`Origin.component - Location.component`. In particular positive0 minus
positive0 is positive0; this must not be replaced by unary negation of
Location, which would produce negative0.

It composes yaw, then pitch, then roll coordinate temporaries. The same
ordered operation is in Core FCoords*=FRotator, Engine IAT `10599B48`, Core
thunk `10101596` → `1011AC80..1011B08C`. Each temporary has positive-zero
Origin and these axes, with s/c from the original lookup table:

| Stage | XAxis | YAxis | ZAxis |
| --- | --- | --- | --- |
| Yaw | `(c,s,0)` | `(-s,c,0)` | `(0,0,1)` |
| Pitch | `(c,0,s)` | `(0,1,0)` | `(-s,0,c)` |
| Roll | `(1,0,0)` | `(0,c,-s)` | `(0,s,c)` |

The three complete compositions and all their Float copies execute even at
zero angles. They are not replaced by one closed-form Euler matrix or an
identity-angle shortcut. Native integer angles wrap at32 bits; lookup index
is `SAR(angle,2) & 0x3FFF`. Cosine uses wrapping `angle+0x4000` before that
index operation. Equivalently unsigned wrapping addition followed by
`(bits>>2)&0x3FFF` avoids C++ signed overflow while retaining every relevant
low bit.

## Original sine-table provenance

The table is a process GMath value, not an invocation of portable libm in
every transform. Core `1014431E..10144346` builds16384 Float entries at
GMath+8C in increasing integer index order:

```
angle = StoreDouble(PC64(FILD_int32(index) * DoubleConstant))
entry = StoreFloat(appSin(angle))
```

DoubleConstant at `10178040` has exact bits `3F3921FB54442D18`.
The Double argument store occurs at `1014432B`; Core appSin thunk `101033A0`
reaches `10123240..10123246`, which is FLD the stored Double, **FSIN**, RET.
The retained x87 result is Float-stored into the table at `10144333`.
This has a real Double angle-rounding boundary before the transcendental.

`std::sin`, `std::cos`, a different pi expression, a host libm table or manually
forced quarter-turn zeros has not been established bit-equivalent. A portable
coordinate builder must consume an explicitly supplied original table value
with genuine lifecycle/provenance, or separately prove an exact table
generator. Independent x86 hardware FSIN test construction does not load or
run the original DLL, but is not by itself an Android generator or proof that
a runtime table was captured from the actual original GMath instance.

The table constructor itself does not establish its incoming x87 control
word; its static initialization reaches `10144130` through `10105BA0`.
Although the detached arithmetic/rotation profile explicitly uses PC64 nearest,
that does not prove the original CRT static-initializer precision state.
The independent test generates every entry under both nearest PC53 and PC64
on the current GenuineIntel i5-3470 (CPUID signature306A9); all16384 final
Float entries match between those two measured profiles. This observation is
not a universal cross-CPU FSIN guarantee or an initialized Android table.

## Brush ToWorld exact outer order

ABrush.ToWorld `1030F310..1030F4F6` reads Location, Rotation, PrePivot at
+180/+184/+188 and complete FScale blocks at native+340 and+32C. Their
MainScale/PostScale property names exist in the original packages, but this
audit has not independently bound each name to its registered native offset.
The source/API therefore intentionally names both blocks by **offset**.
DrawScale is not read in this body.

The complete operation is:

1. **First**, negate PrePivot XYZ with FCHS and Float-store the three negative
   components (`1030F31F..1030F341`). Retain those stored values until step7.
2. Copy UnitCoords and Float-store Origin minus Location XYZ
   (`1030F353..1030F389`), exactly as the generic Actor translation.
3. Apply Core FCoords*=FScale with the complete native **+340** block
   (`1030F38F..1030F39D`).
4. Apply Core FCoords*=FRotator to actual Rotation, yaw/pitch/roll in order
   (`1030F3B9..1030F3C7`).
5. Apply the **+32C** block's independently built sheer coordinates and full
   FCoords composition (`1030F3CD..1030F43A`).
6. Multiply all axis components by that block's XYZ scale, and separately
   divide Origin XYZ by the scale (`1030F440..1030F4B5`). Steps5/6 reproduce
   the same FScale operation that Core performs for the earlier block.
7. Float-store each Origin minus the previously stored **negative** PrePivot
   component (`1030F4B9..1030F4E6`), then copy all48 output bytes.

Replacing the final subtraction with a regrouped Location/pivot expression,
moving PrePivot negation to the end, or collapsing the scale/rotation stages
changes the evidenced stores or signed-zero/refusal schedule.

## FScale operation and SheerAxis

Engine IAT `10599B4C` names Core FCoords*=FScale, thunk `1010256D` →
`1011C0A0..1011C3B8`. The FScale value has Scale Float3 at+0, SheerRate Float
at+C and SheerAxis byte at+10. Native tail padding is not another scalar.

First compute the original SheerSnap, start a separate UnitCoords temporary,
and store the snapped coefficient in one of these fields:

| SheerAxis byte | Shear coordinate coefficient |
| --- | --- |
| 1 | XAxis.y |
| 2 | XAxis.z |
| 3 | YAxis.x |
| 4 | YAxis.z |
| 5 | ZAxis.x |
| 6 | ZAxis.y |
| 0 or7..255 | None: leave UnitCoords and discard snapped result |

Then perform the full nonalias FCoords composition with this shear temporary,
updating Origin then XAxis/YAxis/ZAxis as described above. Do not skip it for
axis0, zero snap, unit scale or apparent identity coordinates.

At `1011C34A..1011C398`, independently Float-store all nine componentwise axis
products: XAxis.x*=Scale.x, XAxis.y*=Scale.y, XAxis.z*=Scale.z; then the same
XYZ products for YAxis, then for ZAxis. This is not one scalar per axis.
Finally Float-store Origin.x/=Scale.x, Origin.y/=Scale.y, Origin.z/=Scale.z
at `1011C39B..1011C3B2`. The receiver contains each completed preceding write.
The +32C inlined version uses Engine IAT `10599B44`, Core FVector*=FVector
thunk `10102135` → `10117A90..10117AC4`, for its second/third vectors;
that operator mutates its receiver and returns a Float3 copy.

## Exact original SheerSnap

Engine helper thunk `10303E68` → `1030F240..1030F2DA` and Core's inlined
`1011C0B5..1011C148` use the same strict comparisons and constants:

```
if      s < Double(-0.65): s + Double(0.15)
else if s > Double( 0.65): s - Double(0.15)
else if s < Double(-0.55): Float(-0.5)
else if s > Double( 0.55): Float( 0.5)
else if s < Double(-0.05): s + Double(0.05)
else if s > Double( 0.05): s - Double(0.05)
else:                     Float(+0)
```

Input s is stored Float; the selected sum/difference remains PC64 until the
shear coefficient's Float store. The comparisons are not against rounded
Float thresholds. For example `Float(0.05)` is above Double0.05 and produces
a tiny positive result, rather than the dead-zone zero.

| Constant | Exact Double bits | Engine VA | Core VA |
| --- | --- | --- | --- |
| -0.65 | BFE4CCCCCCCCCCCD | 10426DB8 | 10176678 |
| +0.65 | 3FE4CCCCCCCCCCCD | 10426D98 | 10176658 |
| -0.55 | BFE199999999999A | 10426D88 | 10176648 |
| +0.55 | 3FE199999999999A | 10426D78 | 10176638 |
| -0.05 | BFA999999999999A | 10426D68 | 10176628 |
| +0.05 | 3FA999999999999A | 10426D58 | 10176618 |
| +0.15 | 3FC3333333333333 | 10426DA8 | 10176668 |

Negative comparisons test x87 AH mask01; strict-greater branches test mask41
and take the zero-mask branch. Float half constants have bits BF000000 and
3F000000. Default/invalid SheerAxis values are a legitimate no-shear case,
not a retail enum refusal. Native computes Snap then discards it there;
the detached helper explicitly omits that unused FP result/status, including
unused nonfinite rate evaluation, while still performing the identity
composition and all active products/divisions.

## Supported domain, API and remaining work

The native coordinate operations contain no orthonormality, determinant or
positive-scale gate. Finite negative, nonuniform and shear scales are not
normalized or replaced. Zero scale of either sign reaches an actual active
division, including0/0 when Origin is zero; the portable bounded domain must
explicitly refuse that nonfinite result. Tiny nonzero scales are supported
unless a real required Float store overflows. Finite singular captured
coordinate values are not an invented inverse failure.

The verified detached source exposes `OriginalBrushScale`,
`OriginalBrushRotation`, explicit16384-Float `OriginalBrushSinTable`,
`ComposeOriginalModelCoords`, `SnapOriginalBrushSheer`,
`ScaleOriginalModelCoords`, `BuildOriginalActorCoords` and
`BuildOriginalBrushCoords`. It takes values, never original binary pointers,
and keeps the two scale blocks as `scale340` and `scale32c`.

## Verification checkpoint (2026-10-16)

- Compose/Scale/Snap:78481 checks,141 explicit domain refusals,35409 exact
  independent hardware-x87 comparisons and7239 paired finite-math refusals.
  Tests cover all256 SheerAxis bytes, exact Double-threshold adjacent Floats,
  signed-zero products, finite singular coordinates, retained PointBy deltas,
  inactive unknown-axis NaNs and active product/division overflow.
- Rotation:104489 checks,54396 exact independent x87 Actor cases,768 low-bit
  quantization aliases and12 explicit refusals. Every16384 phase is exercised
  for all three axes, plus full-range signed/wrapping angles, synthetic tables,
  mixed angles and128 unit-scale brush/PrePivot cases. The separate test's
  FSIN table synthesis calls no production numeric helpers or original code.
- Builder wiring:42 checks/3 refusals with explicit fixture table values,
  closed-form noncommuting scale/rotation/scale results, PrePivot, shear chains,
  eight-corner bounds and constructed-coordinate Model hit integration. It
  does not replace the independent scalar/rotation oracles.
- Targeted host tests, coherent full host build and strict ARM64 syntax checks
  pass. CTest119 passed/134 registered/15 explicit owned-data skips/zero
  failures in38.35s. Ignored local logs: `artifacts/brush-coords-*-20261016.log`.

These results prove the supplied-value coordinate service, not live native
identity, LUT initialization, world movement or on-device crate physics.

Still required for native runtime integration: genuine object/class/virtual
dispatch authority; validated native-offset-to-reflected-property mapping;
the actual original lookup-table lifecycle/provenance; one captured ToWorld
value for each Model query; current Brush/Mesh/Cylinder primitive selection;
and shared VM transaction/work/rollback guarantees. These pure coordinate
values and registration bounds must not be promoted to final brush collision
or a successful original world-startup claim.
