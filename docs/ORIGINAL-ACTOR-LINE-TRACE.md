# Original actor cylinder line trace audit

Read-only static audit of the owned Deus Ex GOTY binaries, 2026-10-16. No
original DLL or EXE is loaded or executed. Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Core.dll is `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Addresses below are preferred PE32 VAs, not portable object layouts.

This contract is a prerequisite for real actor/world line queries. It does
not establish ULevel.MoveActor, actor-hash parity, FindBase, falling physics,
original world startup, or playable crate destruction. The existing pinned
actor-overlap registry is not the retail swept actor query.

## Entry points and scope

| Operation | Thunk | Body |
| --- | --- | --- |
| UPrimitive.LineCheck | `10301B9F` | `103D8AF0..103D9222` |
| ULevel.SingleLineCheck | `103033B4` | `1039AED0..1039B13B` |
| ULevel.MultiLineCheck | `10302AA9` | `1039B220..1039B7B1` |
| FCollisionHash.ActorLineCheck | `10301348` | `10359160` |
| UMesh.LineCheck | `1030259F` | `103A6D10` |
| UModel.LineCheck | `10303E5E` | `103F3C20..103F4186` |

The complete scalar cylinder contract below is specifically **UPrimitive**
LineCheck. The native method takes Hit, Owner, End, Start, Extent, TraceFlags.
It never reads TraceFlags, the primitive object's fields, owner Rotation,
Mesh, Brush, flags, or class. A null Owner returns clear immediately and
does not read endpoints/extents or change any caller hit fields
(`103D8B0D..103D8B32`). Actual runtime primitive selection remains the caller's
responsibility; do not force every brush or mesh through this cylinder.

Further2026-10-17 audit closes actual UMesh/ULodMesh virtual dispatch: their
LineCheck wrapper uses this UPrimitive implementation on **both** branches,
canonicalizing all extent DWORDs to+0 only when every zero/unordered comparison
has C3=1. This does not apply to UModel or unknown subclasses, and does not
create a primitive object. See ORIGINAL-NATIVE-PRIMITIVE-DISPATCH.md for full
vtable/registration and lazy Mesh-versus-Engine fallback evidence.

Use mutation-style detached hit storage to retain partial writes on clear
returns. The relevant retail FCheckResult fields are Next at0, Actor at4,
Location at8..10, Normal at14..1C, Primitive at20, Time at24, Item at28.
The cylinder never writes Next or Item. It does **not** initialize the complete
hit before its geometric tests. Clear can leave a new Normal with all other
seeded fields unchanged.

## Arithmetic notation and expanded bounds

All arithmetic instructions use ordinary x87 PC64 nearest-even. `F(v)` below
means an explicit IEEE Float store; `D(v)` means an explicit Double store.
Unmarked intermediates stay PC64, even after an FST which does not pop ST0.
No host long-double width, epsilon, generic quadratic or ellipse substitutes.

Let O be owner's stored Location, R its CollisionRadius and H its
CollisionHeight; S is Start, E is End and Q is Extent. Bounds are constructed
in this order (`103D8B35..103D8C9D`):

1. `height=F(Q.z+H)`.
2. `radiusY=F(Q.y+R)`.
3. Retain `radiusXRetained=Q.x+R`, and separately store
   `radiusX=F(radiusXRetained)` without reloading it.
4. X upper bound is retained `radiusXRetained+O.x`; X lower is retained
   `O.x-radiusX` using the Float radius. Neither bound is Float-stored.
5. Y upper/lower are retained `radiusY+O.y` / `O.y-radiusY`.
6. Z upper/lower are separately stored
   `upperZ=F(height+O.z)` and `lowerZ=F(O.z-height)`.

The radial quadratic uses radiusX only. Q.y affects Y broadphase, not an
elliptical narrow phase. Ordinary nonnegative finite dimensions are the
portable supported domain; a refusal policy must be explicit and must not
silently clamp or replace unsupported dimensions.

Broadphase clear-return order is upperX, lowerX, upperY, lowerY, upperZ,
lowerZ. X/Y reject when both endpoints are strictly above upper or strictly
below lower. Z likewise rejects only when both endpoints are strictly above
upperZ or both strictly below lowerZ. Exact cap equality is not rejection. These tests have
no caller-hit writes. Preserve short-circuit input access where a field has
not yet been used; in particular null Owner is not permission to validate a
fabricated cylinder or reject unused NaNs.

## Z slab clipping and partial Normal writes

Initialize local Float Enter=0 and Exit=1, without changing caller Hit.Time.
For both quotients below the subtraction and division remain PC64; only an
accepted interval assignment Float-stores the quotient.

Upper slab (`103D8CD2..103D8D84`):

- If `S.z>upperZ && E.z<upperZ`, calculate
  `t=(upperZ-S.z)/(E.z-S.z)`. If retained `t>0`, store Enter=F(t) and write
  caller Normal=(+0,+0,+1).
- Otherwise, if `S.z<upperZ && E.z>upperZ`, calculate that same quotient.
  If `1>t`, store Exit=F(t). Normal is unchanged.

Lower slab (`103D8D87..103D8E22`):

- If `S.z<lowerZ && E.z>lowerZ`, calculate
  `t=(lowerZ-S.z)/(E.z-S.z)`. If retained `t>Enter`, store Enter=F(t) and write
  caller Normal=(+0,+0,-1).
- Otherwise, if `S.z>lowerZ && E.z<lowerZ`, calculate the same quotient.
  If `Exit>t`, store Exit=F(t).

If stored Enter>=stored Exit, return clear at `103D8E22..103D8E44`, retaining
any preceding Normal write. There is no normal reset on this failure.

## Radial quadratic and special initial-overlap branch

At `103D8E47..103D8EB2`, retain `px=S.x-O.x`, `py=S.y-O.y`.
Retain `dx=E.x-S.x`, `dy=E.y-S.y`, and FST their Float copies DX,DY **without
popping/reloading** before the polynomial:

```
A = F(dy*dy + dx*dx)
B = F(2 * (dy*py + dx*px))
C = F((py*py + px*px) - radiusX*radiusX)
Discriminant = F(B*B - (C*A)*Double4)
```

There is no Float spill of px/py, dx/dy, their products, or partial sums before
those named stores. The multiplication order of `(C*A)*4` is observable.

Before rejecting a negative discriminant, the original tests:

```
C < Double1 && S.z>lowerZ && S.z<upperZ
```

The threshold is **1**, not0, at `103D8EB5..103D8EE5`. If true, recompute the
retained three-component S-O differences and calculate
`(DX*px + DY*py) + Float0*pz`, reloading Float DX,DY. The stored E.z-S.z
calculation in this branch is overwritten and does not enter this dot.
Compare against Double **-0.1** (`BFB999999999999A`, `10431ED8`):

- If dot>=-0.1, return clear immediately (`103D8F37..103D8F42`), retaining
  any earlier cap Normal.
- If dot<-0.1, write Hit.Time=+0 and Hit.Location=S, then construct the
  Float vector `(F(S.x-O.x),F(S.y-O.y),+0)` and apply imported Core.SafeNormal.
  Write the resulting Normal, Actor=Owner and Primitive=None; return blocked
  (`103D8F48..103D8FCF`). Next and Item remain unchanged.

If that special gate is false, reject when stored Discriminant<0. Next,
if stored A<Double1e-8, return clear when C>0, otherwise go directly to the
common hit finish using existing Enter and Normal (`103D8FFA..103D9033`).
Thus even a tangent C=0 is accepted in this near-vertical case. Normal can
still be the caller's original seeded value when no cap has set it.

## Quadratic roots and radial normal staging

For A>=Double1e-8, compute:

```
root = appSqrt(Double(Discriminant))  // retained PC64 root
inverse = Double0.5 / FloatA        // retained PC64 inverse
far  = (root - FloatB) * inverse
near = -((root + FloatB) * inverse)
```

The root is FST to Double at `103D9045` but is **not reloaded** into these
root formulas. If `Exit>far`, assign Exit=F(far). If retained `near>Enter`,
FST Enter=F(near) without popping near, then construct the radial normal
using that **retained near**, not its Float Enter copy (`103D9089..103D9105`):

- xOffset=F(DX*near), yOffset=F(DY*near).
- xPoint=F(xOffset+S.x), yPoint=F(yOffset+S.y).
- Normal.x=F(xPoint-O.x), Normal.y=F(yPoint-O.y).
- The Z interpolation/subtraction also executes and Float-stores, then
  Normal.z is explicitly overwritten with exact +0 at `103D90FE`.

Normalize this **caller Normal in place**, but only if its retained
`(nx*nx+ny*ny)+nz*nz` sum is >=Double1e-8. FST that sum to Double before
comparing, keeping the retained sum for the guard. The normalization has
an important distinct storage boundary (`103D9123..103D9161`):

1. appSqrt takes the stored Double squared length.
2. **Store its result to Double and pop** at `103D9141`.
3. Compute retained Double1 / that **stored Double root**.
4. Float-store each original component multiplied by the retained reciprocal.

Below the threshold, leave the radial normal unchanged; do not zero it.
This is not the imported SafeNormal or guarded Core.Normalize behavior.

After root processing, reject if stored Enter>=stored Exit at
`103D916A..103D918C`. This clear return retains all caller Normal mutations
but does not write Time, Location, Actor or Primitive.

## Common hit finish

`103D918F..103D9222` computes retained
`t=clamp(FloatEnter-Double0.001,Double0,Double1)`.
FST Float Hit.Time at `103D91C4` **without popping/reloading t**.

Location has asymmetric stores:

- xOffset=F(FloatDX*t), yOffset=F(F(E.y-S.y)*t).
- zDelta=F(E.z-S.z); zOffset=zDelta*t remains retained, with **no Float
  offset store**.
- Location.x=F(xOffset+S.x), Location.y=F(yOffset+S.y),
  Location.z=F(zOffset+S.z).

Finally write Actor=Owner, Primitive=None and return blocked (native Bool0).
There is no test that this resulting Float Time<1 and no recomputation of the
normal after backoff. Original clear is not simply `Time==1`; it is the
control-flow outcome. Next and Item stay unchanged on every path.

## Imported Core.SafeNormal

Engine IAT `10599B80` names `?SafeNormal@FVector@@QBE?AV1@XZ`.
Core thunk `101031C0` reaches `10144BE0..10144C60`:

1. Compute retained `(x*x+y*y)+z*z` in PC64.
2. FST Double squared length at `10144C04`, compare still-retained sum against
   Double1e-8 at `10144C08`.
3. Below threshold return exact (+0,+0,+0).
4. Otherwise appSqrt the stored Double, retain the root and Double1/root,
   and Float-store original x/y/z scaled by that reciprocal.

There is **no squaredLength==1 fast path** in this binary and no Double root
spill. This differs from the inlined side-normal path above.

## Exact constants and implementation limits

| Meaning | VA | Bits/type |
| --- | --- | --- |
| inside tolerance | `104298A0` | `3FF0000000000000`, Double1 |
| inside directional gate | `10431ED8` | `BFB999999999999A`, Double-0.1 |
| quadratic factor | `10430A80` | `4010000000000000`, Double4 |
| inverse factor | `1042AC10` | `3FE0000000000000`, Double0.5 |
| direction/normal threshold | `1042D538` | `3E45798EE2308C3A`, Double1e-8 |
| parametric backoff | `1042AC20` | `3F50624DD2F1A9FC`, Double0.001 |
| Core normal threshold | `101765D8` | `3E45798EE2308C3A`, Double1e-8 |

A portable finite/overflow refusal must restore caller-owned partial hit writes
and the enclosing root transaction, not turn a failure into a clear ray.
Null Owner and legitimate early branches must remain genuine short circuits.
Do not silently normalize extents, clamp radius/height, replace the initial
overlap dot rule, reset a missed hit, invent an Item, or substitute a host
double quadratic. The body contains no callbacks; higher-level queries,
blocking gates, hit ordering and movement callbacks remain separate work.

## Portable primitive checkpoint (2026-10-16)

`native/quest_actor_line_trace.h` now implements the complete detached
UPrimitive cylinder operation. Owner identity is opaque; no primitive/hash or
class selection is invented. The caller packet is copied before evaluation
and committed on either genuine clear or blocked returns. Thus clear retains
the original partial Normal writes while explicit finite/overflow/domain
refusals leave the entire caller packet unchanged. A future VM/native caller
must propagate a refusal through its enclosing root transaction, not convert
it into clear. Next/Item and all unused seeded fields remain unvalidated and
unchanged where the original does not read/write them.

Arithmetic uses integer PC64 dyadics and explicit Float/Double stores,
including the radial-normal Double root spill, SafeNormal's retained root,
retained root/time after non-popping stores, asymmetric final Z interpolation
and original overwritten finite scratch stores. Null Owner and genuine early
bounds returns preserve active-field short-circuiting. Nonnegative finite
collision dimensions are the supported domain; no clamp is applied.

The independent test header `desktop/quest_actor_line_trace_x87_oracle.h`
uses actual hardware x87 PC64 operations, not production dyadics or original
binary execution. Deterministic controls and24,000 random sweeps plus dense
contact/nextafter neighborhoods pass:84664 checks,668 explicit safety
refusals,7756 blocked,20011 clear and1699 partial-Normal clear outcomes.

Independent rereading corrected an audit error **before acceptance**: the
upper cap/broadphase and all slab crossings are strict, not inclusive. In
particular FCOMP followed by `test AH,0x41; JNE` rejects equality from its
greater-than body; it does not establish >=. Production, independent oracle
and deterministic cap-contact expectations were all corrected against the
actual instruction sequence. Exact upper/lower zero-length contacts exercise
the retained caller-Normal hit rather than a fabricated early clear.

This is not yet integrated actor-hash traversal, native Single/MultiLineCheck,
Move266, startup, support/Base or falling physics. See ORIGINAL-ACTOR-MOVE.md
and ORIGINAL-MULTI-LINE-TRACE.md for the separately audited complete caller
ordering and remaining dependencies. No live crate/APK acceptance is claimed.

The coherent full host build and strict ARM64 syntax pass. CTest110 passed /
125 registered /15 explicit owned-data skips /zero failures in26.78s.
Local ignored evidence is `artifacts/actor-line-math-20261016.log`,
`artifacts/actor-line-model-cache-*-20261016.log` and the separately supplied
original-data regression logs `artifacts/actor-line-*-owned-20261016.log`.
Engine/Core/bundled MSVCRT hashes were rechecked against those stated above
and in ORIGINAL-MULTI-LINE-TRACE.md. No commercial executable code is run.
