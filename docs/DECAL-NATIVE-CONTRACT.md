# Original GOTY decal native contract audit

Audit dates: 2026-10-11 and 2026-10-12. This is a bounded static audit of the user's original
GOTY binaries and package bytecode, not a claim that the port implements decals.
The original executables/DLLs were only read, never loaded or executed. Generic
Unreal/SurrealEngine decal behavior is not evidence for this contract.

## Authority and reproducible evidence

Original installation: `D:\Steam\steamapps\common\Deus Ex`.

| File | SHA-256 | PE image base |
| --- | --- | --- |
| `System/Engine.dll` | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` | `0x10300000` |
| `System/Engine.u` | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` | n/a |
| `System/Render.dll` | `607F1077C5CD3E25F8A4CDE1FFACF4CF9260AEBC92A72BE40721CD6955BD75D1` | `0x10B00000` |

Ignored evidence lives in `artifacts/decal-native-20261011/`: the `attach-*`,
`detach-*`, `model-line-*`, `point-recursion-*`, `*-serialize-*`, `render-clip-*`
disassemblies; `original-decal-scripts.log`, `original-decal-properties.log`,
`original-shadow-tick.log`; and the `read-*-*.ps1` read-only PE/package helpers.
Exports/imports are also recorded in
`artifacts/world-native-startup-20261011/engine-exports-imports.log`.
The follow-up evidence includes `original-decal-inherited-cdo.log`,
`model-postload-*`, `multi-vector-multiply-*`, and `pe-multi-evidence.log`.
LLVM NDK 27 was used for static disassembly; the existing desktop bytecode
inspector decoded package functions. Nearest unrelated symbols printed by
objdump do not name the private function bodies; thunk targets identify them.

## Reflected API and script lifecycle

`Engine.Decal.AttachDecal` returns **bool**, with required float `TraceDistance`
and optional vector `DecalDir`. Native index is zero, function flags `0x500`;
parameter flags are `0x80`, `0x90`, and bool return `0x580`. The native wrapper
initializes distance and optional vector to zero before evaluation. Its export
thunk `0x10302F9F` jumps to `0x103E7250`.

`DetachDecal` has no parameters or return value (same function flags/native
index). Thunk `0x10301EBA` jumps to `0x103E9A30`.

Original `Decal.BeginPlay` calls `AttachDecal(100)` and destroys self if false.
`Decal.Destroyed` calls `DetachDecal()` then its superclass event. `Decal.Update`
is an empty event. Do not replace the native's failure with success merely to
keep `Shadow` alive: the package explicitly branches on its result.

`DeusEx.ScriptedPawn.CreateShadow` conditionally spawns `DeusEx.Shadow`, owned by
the pawn, at `Pawn.Location-(0,0,CollisionHeight)`, rotation `(16384,0,0)`.
This invokes inherited `Decal.BeginPlay`; a valid BSP attachment is therefore
part of the actual NPC startup lifecycle.

### Authored inherited defaults (not a primary-only shadow)

The read-only `read-decal-cdo.ps1` reader now skips nonempty original class
bytecode with the same token/logical-size rules as the existing package decoder;
it checks the exact class payload ending after the tagged defaults. The actual
chain is `Core.Object -> Engine.Actor -> Engine.Decal -> DeusEx.Shadow`.

- Actor has `DrawScale=1` (tag `62380`, value `62383`) and `ScaleGlow=1`
  (tag `62387`, value `62390`). Its declared logical class bytecode is 832 bytes,
  and the default-property stream begins at `62304` and ends at `62461`.
- Decal has **`MultiDecalLevel=4`** (tag `106781`, value `106784`), not zero.
  Its defaults run from `106781` to exact class end `106828`; it does not
  override DrawScale or ScaleGlow.
- Shadow overrides only `bHasDecal=true` and
  `Texture=DeusExItems.Skins.FlatFXTex40` in its default stream
  `601877..601886`. It therefore inherits MultiDecalLevel four and DrawScale one.
  `DeusEx.u` SHA-256 is
  `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480`.
- That actual Texture is export 476, class Engine.Texture, in DeusExItems.u.
  Tagged `USize=32` and `VSize=32` occur at `5707776`/`5707782`, with values at
  `5707778`/`5707784`. Its tagged stream ends at `5707828`; native texture payload
  follows through export end `5708869`, so that texture stream is deliberately
  not mistaken for a complete class payload. DeusExItems.u SHA-256 is
  `F7114C5228248FCBE9E89E9C6408ABCCE1E41566207A5ABB7C7E58CC4D9BEAAA`.

These are class defaults before instance/script changes; Shadow.Tick subsequently
changes DrawScale. Reducing MultiDecalLevel to zero would change actual authored
NPC behavior rather than implement the original startup path.

## Attach: bounded entry and primary surface selection

The following is directly established for the primary attachment path:

1. Set result false. Require a non-null native client and at least one client
   viewport (`0x103E72E5..0x103E7328`). The native assumes preceding engine/level
   links exist. A headless success/failure shortcut is not equivalent behavior.
2. Require `self.Texture` (actor offset `+0x16C`), otherwise log
   `AttachDecal: No Texture` and return false. Only after these guards,
   `MultiDecalLevel` (`+0x318`) is upper-clamped to four; negatives are preserved.
3. Let `X` be rotation X axis. Trace from actor Location to
   `Location+guardedNormalize(-X)*TraceDistance` against **UModel**, not Actor.Trace or
   ULevel.SingleLineCheck. Owner argument is null, extent is zero, flags are
   **6**. Call at `0x103E7632` uses model vtable slot `+0x58`.
4. Nonzero LineCheck return means unobstructed and returns false. Item `-1`
   also returns false. `FCheckResult.Item` is a **BSP node index**, not surface.
5. Resolve that node's surface and authored point/normal. Recompute the exact
   original line/plane intersection; Attach does **not** use the wrapper's
   backed-off Location or Hit.Normal as its projected decal center.
6. Private `FindCoplanarSurface`, `0x103E9090`, called at `0x103E774D`, tests the
   hit node's actual convex polygon edge signs using original points/vertices.
   On failure (including an empty polygon) it follows only the coplanar `iPlane`
   chain, not other BSP children; null index returns -1. There is no added edge
   epsilon. Return value is a
   surface index. Resolve the selected surface's own point/normal and recompute
   the exact center `C` on that plane.
7. Reject exact zero `X dot N` (`0x103E78A6`), or
   `abs((surfaceBase-C) dot N)>FLOAT(0.001)` (`0x103E7910`). The first dot uses
   `(Yproduct+Zproduct)+Xproduct` with no FLOAT store before comparison to
   FLOAT zero (`0x103E787D..789B`). The residual first FLOAT-stores the
   base-minus-center vector components, then sums `(Xproduct+Zproduct)+Yproduct`
   in the register (`0x103E78E1..78F4`). Its dot is **not FLOAT-stored** before
   sign handling and comparison (`0x103E78F6..7905`). The threshold is the
   binary32 constant at `0x10432B6C`, bits `0x3A83126F`, promoted for comparison,
   not DOUBLE 0.001. Exact boundary is accepted. Reject surface flags masked
   by **`0x00000600`** (`0x103E7919`). The numeric mask is established; this audit
   does not assign it an unproven Invisible/FakeBackdrop name.

Native result layout observed at callers: Actor `+0x04`, Location `+0x08`,
Normal `+0x14`, Primitive `+0x20`, Time `+0x24`, Item `+0x28`.
UModel.LineCheck thunk `0x10303E5E` jumps to `0x103F3C20`; point recursion thunk
`0x1030207C` jumps to `0x103F3560`. Point recursion records node identity at
`0x103F3A8C`. The wrapper uses float-stored segment squared length, subtracts
`0.5/segmentLength` from projected Time, clamps to `[0,1]`, and derives Location.
At `0x103F3DE4`, `fsts` writes float Time but retains the unrounded x87 value for
the subsequent Location multiply. Portable tests must not claim exact x87
near-plane rounding parity.

The point recursion's null-coordinate distances are float-stored before strict
double-threshold comparisons: both distances > -0.001 descend front; otherwise
both < +0.001 descend back. CSG for flags 6 is nonzero NumVertices and no
NodeFlags bits in `0x27`. Front state is outside OR CSG; back is outside AND NOT
CSG. Crossings split at `Start+(Start-End)*StartDistance/(EndDistance-StartDistance)`.
The direction, each multiplied offset, and each final addition have separate
float stores. Near-child recursion runs first; a blocked leaf returns immediately.
The native keeps last-node unchanged through same-side descents (`0x103F3755`,
`0x103F37BA`) and near-side recursion; only entry to the far split side assigns
the splitting node as last-node (`0x103F39BE`). Consequently a wholly-inside
segment may report the initial last-node zero, not the deepest CSG node.
An empty Model returns RootOutside without modifying caller hit storage.
The generated portable helper is separately bounded/validated and refuses a
blocked zero-length query; those safety policies are not native early returns.

## Primary quad geometry

The optional direction controls **texture orientation**, not the trace ray.
Texture `USize` only is read for geometry at `0x103E7A13`; that loaded value
is used twice in the radius product. `VSize` is not used. The radius is:

`R = sqrt(USize*USize*DrawScale*DrawScale*0.5)`.

Negative DrawScale thus does not invert geometric radius, but can invert UVs.
Below, native guarded normalization divides by length only when squared length
is at least `1e-8`; it is not an invented fallback for degenerate vectors.
The secondary correction's imported `FVector.Normalize` is independently
confirmed by Core export `0x1010112C -> 0x10117CF0`: sum order is
`(x*x+y*y)+z*z`, comparison is against DOUBLE `1e-8` at `0x101765D8`, and a
strictly smaller sum returns false **without any component writes**. Tiny and
zero vectors therefore remain unchanged, not replaced with zero or a tangent.
On success the double-stored sum is passed to appSqrt, its reciprocal scales
components, and X/Y/Z are separately FLOAT-stored in place. This is the same
finite-domain retention policy used by primary inlined guarded normalizations.
Evidence is `core-normalize-*` and `read-core-normalize.ps1`; Core.dll SHA-256
is `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

- Exact zero optional vector: sample three `appFrand` components in `[-1,1]`,
  reject squared length greater than one, then UnsafeNormal the accepted vector.
  Project `P=D-N*(D dot N)`. Set `A=P`, `B=P cross N`. This path does not enter
  the supplied-direction 45-degree-combination branch.
- Supplied nonzero vector: project `P=D-N*(D dot N)`. If any projected component
  has absolute value at least `0.0001`, compute `Q=P cross N`, normalize P and Q
  separately, then `A=P+Q`, `B=P-Q` (`0x103E82F6..0x103E8465`).
- If all projected components have absolute value below `0.0001`, replace D
  with `surfaceBase-C`, project again, then use the direct `A=P`, `B=P cross N`
  path. This fallback does not use the supplied-direction combination.

Normalize A and B with the common guarded normalization, scale by R, and retain
the ordered quad **`C+RA, C+RB, C-RA, C-RB`**. Before returning true, subtract
`surface.pBase` from each stored corner (`0x103E892A..0x103E896F`). This differs
from generic rectangular `USize/VSize` implementations.

Private helper `0x103E9340` tests this whole quad against each cached surface BSP
node polygon, in that cached list's order. It clips against convex edges with
strict sign-crossing tests, retains boundaries, and appends a node index whenever
the clipped polygon is nonempty. It stores **node membership**, not the clipped
geometry. The retained whole quad is clipped again during rendering. An empty
membership list means no node restriction to the original renderer; it does not
mean the record is invisible.

### Cached surface-node order

`UModel.PostLoad` export `0x10301F0A` jumps to `0x103ACE90`. The loop at
`0x103ACEBA..0x103ACF14` visits **every node in ascending authored node-table
index**, reads that node's iSurf (`+0x1C`), and appends the node index to the
surface's cached Nodes (`+0x34`, count `+0x38`). There is no vertex-count or
node-flag filter: even an empty polygon's node index is included. This is not
front/back/coplanar BSP traversal order. The loop itself does not clear an
existing cache; on a fresh model its initially empty arrays acquire that order.
FBspSurf.Serialize does not write/read the cached node list.

## MultiDecalLevel secondary grid and reprojection

This section is a static translation of `0x103E7E37..0x103E8915`, not yet
portable implementation/test evidence. Let `M` be the already upper-clamped
MultiDecalLevel, `K[0..3]` the **absolute** primary corners, `N0` the raw authored
primary surface normal, and `O=UnsafeNormal(N0)*(-16)`.

The grid uses `E0=K[1]-K[0]`, `E1=K[3]-K[0]`. The outer index `i` starts at zero
and runs while `i<M`; the inner index `j` restarts at zero and runs while `j<M`
(`0x103E7F28..0x103E7F58`, increments `0x103E8905/890D`). Fractions are
**`(i+1)/M` and `(j+1)/M`**, not i/M, j/M or half-cell centers. It computes,
with intermediate component float stores:

`Q = K[0] + E0*((i+1)/M) + E1*((j+1)/M)`.

Thus M=4 makes sixteen ordered samples including the far corner, excluding
the origin row/column; M<=0 performs no secondary samples. The ray is
**Start=`Q-O`, End=`Q+O`**, owner null, zero extent, flags six (`0x103E80C6..812D`).
For an upward primary normal it starts 16 units above Q and ends 16 below.

Each sample:

1. Skip a clear UModel trace or Item -1. Resolve the hit NODE's surface and
   intersect this complete secondary ray with its authored base/normal, ignoring
   the backed-off Hit.Location, then run FindCoplanarSurface on that center
   (`0x103E8147..8289`).
2. Resolve selected surface base `B1` and raw normal `N1`. Skip if that surface
   occurs **anywhere in this actor's existing SurfList**, including primary and
   earlier secondary attachments. Skip PolyFlags masked by `0x600`
   (`0x103E82CD..82F4`, `0x103E846A`). This uniqueness check is secondary-only;
   the primary can accumulate duplicates over repeated Attach calls.
3. Compute signed cosine `c=(N0 dot N1)/(Size(N0)*Size(N1))`; primary Size is
   float-stored, secondary Size remains an unrounded appSqrt return for the
   denominator. Save c to FLOAT at `0x103E84FA`. The acceptance comparison still
   uses the retained unrounded value: reject **`abs(c)<=0.7`**, where 0.7 is
   DOUBLE constant `0x1042ADC0`; opposite-facing normals can pass. Later corner
   calculations use the float-stored **signed** c, not its absolute value.
4. Insert a zeroed FDecal using the same first-live-Texture grouping as primary,
   write owning self, and append surface index to self.SurfList
   (`0x103E851F..85AC`). There is no corresponding secondary moverRotation or
   SetBase block in this path; those occur for the primary attachment only.
5. For each original primary corner q, perform the following two line-plane
   projections, then the original correction. `F(...)` denotes a FLOAT store,
   vector arithmetic stores each output component, and `Normalize` is the
   original guarded mutating normalization:

   ```text
   E = q - N0
   R = E + (q-E) * F(((B1-E) dot N1) / ((q-E) dot N1))
   W = q - N1
   S = W + (q-W) * F(((B1-W) dot N1) / ((q-W) dot N1))
   a = F(Size(q-S) / c)
   b = F(Size(q-R) / c)
   D = Normalize(S-R)
   secondaryCorner = S - D * F(b-a)
   ```

   This is the literal operation sequence (`0x103E85BB..889C`), not a replacement
   by a simple projection onto N1. The scalar-times-vector helper
   `0x10301EEC -> 0x103D7CB0` confirms the final scalar multiplication with
   component float stores. Imported `FVector.Normalize` is IAT `0x1059A0F4`.
   Both imported dot results in each projection are FLOAT-stored before ratio
   division; the dot implementation is Core export `0x101017AD -> 0x10117710`,
   returning register `(Zproduct+Yproduct)+Xproduct` without an intervening
   FLOAT store. `core-dot-thunk/body.asm` independently establish that order.
6. Clip the whole resulting absolute quad against the selected surface's cached
   node polygons to retain membership (`0x103E88AA..88B4`), then subtract B1 from
   each retained corner (`0x103E88BF..88FC`). Continue the grid; do not replace
   earlier records. Finally subtract primary base from primary corners and
   return true.

Native NaN/zero-normal/singular-denominator behavior is not certified useful or
safe. Portable input validation and bounded sample/clip work must be explicit;
rejecting such inputs is a port safety boundary, not a proven native early-return
branch. Exact x87 near-threshold/component rounding remains uncertified.

## Native ownership, ordering, and detach

Native model arrays: Nodes `+0x58`, Vertices `+0x68`, Vectors `+0x78`, Points
`+0x88`, Surfs `+0x98`. FBspSurf stride is `0x40`; Texture `+0`, flags `+4`,
pBase `+8`, vNormal `+0x0C`, Actor `+0x24`, native Decals array `+0x28`, and
cached BSP-node array `+0x34` (each array is data/count/capacity).

Native FDecal stride is `0x4C`: four relative vector corners `+0..+0x2F`, owning
Decal Actor `+0x30`, node-membership array `+0x34`, stored mover rotation `+0x40`.

Attach does **not** implicitly detach earlier attachments. It scans the selected
surface's ordered native records. Insert before the first record whose owning
actor's **live Texture identity** equals self.Texture, otherwise append. Store
self in the record, and append selected surface index to self.SurfList. The
primary path retains duplicate indices/records across repeated Attach calls
(`0x103E7922..0x103E79AA`). Sorting by actor slot or replacing the last record
would be wrong.

On a surface owned by AMover, Attach stores that mover's current Rotation and
calls `self.SetBase(self.Owner,true)`, notably not surface.Actor
(`0x103E79FE`). This establishes a mover dependency, not a complete translation
of moving-decal rendering.

### Reentrant primary publication/callback ordering

Follow-up static reading of `artifacts/decal-native-20261011/attach-body.asm`
and `pe-evidence.log` establishes the following **in-call** order. These are
original image addresses, not evidence of completed host integration:

1. After the client/viewport and nonnull Texture guards, upper-clamp and write
   `self.MultiDecalLevel` at `0x103E733B..7351`. Compute the rotation-derived
   ray and prepare the optional direction; zero direction consumes its
   rejection-sampled random draws at `0x103E74FD/7514/752B`, before the primary
   UModel trace at `0x103E7632`. Trace/selection/acceptance finish before any
   native surface record is inserted. The selected center, references to the
   surface base/normal, and prepared direction are retained across the later
   callback, not recomputed from a callback-modified self Location or Rotation.
2. Scan the primary surface's records using each owner's **then-live** Texture
   and self's then-live Texture (`0x103E793B..7948`). InsertZeroed
   (`0x103E7957`) or AddZeroed (`0x103E7979`) creates the FDecal; the PE evidence
   names the imports `FArray::InsertZeroed` and `FArray::AddZeroed`. Write self
   into record `+0x30` at `0x103E7992`, then append the selected surface to
   self.SurfList (`0x103E799F..79AA`). The primary record/list entry therefore
   already exists, with zero corner/membership storage, before the mover
   SetBase path. It is not a fully computed quad atomically inserted afterwards.
3. Test the selected surface's Actor for AMover ancestry
   (`0x103E79AD..79D4`). Copy that mover's current Rotation into record `+0x40`
   (`0x103E79D6..79F4`), then read **self.Owner** and call
   `self.SetBase(self.Owner,true)` (`0x103E79F7..79FE`). This uses the already
   captured mover rotation even if the subsequent callbacks rotate the mover.
   The existing original SetBase contract includes old-base Detach, new-base
   Attach and self BaseChange when applicable; see
   [ACTOR-LIFECYCLE.md](ACTOR-LIFECYCLE.md). Equal-base/cycle refusal does not
   manufacture callbacks.
4. **After SetBase returns**, reread self.Texture at `0x103E7A0D`, load its
   signed USize at `0x103E7A13`, and read live self.DrawScale at
   `0x103E7A16/7A1C`. Build orientation/radius using those post-callback values.
   The direct-axis path first writes the two axes into record corner0/1
   (`0x103E7BC7..7BD8`, `0x103E7C24..7C2F`); the supplied-direction combination
   path writes them at `0x103E8423..842C`, `0x103E845A..845F` and joins the
   common normalization at `0x103E7C2F`. The common path normalizes/scales them,
   writes the opposite corners (`0x103E7D4B..7D60`, `0x103E7D85..7D9B`), then adds
   retained center C to all four (`0x103E7D9E..7E2F`). Only then does
   `0x103E7E32` call the cached-surface-node membership helper on the absolute
   quad. The cache's ascending authored-node order remains as described above.
5. Build secondary grid edges from that absolute primary record. The outer
   and inner loop tests reread live self.MultiDecalLevel at
   `0x103E7F35/7F4A`; the fraction denominators use the current value at
   `0x103E7F68/7FA9`. **There is no second upper clamp after SetBase.**
   Secondary deduplication scans the then-current native self.SurfList
   (`0x103E82D5..82EA`), not a list captured before callbacks. Each secondary
   insertion again compares the existing owner's live Texture with self's
   current Texture (`0x103E853E/8547`), inserts a zeroed record, writes its owner
   and appends SurfList (`0x103E855A..85AC`). It has no second mover/SetBase
   block. Corrected secondary corners are written before membership clipping
   (`0x103E88B4`), then made relative to that secondary base
   (`0x103E88C9..88FC`). **Primary** corners remain absolute through the entire
   secondary grid and become relative to the retained primary base only at
   `0x103E892A/8941/8958/896F`, just before returning true.

Consequently, `ProjectOriginalDecalGeometry` is a geometry-only **fixed-input**
query; its composed result must not drive an entire native Attach across the
mover callback as if all inputs and owner lists were snapshots. A faithful host
needs a pre-callback trace/selection phase; journalled provisional primary
record, root and reflected SurfList publication; the original SetBase callback
path; then post-callback live radius/Texture/Multi/SurfList reads and ordered
quad/membership/secondary updates. Successful pretrace RNG draws must be visible
to callback script's RNG operations; publishing an old candidate seed after the
callback would overwrite its draws. The full outer VM transaction still owns
rollback and final world revision publication.

Callbacks can change self.Texture/DrawScale/MultiDecalLevel and the participating
actors' ownership, bases or rotations. Native Attach/Detach calls can change
SurfList and surface record storage even though SurfList is script-Const. For
example, changing Texture can change post-callback radius and secondary grouping
without relocating the primary record selected using the pre-callback Texture.
Changing Multi above four after its initial clamp must not be silently clamped
again by a reused fixed-input grid helper; explicit bounded work remains a port
safety requirement. The original retains a raw primary-record pointer across
SetBase. This audit does **not** certify defined behavior when callbacks delete
that record, invalidate its storage, clear Texture before the later dereference,
or otherwise invalidate captured native references. Such cases need explicit
transactional refusal/safety contracts, not invented replacement records,
automatic reattachment, null-as-false behavior or claims of original crash parity.
No new x87 precision or rendered-shadow acceptance is established here.

Detach (`0x103E9A67..0x103E9C2D`) repeatedly takes the **last SurfList index**,
scans that surface's records forward, destroys node-array storage and removes
the first record whose Actor is self, then removes the last SurfList integer
whether a matching native record was found or not. It continues until SurfList
is empty. Repeated attachments therefore require both ordered model records and
actor SurfList to be journaled; a last-attachment-only counter is insufficient.

Original Engine.u descriptors: SurfList is Array<IntProperty> flags
**`0x00400002`** (Const + NeedCtorLink), **not Transient**. MultiDecalLevel and
LastRenderedTime have flags zero. Native actor offsets are MultiDecalLevel
`+0x318`, LastRenderedTime `+0x31C`, SurfList `+0x320` (count `+0x324`).

## Render consumer

URender.ClipDecal export `0x10B01069` jumps to `0x10B1BDA0`; the DrawWorld caller
is at `0x10B1B23F`. It traverses native surface records in insertion order and
reads each actor's live Texture, grouping adjacent equal textures for binding.
For each visible saved polygon, a nonempty record node list filters its node
identity; an empty list imposes no restriction. Retained geometry is clipped to
the actual visible polygon.

ClipDecal normalizes the authored surface normal and applies a **0.75 Unreal
unit normal offset** before projection (`0x10B1BF31`, constant `0x10B2B304`).
Mover rotation delta handling is present, but not fully translated in this
audit. The static surface path adds surface.pBase to the relative corners.

UVs start at corner zero: U is projection along normalized corner0-to-corner1,
V along normalized corner0-to-corner3, each divided by actor DrawScale
(`0x10B1C828..0x10B1CA99`). No VSize/aspect ratio or half-texture offset is added.
FTransTexture U/V offsets are `+0x50/+0x54`.

Render color is not necessarily constant white. `0x10B1C551..0x10B1C5A7`
computes gray as `clamp(byte[actor+0x190]/256 + float[actor+0x18C]*0.5,0,1)`;
the native field-name mapping of those offsets remains to be independently
established. It writes Light `(gray,gray,gray,0)` and Fog `(0,0,0,0)` at vertex
offsets `+0x30/+0x40`. Draw flags **`0x40`** are passed at `0x10B1B298`; actor.Style
is not consulted by that call. After producing a nonempty rendered polygon,
the caller updates LastRenderedTime from Level.TimeSeconds (`0x10B1B280`).
Attach itself does not update that timestamp.

## Serialization, GC, and restore limits

ADecal shares AActor's PostLoad/Destroy/Serialize vtable entries. No Decal-native
override was found. AActor.Serialize (`0x1030166D -> 0x10369AD0`) calls only the
imported UObject.Serialize; it does not rebuild attachments.

UModel.Serialize (`0x103026B2 -> 0x103ACAA0`) visits Surfs via
`0x1030158C -> 0x103AE5B0`. FBspSurf's element serializer
(`0x10301F28 -> 0x103AA3A0`) visits native Decals only when archive loading/saving
fields `+0x0C/+0x10` are both zero (`0x103AA45F..0x103AA474`).
The native Decal-array helper is `0x10301E92 -> 0x103ADF30`; its element helper
is `0x1030360C -> 0x103AF430`. That element helper, in non-loading/non-saving
collector mode, visits only owning Actor `+0x30` through the archive object
virtual. Therefore model native attachment ownership participates in GC, but
normal disk archives do **not** store corners, membership, or mover rotation.

SurfList is a nontransient reflected field, but persisting its integers does
not recreate native geometry. No universal postload reattachment is established.
Original Shadow.Tick can detach/recreate its own attachment on a permitted
redraw; it first returns if `(Owner.InStasis() || moved<0.1) && bHasDecal`, so it
cannot be assumed to repair every saved attachment immediately after load.
For a traced floor hit, Tick computes
`max(0,CollisionRadius/15-(dist-CollisionHeight)/130)`, applies ShadowScale for a
ScriptedPawn owner, places/rotates the shadow to the floor, calls
`AttachDecal(32,(0.1,0.1,0))`, then sets bHasDecal true **without checking its
returned bool** (`original-shadow-tick.log`, bytecode offsets `353..529`).

## Explicit remaining gaps / implementation boundary

- MultiDecalLevel > 0 secondary grid/selection/reprojection is statically
  translated above. Subsequent portable geometry/state implementations and
  generated/original-data controls are tracked separately in
  [DECAL-GEOMETRY-PLAN.md](DECAL-GEOMETRY-PLAN.md). Those controls do not complete
  native Attach, viewport/mover ownership, saves, renderer or full parity.
- Cached surface-node construction/order is proven for fresh UModel.PostLoad;
  repeated native PostLoad appends without clearing and must not be silently
  conflated with a fresh cache.
- Full mover corner-transform/UV lifecycle, every rendering enable gate, and
  native field names for render gray inputs remain unproven.
- Generic original save/load native-attachment reconstruction remains open.
  Persisting native geometry in a port checkpoint may be a deliberate port
  policy, but is not original disk format fidelity.
- Singular inputs and x87 boundary rounding are not certified bit-exact.
- Triangle colliders that discard authored node/surface identities cannot
  satisfy the original Attach surface-selection contract.

Until those dependencies are covered, native Attach must remain explicitly
unsupported for unsupported modes. Returning false or success in lieu of the
native operation would make the startup transaction appear valid while
destroying a script-required shadow or committing invisible/stale state.
