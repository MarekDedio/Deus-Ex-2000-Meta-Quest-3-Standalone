# Original moving-brush tracker construction (2026-10-30)

This is a static contract audit for the positive `GNewBrushTracker` branch
selected by the first `Maps/00_Training.dx` LoadMap continuation. It is not
implementation, startup completion, crate acceptance, or APK evidence. No
commercial DLL/EXE was loaded or executed; no build, map test, or other-map
integration was performed by this audit. This document is its only edit.

The immediate caller and still-required later services are documented in
`ORIGINAL-POST-PARTITION-CLIENT-SETUP.md`. That continuation's explicit guard
must remain until the real selected service, including initial BSP insertion,
is implemented under the caller's ownership/rollback journal. A static world
Model, working demo door, empty tracker, or successful isolated callback is
not that service.

## Authority and address notation

Authority is the read-only owned file
`D:/Steam/steamapps/common/Deus Ex/System/Engine.dll`, freshly fingerprinted:
SHA256 `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Addresses are that original PE32's virtual addresses and byte offsets, not
portable object layouts. Static disassembly used the external
`C:/ProgramData/mingw64/mingw64/bin/objdump.exe`; the existing port-owned
`artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe` only
parsed `.u` property descriptors. It did not execute the original scripts.

Original export identities authenticate `ABrush::PrivateStaticClass` at
`10582A28` and `AMover::PrivateStaticClass` at `1058BB58`. The ctor performs
real inheritance traversal/checked casts, not string-name approximations.

## Factory, storage, and live Model layout

`GNewBrushTracker` thunk `103021EE -> 1037C860` allocates `0x68` bytes and
invokes constructor thunk `1030168B -> 1037AC40` with the actual ULevel.
Allocation failure returns null. Normal positive construction returns the
constructed tracker, subsequently stored at Level `+FC` by LoadMap.

Level `+98` is the actual UModel. Its relevant embedded TTransArray fields
have Data/Num/Max/Owner words; **Num is not at the field start**:

| Native Model field | Data | Num | Max | Owner | Element bytes |
| --- | --- | --- | --- | --- | --- |
| Nodes | `58` | `5C` | `60` | `64` | `40` |
| Verts | `68` | `6C` | `70` | `74` | `08` |
| Vectors | `78` | `7C` | `80` | `84` | `0C` |
| Points | `88` | `8C` | `90` | `94` | `0C` |
| Surfs | `98` | `9C` | `A0` | `A4` | `40` |

Do not confuse the portable C++ Model member ordering/element sizes with
this original ABI. A port needs typed, mutable, authoritative geometry, not
five unrelated copied counters or a triangle-only mesh. The same live Model
must be read and mutated by startup, mover updates, queries, and teardown.

Tracker construction stores the following source fields:

| Tracker offset | Meaning established by use |
| --- | --- |
| `00` | vtable `1042C578` |
| `04` | actual ULevel |
| `08..10` | current transformed polygon normal, filled by Update |
| `14` | dynamic-node allocation search cursor, initially old Nodes.Num |
| `18` | dynamic-vert allocation search cursor, initially old Verts.Num |
| `1C` | Points.Num **after** all SetupActor calls, before Update |
| `20` | original Surfs.Num, captured before SetupActor |
| `24` | original Vectors.Num, captured before SetupActor |
| `28` | original Points.Num, captured before SetupActor |
| `2C` | original Nodes.Num, captured before SetupActor |
| `30` | original Verts.Num, captured before SetupActor |
| `34` | current mover during Update/insertion |
| `38` | pointer to the next mover-root-chain link to fill |
| `3C` | pending mover linked-list head during Update |
| `40` | free-point-list head; constructor stores `-1` |
| `44/48/4C` | 4-byte point-link/owner array Data/Num/Max |
| `50/54/58` | 8-byte dynamic-node `(owner,parentIndex)` array |
| `5C/60/64` | 4-byte dynamic-vert owner array |

The ctor explicitly zeros all three array triples. Other work fields are
set when used; this is not evidence of an all-zero native tracker object.

## Exact constructor pass order

The body `1037AC40..1037AF51` performs this sequence:

1. Initialize the three tracker arrays, vtable, Level, five original counts,
   and free-point head as above (`1037AC66..1037ACDE`).
2. Live-walk Level Actors in increasing index order. Level Data is `+2C`,
   Num is `+30`, and the current bounds/data are reread by the loop. Select
   nonnull actors with Brush `+178` nonnull, `IsA(ABrush)`, and native Actor
   `+28` mask`0x1` (`bStatic`) clear. Checked-cast each selected actor to AMover,
   then call SetupActor thunk `10302FC2 -> 1037B5C0`
   (`1037ACE1..1037ADB6`). This is not an authorized filter that silently
   ignores a non-Mover dynamic Brush.
3. Save post-setup Points.Num at tracker `+1C` (`1037ADB7..1037ADC6`).
4. Shrink Vectors.Max to Vectors.Num, then Surfs.Max to Surfs.Num, retaining
   their contents and transaction owners (`1037ADC9..1037AE11`).
5. Grow/zero the actual Nodes pool, using initialAdd512 and ceiling65536;
   store its previous Num at tracker `+14` (`1037AE12..1037AE34`).
6. Grow/zero the actual Verts pool, using initialAdd512 and ceilingINT_MAX;
   store its previous Num at tracker `+18` (`1037AE37..1037AE59`).
7. Rebuild tracker `+50` as zeroed 8-byte entries, one per
   `Nodes.Num - originalNodes` (`1037AE5C..1037AE6B`).
8. Rebuild tracker `+5C` as zeroed 4-byte entries, one per
   `Verts.Num - originalVerts` (`1037AE70..1037AF0A`).
9. Call real `Update(nullptr)`, thunk `10301037 -> 1037A400`
   (`1037AF0C..1037AF10`), then finish the constructor. This is a positive
   initialization pass over the movers, not permission for an empty callback.

Growth helpers are `10301D66 -> 1037CC30` for 64-byte Nodes and
`103014D3 -> 1037CD70` for 8-byte Verts. Each saves oldNum, computes
`target=min(2*oldNum+initialAdd,ceiling)`, adds/zeros only a positive delta,
then shrinks Max to Num and returns oldNum. During expansion beyond capacity
the original FArray formula is `newNum + floor(3*newNum/8) + 32` before the
final shrink. A capacity reservation with unchanged Num is not equivalent.
`10303161 -> 1037CEB0` rebuilds the zeroed 8-byte owner pairs; it does not
copy FBspNode bytes into that array.

## SetupActor: real polygon/surface linkage

`1037B5C0..1037B9E0` first writes:

- mover.Brush UModel `+F8 = -1` (dynamic-node root chain).
- mover `OldPos +4AC = (-1,-1,-1)`.
- mover `OldRot +4D0 = (123,456,789)`.
- clear only native Actor `+28` mask`0x100` (`bAssimilated`).

Property inspection authenticates `Engine.Mover.OldPos` as
StructProperty/Core.Object.Vector, `OldRot` as
StructProperty/Core.Object.Rotator (both dim1, flags0), and
`Engine.Actor.bAssimilated` as BoolProperty dim1 flags`0x2002`.

Brush UModel `+54` is its UPolys object, whose Data/Num are `+28/+2C`.
Native FPoly stride is `0x110`; its fields consumed here/Update are:

| Offset | FPoly field |
| --- | --- |
| `00/0C/18/24` | Base, Normal, TextureU, TextureV, each FVector |
| `30..EF` | fixed-capacity16 FVector vertices |
| `F0/F4/F8/FC` | PolyFlags, Actor, Texture, ItemName |
| `100/104/108` | NumVertices, LinkIndex, BrushPolyIndex |
| `10C/10E` | signed16 PanU/PanV |

For **every** actual polygon, in serialized order, SetupActor appends one
zeroed64-byte world Surf, three zeroed12-byte world Vectors, and one
zeroed12-byte world Point, then writes:

| World Surf native offset | Source/store |
| --- | --- |
| `00` | polygon.Texture `+F8` |
| `04` | `polygon.PolyFlags & 0x3CFFFFFF`, plus `0x100000` if bSpecialLit |
| `08` | new world Point index |
| `0C/10/14` | the three new world Vector indices |
| `18` | polygon `+108` value, verbatim |
| `1C` | polygon ordinal within this mover's UPolys |
| `20/22` | polygon PanU/PanV |
| `24` | the actual mover pointer |

Finally it **rewrites the source polygon's LinkIndex `+104`** to the new
world Surf index (`1037B986..1037B98C`). The source UPolys must therefore
retain all records and their fields/references; a rendering triangulator
that discards native polygon records cannot support this service.

Actor native `+1EC` mask1 is bSpecialLit, authenticated independently:
native property-difference logic `10362EA8..10362EB6` tests this exact bit,
then resolves UTF-16 `bSpecialLit` at `1047AA7C` (`10362ECD`). Engine.u
descriptor is BoolProperty dim1 flags`0x21`. SetupActor tests that bit at
`1037B972..1037B97B`. The ORed Surf bit is PF_SpecialLit.

SetupActor does **not** copy Base/Normal/U/V into its zero placeholders.
They are populated from transformed polygons by the mandatory Update pass.

## Initial Update(nullptr): selected passes and topology

`1037A400..1037AA30` captures an original global memory-stack marker and
sets tracker `+3C` to null. A null parameter means a full current Actor walk,
with the same nonnull-Brush/ABrush/not-bStatic selection. For each eligible
actor not already bAssimilated it:

- sets bAssimilated;
- allocates an8-byte `(actor,next)` stack node and prepends it to `+3C`;
- copies current Location `+114..11C` to OldPos `+4AC..4B4`;
- copies current Rotation `+120..128` to OldRot `+4D0..4D8`.

The pending work order is therefore the reverse of the increasing actor
selection order, not a new sorted set. Nonnull Update parameters are checked
for AMover identity and enter the enqueue path only when Location or Rotation
differs from the stored old values and bAssimilated is clear. An invalid
nonnull type is normalized to the null/full-walk selection.

Pass1 walks each pending mover's existing Brush `+F8` dynamic-root chain,
calling `103032AB -> 1037C4F0` for each root and following node `+30`.
Fresh SetupActor roots are -1, so no old-root callback is selected initially.
This helper's full noninitial dependency propagation is not established here.

Pass2 calls `10303F03 -> 1037C6D0` for each pending mover. This flushes its
old dynamic nodes: detach parent Front/Back/Plane links, return point indices
to tracker `+44/+40` free list, clear dynamic vertex ownership and node
owner/parent pairs, then set Brush `+F8=-1`. Fresh initial roots take the
short `-1` path only. This helper is not the transform/insertion operation.

Pass3 processes each pending mover (`1037A66E..1037A9FC`):

1. Clear only bAssimilated and store the mover at tracker `+34`.
2. Build its original FModelCoords through exported ABrush::BuildCoords,
   thunk `1030202C -> 1032F9E0`, supplying the first output and null second
   output. Capture the returned Float determinant.
3. Allocate scratch polygon/vertex storage; copy each full native FPoly,
   then call exported FPoly::Transform `10301280 -> 10387360` with those
   coords, mover PrePivot `+180`, mover Location `+114`, and determinant.
   Collect transformed vertices into a contiguous list.
4. Construct an FSphere from those vertices (Core import `10599E80`;
   `??0FSphere@@QAE@PBVFVector@@H@Z`), then call world Model
   PrecomputeSphereFilter, thunk `10301BEF -> 103F6460`.
5. Initialize tracker `+38` to the address of mover.Brush `+F8`.
6. For each transformed scratch FPoly, use its retained LinkIndex to select
   the actual world Surf. Store transformed Base into that Surf's Point,
   transformed Normal/U/V into its three Vector slots, and Normal into
   tracker `+08..10` (`1037A855..1037A94D`).
7. If current world Nodes.Num>0, invoke FilterPoly
   `103030B2 -> 1037C0C0` with
   `(surfIndex, nodeIndex0, coplanarIndex-1, transformedPoly, outside1)`.
8. Terminate the final root-chain link with -1. Walk the mover's resulting
   root chain, assert each node's recorded owner is this mover, and clear
   nodeflags mask`0x20` (`1037A981..1037A9D8`).
9. Pop per-mover scratch, then finally pop the entry stack marker. Stack
   scratch lifetime cannot become dangling published Model or polygon data.

### Sphere prefilter is a Model mutation

`103F6460..103F651A` begins at world node0 when Nodes.Num is nonzero. It
clears nodeflags`0xC0`, computes plane distance to the sphere, marks front
only with`0x40` or back only with`0x80`, and traverses appropriate children;
overlap visits both sides using helper thunk`103012A3`. These flags are
consumed by FilterPoly. This is not an unrelated renderer frustum check.

### FilterPoly and AddPolyToBsp are selected by initial Update

FilterPoly `1037C0C0..1037C405` allocates split scratch, partitions polygons
with14 or more vertices through thunk`10303828`, and traverses current
world nodes. It uses prefilter flags`0x40/0x80`; otherwise it calls
FPoly's plane-split operation through thunk`10301910`. It handles front,
back, coplanar orientation, split fragments, source node CSG flags, and the
outside state. At selected outside leaves/fragments it calls insertion
thunk`10302D29 -> 1037BB00`. The exact floating comparisons, split arithmetic,
and all recursive cases still require faithful helper implementations; a
bounding-box-only insertion is not justified by this evidence.

AddPolyToBsp `1037BB00..1037BF8A` is concrete geometry mutation, not a count:

- Find a free dynamic node from tracker `+50`, with wraparound search from
  `+14`; store `(currentMover,parentIndex)` in its ownership pair.
- Populate the node's surface, parent-chain linkage, plane
  `(Normal, Base dot Normal)`, vertex count, -1 child/bound/root-chain fields,
  inherited zones/leaves and zone masks. Initial flags include`0x20`;
  PolyFlags affect NotCsg/ShootThrough/NotVisBlocking bits. Parent ZoneMask
  is updated with the selected child-zone bits.
- Find a contiguous free FVert run through tracker `+5C`, wrapping from
  `+18`, and mark ownership. Failure clears the node's ownership pair and
  returns without publishing a parent/root link.
- For each polygon vertex, allocate/reuse an actual world Point through
  thunk`10302220 -> 1037CFF0`, set FVert.PointIndex and Side=-1, and write
  the transformed FVector into world Points. Point allocation appends both
  a Point and matching tracker `+44` entry when its free list is empty.
- Attach the new node to the selected parent's Front/Back/Plane field.
  Fill the current mover-root-chain link, then advance tracker `+38` to
  newNode `+30`. Therefore native node `+28` (coplanar BSP child) and `+30`
  (per-mover root chain) are distinct and must not be conflated.

Original FBspNode offsets used in these operations are Plane`00..0C`,
ZoneMask`10..14`, VertPool`18`, Surf`1C`, Back`20`, Front`24`, PlaneChild`28`,
bound/root-chain words`2C/30`, Zone bytes`34/35`, NumVertices`36`, Flags`37`,
and Leaf indices`38/3C`. Portable structures may store these differently.

### Additional insertion controls recovered from the complete body

The bounded artifact `brush-tracker-add-poly-static-20261030.log` contains
the full `1037BB00..1037BF8A` body, not a truncated prefix. Stack arguments
at entry are SurfIndex`+8`, NodeIndex`+C`, CoplanarIndex`+10`, Place`+14`,
and FPoly pointer`+18`; this method returns no success Bool.

If CoplanarIndex!=-1, it becomes the candidate parent, Place is forced to2,
and its PlaneChild`+28` links are followed to the last coplanar node. A
separate Bool remembers whether the original NodeIndex and CoplanarIndex
differed. The original NodeIndex is still used for inherited world-zone
updates. Do not conflate these two source nodes.

After finding a free dynamic node, it records current mover and selected
parent in the ownership pair. Plane XYZ bit-copy polygon Normal; W stores
the retained `(Base.y*Normal.y + Base.z*Normal.z) + Base.x*Normal.x`.
NodeFlags starts20; polygon flag8 sets21, flags`0x04000001` OR4, and flag2
OR2. NumVertices copies the low byte of the original count. The two
bound/root words, Front, Back and PlaneChild are initialized to-1.

The contiguous vertex allocator has strict signed limits that must not be
silently corrected: the first scan stops when
`cursor + polygon.NumVertices >= world.Verts.Num`, and the wrapped scan
stops when that sum is >= the originally saved cursor. It advances the
stored cursor one slot per failed/incomplete run; when a run is found it
marks ownership backwards from that run's last slot. A failed vertex run
sets the node's VertPool=-1, clears only its owner/parent pair and returns
before attaching it. It does not undo all preceding unlinked node/ZoneMask
writes. Port atomic-root rollback is additional ownership, not evidence that
the original method itself was an all-or-nothing insertion transaction.

The final attach uses `parent + 0x20 + 4*Place`, then fills the mover-chain
destination and advances it to newNode+30. Root-chain links and the BSP
PlaneChild+28 remain separate. This subsection establishes these additional
source controls, not a finished insertion implementation or test.

## Lifetime/query contract

The native vtable at `1042C578` contains five entries:

| Slot | Thunk/body | Observed operation |
| --- | --- | --- |
| `00` | `10302CA2 -> 1037B070` | deleting-destructor wrapper |
| `04` | `10301037 -> 1037A400` | Update(optional mover) |
| `08` | `10301974 -> 1037A290` | validate dynamic Brush/checked Mover, then flush it |
| `0C` | `10302BD5 -> 1037AC10` | signed `surfIndex >= originalSurfs.Num` query |
| `10` | `10302397 -> 1037B4E0` | FArchive CountBytes for the three tracker arrays |

Destructor body `1037B0B0..1037B3C4` removes the appended portions in this
order: Surfs, Points, Vectors, Nodes, Verts, using each constructor-saved
original count. Native TTransArray transaction hooks are present on removals.
It then scans remaining original Nodes and resets Front/Back/Plane child
indices that refer beyond the restored Nodes.Num to -1, and frees the
three tracker arrays. Teardown cannot just discard an opaque tracker while
leaving appended nodes/surfaces and stale static-node child links behind.
Original destruction and a port's atomic caller rollback are related but
not interchangeable: the latter must also restore captured actor/poly/cache
state on any refused or failed startup attempt.

## Implementation boundary and first-Training acceptance

This audit establishes constructor ordering and the selected initial
mutation graph; it does not establish that any port provider now implements
it. Missing work must remain named, explicit refusals rather than callbacks
that return success with no effect. Required integration includes:

- retain typed original UPolys records and all flags/refs/links/pans;
- retain and mutate actual typed Model Points/Vectors/Surfs/Nodes/Verts,
  including native ownership/index correspondence and mutable bAssimilated,
  OldPos/OldRot, Brush root links, and source polygon LinkIndex;
- implement and verify BuildCoords, Transform, FSphere, plane splitting,
  sphere prefilter, insertion, dynamic allocation, update/removal and teardown
  with original precision/comparison/order semantics;
- keep those writes, allocations, references and publication inside the
  existing bounded LoadMap caller's ownership/rollback journal;
- prove the real first-Training positive mover selection and resulting BSP
  mutations, failure rollback and unchanged owned files before publishing
  this service or proceeding past the startup guard.

Other maps are not needed for this increment. Completing this service alone
would still not complete later client reset/render/detail services, gameplay
Tick scheduling, full original HUD, crate destruction/loot/gravity, or headset
acceptance. No new APK follows from this document.

## Polygon admission implementation (2026-10-30)

`portable_brush_polys.{h,cpp}` now retains the complete serialized v68 FPoly
records separately from the rendering triangulator. Fixed native16-vertex
storage, Base/Normal/U/V, all flags, signed Actor/Texture/FName source indices,
both original links and signed pans are preserved. References are package
indices, not borrowed runtime pointers or fabricated native UObject identity.
Unused fixed vertex slots are portable zero storage, not recovered constructor
data. Serialized capacity never determines live allocation size.

The detached source loader validates version/licensee, genuine Core.Class /
Engine.Polys import and package provenance (or local Class export), bounded
qualified identity, export/file/property boundaries, finite active vector
data, counts/references/names and exact body consumption. Resolver comparison
does not intern synthesized identities. Its caller byte allowance includes
path/identity scratch before property loading as well as raw bytes, retained
prefix and decoded records; no property-phase full-budget double allowance.
It only reads original files and publishes a completed detached value.

Generated controls pass3054 checks/3019 refusals; first Training passes3077
checks/1030 refusals, including exact byte re-encoding for all1018 UPolys
exports,8553 polygons and32868 vertices. Original Training length, timestamp
and file fingerprint are unchanged. Detached mutated-table controls reject
class/root provenance mismatches, cycles, oversize paths and tight phase
budgets; a60KiB qualified identity succeeds within its admitted allowance.
The actual load also leaves the synthesized Engine.Polys name-table identity
unchanged. A24000-component detached source path must be refused by the caller
scratch admission, before filesystem parsing/allocation; the pinned-platform
path component records and allocation-growth overlap are charged, not merely
the source character bytes. No byte cap was increased.
Strict ARM64 and Android-native compilation also pass. Wider regression
evidence is recorded separately in STATUS.md when complete.

This is the actual data prerequisite, NOT tracker construction or live BSP
publication. Runtime loading/GC ownership, SetupActor writes, dynamic pools,
Update, topology, queries, teardown and caller undo remain to be implemented.
The existing post-partition startup guard is unchanged. Exact next transform
contracts are in ORIGINAL-BRUSH-TRANSFORM.md; no new APK is claimed.

## Bounded static reproduction

Use external disassembly on the read-only owned file, never original module
loading/entrypoints. Useful bounded ranges:

```powershell
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037ac40 --stop-address=0x1037af52 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037b5c0 --stop-address=0x1037b9e1 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037a400 --stop-address=0x1037aa31 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037bb00 --stop-address=0x1037bf8b 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037c0c0 --stop-address=0x1037c406 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:/Steam/steamapps/common/Deus Ex' Engine.Mover.OldPos
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:/Steam/steamapps/common/Deus Ex' Engine.Mover.OldRot
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:/Steam/steamapps/common/Deus Ex' Engine.Actor.bAssimilated
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:/Steam/steamapps/common/Deus Ex' Engine.Actor.bSpecialLit
```
