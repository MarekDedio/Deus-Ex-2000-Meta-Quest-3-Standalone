# Original GOTY decal native contract audit

Audit date: 2026-10-11. This is a bounded static audit of the user's original
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

## Attach: bounded entry and primary surface selection

The following is directly established for the primary attachment path:

1. Set result false. Require a non-null native client and at least one client
   viewport (`0x103E72E5..0x103E7328`). The native assumes preceding engine/level
   links exist. A headless success/failure shortcut is not equivalent behavior.
2. Require `self.Texture` (actor offset `+0x16C`), otherwise log
   `AttachDecal: No Texture` and return false. Only after these guards,
   `MultiDecalLevel` (`+0x318`) is upper-clamped to four; negatives are preserved.
3. Let `X` be rotation X axis. Trace from actor Location to
   `Location-normalize(-X)*TraceDistance` against **UModel**, not Actor.Trace or
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
   `abs((surfaceBase-C) dot N)>0.001` (`0x103E7910`). Reject surface flags masked
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
Texture `USize` only is read for geometry, twice at `0x103E7A13` and nearby;
`VSize` is not used. The radius is:

`R = sqrt(USize*USize*DrawScale*DrawScale*0.5)`.

Negative DrawScale thus does not invert geometric radius, but can invert UVs.
Below, native guarded normalization divides by length only when squared length
is at least `1e-8`; it is not an invented fallback for degenerate vectors.

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

- MultiDecalLevel > 0: secondary grid traces, angular rejection and corner
  reprojection are present (`0x103E7F28..0x103E8915`), but their complete contract
  is not translated/tested here. Secondary surfaces already present in SurfList
  are skipped; this differs from primary repeated-attach accumulation. Grid
  traces use the normal-scaled `-16` offset. Do not claim full multidecal parity.
- Exact cached-surface-node list construction/order is not audited. Deriving
  table order is a port policy until demonstrated equivalent.
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
