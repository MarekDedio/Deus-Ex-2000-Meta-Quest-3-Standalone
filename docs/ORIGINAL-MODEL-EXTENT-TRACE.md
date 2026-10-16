# Original Model extent trace audit

This is a static, read-only audit of the retail GOTY Engine.dll, not evidence
that native Move266, FindBase, falling physics or an on-device crate hit works.
The original DLL/EXE was never executed. Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Engine.u is
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.

The useful next primitive is the **nonzero-extent** branch of UModel.LineCheck.
The existing zero-extent point trace, overlap placement query and render
triangles are not substitutes. The locally pinned SurrealEngine
`677ee14c5b83486e6634687953779aafb7973ad6` TraceAABBModel is useful explanatory
reference, but differs in arithmetic, traversal and box padding. Its results
must not be described as retail parity.

## Native entry points

| Operation | Export thunk | Body |
| --- | --- | --- |
| Actor.execMove | `0x10303ACB` | `0x103E3030`–`0x103E30ED` |
| ULevel.MoveActor | `0x10303C60` | `0x103990E0`–`0x10399F5F` |
| UModel.LineCheck | `0x10303E5E` | `0x103F3C20`–`0x103F4184` |
| box-check constructor | `0x103034EF` | `0x103F18E0`–`0x103F197D` |
| extent recursion | `0x10302838` | `0x103F42F0`–`0x103F568C` |
| additional plane clip | `0x10302676` | `0x103F5B80`–`0x103F5D32` |
| absolute extent dot | `0x1030143D` | `0x103F6590`–`0x103F65DB` |
| two-plane intersection | `0x1030112C` | `0x103F6600`–`0x103F67E6` |

Actor.Move metadata is Engine.u export4571: native266, Delta is a required
Core.Object.Vector StructProperty with dimension1 and flags`0x80`; ReturnValue
is BoolProperty dimension1 flags`0x580`. The wrapper evaluates Delta, finishes
arguments, copies the actor's then-current Rotation and calls its actual
native XLevel virtual`+0x88` with actor/Delta/Rotation/FCheckResult and **all
four movement options zero**. It does not call SetLocation or the reflected
Level object's methods.

## Model wrapper

If the Model has no nodes, LineCheck returns native RootOutside without
performing the extent branch. Exact zero extents select the separately audited
point branch. Any nonzero extent component selects `0x103F3F6E`.

The extent wrapper sets caller Hit.Time to Float2, constructs the helper with
Hit/Model/optional owner/extent/flags, and copies the **full** original End and
Start into it. Size is computed from the Float componentwise End−Start vector
and stored to Float. Recursion begins at node0, last-parent0, side0, with
Outside=RootOutside. There is no normalized-direction input.

If recursion set its hit flag, the wrapper computes retained
`max(Float0.1, Float0.1 / storedFloatSize)`, subtracts that from stored Hit.Time,
clamps to `[0,1]`, then **stores Float Time before** computing Location. It
forms Float End−Start components, multiplies each by storedFloatTime, stores
Float offsets, adds Start, and stores Float Location. The wrapper reports
clear iff this resulting Float Time equals1. If no helper hit was recorded,
it reports clear without inventing a nearest hit.

Float0.1 is bits`3DCCCCCD` at`0x10431C54`; this backoff is **maximum**, not
minimum. The constant/quotient comparison at`0x103F404A` retains the quotient
when the constant is smaller. Zero-size/undefined floating status needs an
explicit bounded-runtime safety policy; it must not become an always-clear
collision shortcut.

## BSP traversal

The helper retains its original full End and Start through every child visit;
it does not split segments. For null owner its node plane is literal authored
Float coefficients. Owner transforms use a separate ToWorld/FPlane path.

At `0x103F43CD`–`0x103F4429`, the traversal computes retained PC64 distances
with deliberately different operation order:

```
dStart = ((nz*Start.z + ny*Start.y) + nx*Start.x) - w
dEnd   = ((nx*End.x   + ny*End.y)   + nz*End.z)   - w
```

Float1.1 (`3F8CCCCD`, `0x10431C34`) pads extents for traversal only. Y/Z
products are stored Float; X remains retained. Padded radius is retained
`(abs(ezPad*nz)+abs(eyPad*ny))+abs(exPad*nx)`. Each abs is conditional `<0`
negation, not an operation that necessarily erases negative zero.

Back is eligible unless **both** distances exceed radius. Front is eligible
unless **both** distances are less than negative radius. Equality remains
eligible. If dStart<dEnd, process Back first; otherwise process Front first.
Then visit the other eligible side. Native Node`+0x20` is Back and`+0x24` is
Front, matching portable serialized field order.

Front's Outside is oldOutside OR IsCsg(0); Back's is oldOutside AND NOT
IsCsg(0). Here IsCsg(0) means NumVertices>0 and `(NodeFlags & 0x21)==0`.
The incoming trace flags do not replace that literal mask in these branches.
At an outside leaf return. At a solid leaf use the **last parent node's**
CollisionBound; absent (`-1`) bounds return without a hit.

## Hull and exact plane clipping

The helper reads the ordered LeafHulls plane references up to literal int32
`-1`, with a retail maximum of64 stored planes. This is not the pinned
overlap helper's “any negative terminator” behavior. Bit`0x40000000` flips
all four Float coefficients; other bits remain part of the plane-node index.
It records each plane's negative/positive X/Y/Z component bits for bevel
selection. After the terminator the next six raw Float words are minXYZ,
maxXYZ. Do not alias int32 storage as an unaligned Float pointer.

For each original plane in order, the inlined clip at
`0x103F4813`–`0x103F4957` is equivalent to the additional-plane helper
`0x103F5B80`–`0x103F5D32`. All arithmetic below is PC64 except explicit
Float stores:

```
e  = Float((abs(ez*nz)+abs(ey*ny))+abs(ex*nx))
dS = Float(((Start.z*nz+Start.y*ny)+Start.x*nx)-w)
dE = Float(((End.z*nz+End.y*ny)+End.x*nx)-w)
q  = dS-e                              // retained, no Float store
if dS>dE and q>=-e and q<0: q=+0
denom = Float(dS-dE)
t = q/denom                            // retained, no Float store
if denom < Double(-1e-5):
    if t<Exit: Exit=Float(t)
else if denom > Double(+1e-5):
    if t>Enter: Enter=Float(t); Normal=plane.xyz
else if dS>e and dE>e:
    reject
require Enter<Exit
```

Enter begins Float−1 and Exit begins current caller Hit.Time. Thresholds are
Double`BEE4F8B588E368F1` at`0x104335D8` and
Double`3EE4F8B588E368F1` at`0x104335C8`. Strict comparisons, retained
quotient comparisons and Float stored interval endpoints are significant.
The retail machine evaluates the quotient even in the parallel branch;
portable avoidance of an unused NaN/Inf requires documenting that floating
status is not exported as gameplay state, not silently changing branches.

## Axis bounds and bevels

For **null owner only**, clip six additional bounding planes in this order:

| Normal | Float W from retained expression |
| --- | --- |
| `(0,0,-1)` | `Double0.1 - minZ` |
| `(0,0,+1)` | `maxZ + Double0.1` |
| `(-1,0,0)` | `Double0.1 - minX` |
| `(+1,0,0)` | `maxX - Double0.1` |
| `(0,-1,0)` | `Double0.1 - minY` |
| `(0,+1,0)` | `maxY - Double0.1` |

Double0.1 is`3FB999999999999A` at`0x1042BCB0`. The **positive-Z expansion**
is retail code (`0x103F4AD6`–`0x103F4AD9`), unlike X/Y maximum shaving.
Uniformly shaving all six faces is not the original contract.

Bevels (`0x103F51AF`–`0x103F5617`) use ordered pairs i=0..n−1,j=0..i−1,
axes X then Y then Z. A pair must have both positive and negative component
sign bits for that axis (union masks3,12,48). It computes the two Float cross
products axis×normal_i and axis×normal_j, then their retained dot. Continue
only when that dot exceeds **Double0.001** (`3F50624DD2F1A9FC`,
`0x1042AC20`), not merely when normal component signs oppose.

Then original two-plane intersection produces Float linePoint and normalized
Float lineDirection. The caller ignores its returned bool. It constructs
bevelNormal using UnsafeNormal(axis×lineDirection), flips it if its dot with
normal_i is negative, constructs a plane from linePoint/bevelNormal and uses
the same exact plane clip above. The exact recovered numeric stages follow;
they still need implementation and independent numerical tests before claiming
the entire extent kernel exact. An algebraically similar double bevel must
not be labelled retail parity.

### Intersection and imported Core numeric stages

`0x103F6600` first computes C=Float3(n1×n2), storing each cross component.
Its determinant is retained `(Cx²+Cy²)+Cz²`. If it is less than Double1e-6
(`3EB0C6F7A0B5ED8D`, `0x10433610`), it writes zero Point and C and returns
false. The ignored false return does not itself skip the bevel in its caller.
Otherwise:

```
A = (C × n1) * w2
B = (n2 × C) * w1
Point = (A+B) * Float(1 / retainedDeterminant)
```

This mathematical notation alone is insufficient: the X/Y components of each
cross are Float-stored **before** their W multiplication, and those X/Y
products are again Float-stored. Each Z cross and Z product remains retained.
SumX and SumY are Float-stored; SumZ remains retained. The inverse determinant
is Float-stored before all three multiplications. All three Point components
are Float-stored. It then calls Core.Normalize(C); the normalized C becomes
lineDirection. This staging is visible at `0x103F66E4`–`0x103F67CB`.

The imported Core operations were read directly from Core.dll SHA256
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`:

| Operation | Thunk | Body | Arithmetic |
| --- | --- | --- | --- |
| FVector cross | `0x10101BD1` | `0x101176C0`–`0x101176F6` | Standard cross; each component Float-stored |
| FVector dot | `0x101017AD` | `0x10117710`–`0x10117728` | Retained `(z*z + y*y) + x*x` pairwise products |
| FVector.Normalize | `0x1010112C` | `0x10117CF0`–`0x10117D61` | Guarded normalization below |
| FVector.UnsafeNormal | `0x10102D4C` | `0x10117DD0`–`0x10117E1E` | Same length arithmetic, no zero/small guard |
| FPlane(point,normal) | `0x10102ACC` | `0x10118360`–`0x10118399` | Copies normal; W=Float((py*ny+pz*nz)+px*nx) |

Both normalizers calculate retained `(x²+y²)+z²`, **not** the Z/Y/X sum
used by the currently existing random-vector normalization helper. They store
that sum to Double, pass the Double value to appSqrt, retain the PC64 root,
retain `Double1/root`, and Float-store each scaled original component. The
guarded version uses FST (not FSTP) at `0x10117D14` and compares the still
retained sum against Double1e-8 before the square-root call. Below that
threshold it returns false and leaves C unchanged. The bevel intersection's
successful determinant gate is higher, but the independent helper contract
must preserve its actual threshold. UnsafeNormal has no guard: the embedding
must explicitly refuse a nonfinite zero-length normalization result rather
than invent an axis. This is a stated finite-runtime limit, not a claim that
the retail masked x87 division itself has undefined behavior.

Core threshold Double1e-8 is`3E45798EE2308C3A` at`0x101765D8`;
Double1 is`3FF0000000000000` at`0x101765A8`. FPlane's W order is Y/Z/X,
not the imported FVector dot order Z/Y/X. The bevel's normal-flip dot uses
the imported dot operation; if negative, Float−1 scaling stores each normal
component before the FPlane constructor.

After all planes, accept a hull only when stored Enter>−1, Enter<Exit and
Exit>0. Write Hit.Time=Enter, literal selected Normal, Actor=owner,
Primitive=Model and helper-hit flag. There is no extra Enter>=0 condition;
the wrapper clamps a negative accepted Enter afterward.

## Integration work that remains separate

ULevel.MoveActor still needs its native object flags/editor gates, swept
actor/Model query and hit ordering, blocking tests, based actor propagation,
encroachment before mutation, collision removal/reinsertion, BumpWall/Bump,
BeginTouch/EndTouch, SetActorZone, native return semantics and one enclosing
rollback journal. FindBase needs the original extent SingleLineCheck flags
`0x17` and SupportActor callback contract, not this primitive alone.

Useful reusable adapters are the actual native Model identity/cache,
portable Model LeafHulls decoding, integer-backed PC64 operations, bounded
graph validation patterns, actor collision registry journaling, original
relationship/event dispatch and typed native property writes. Existing
spawn-placement validation differs in its negative-terminator policy and
must not be reused unchanged as retail extent-trace validation.

## Verified plane component (2026-10-16)

`native/quest_model_extent_clip.h` now implements the exact individual plane
clip and null-owner six-bound-plane construction above. Integer PC64 arithmetic
preserves conditional-abs negative zero, Z/Y/X accumulation, explicit Float
pushout/distances/denominator, retained numerator and quotient comparisons,
Double parallel thresholds, Float interval/normal writes and strict final
interval rejection. Rejected intervals retain their original preceding writes.
The six W expressions use Double0.1, including the positive-Z expansion.

`desktop/quest_model_extent_clip_test.cpp` passes335711 checks and12176
deliberate finite/overflow safety refusals: deterministic entry/exit/contact/
parallel/tie/collapsed interval and signed-zero controls,80,000 independent
hardware x87 clip samples,20,000 independent bound-plane samples and Double
parallel-threshold neighbors. The portable component omits only the unused
parallel-branch quotient/FP status; its independent x87 oracle executes that
division and verifies the resulting observable interval and normal bits.

Full coherent host build and strict ARM64 syntax succeed. CTest107 passed /
122 registered /15 explicit original-data skips /zero failures in23.08s.
Local ignored logs are `artifacts/model-extent-clip-*-20261016.log`.

This component is not yet called by a complete model sweep or native Move266.
Remaining kernel work includes exact validated hull/64-plane/cache authority,
near-first BSP traversal, owner transforms, complete intersection/normalization/
bevel chain, full query budgets and wrapper hit finalization. Then the actual
ULevel actor-hit ordering/callback/collision transaction must be integrated.
No original startup, live damage, falling, loot or new APK is claimed here.
