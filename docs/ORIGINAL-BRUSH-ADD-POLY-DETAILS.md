# Original moving-brush AddPoly parent, zone and point ownership details

Read-only static follow-up, 2026-10-31. These notes close specific source
mapping gaps in `ORIGINAL-MOVING-BRUSH-TRACKER.md`; they do not implement
AddPolyToBsp, tracker construction/Update, queries, crate gameplay, successful
startup or an APK. No commercial DLL/EXE was loaded or executed. This document
is the only file edited for this follow-up; no compiler input, build or test
was changed/run.

## Authority and reproduction

Authority is the owned, read-only
`D:/Steam/steamapps/common/Deus Ex/System/Engine.dll`, freshly SHA256 checked:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Addresses below are that PE32's native virtual addresses/offsets, not portable
C++ layout. AddPolyToBsp thunk `10302D29` targets `1037BB00`.

The complete existing disassembly artifact
`artifacts/brush-tracker-add-poly-static-20261030.log` covers
`1037BB00..1037BF8A`, is 23146 bytes, and has SHA256
`DD788414650A2C88CDA523877BAA7B2532C587AF50546FB9C3E16C35C3DEA81D`.
The point allocator thunk `10302220 -> 1037CFF0` was additionally inspected
with the external objdump, directly as file data. Its normal body returns
at `1037D139`; the following exception-handler code is not a second successful
allocation path. No new disassembly artifact was written for that read.

```powershell
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037bb00 --stop-address=0x1037bf8b 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037cff0 --stop-address=0x1037d16a 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
```

## Arguments, distinct parents and field mapping

AddPoly's original entry arguments are SurfIndex at `ebp+08`, NodeIndex at
`+0C`, CoplanarIndex at `+10`, Place at `+14`, and FPoly pointer at `+18`.
For this note let N be original NodeIndex, C original CoplanarIndex, P original
Place, and T the ultimately selected attachment parent.

`1037BB1B..1037BB6E` saves P at local `-24` before changing placement:

- Initially T=N and the remembered `different` Bool is false.
- If C!=-1, remember `different=(N!=C)`, force actual attachment Place=2,
  and follow C's PlaneChild `+28` links until the last coplanar node T.
- The comparison uses the initial C, not the last T. N, P and T remain
  separately significant afterward.

The selected free dynamic node's ownership pair is written as
`(currentMover,T)` at `1037BBCB..1037BBD3`, before its geometry fields.
Actual node indices remain in the same world Model index space.

| Native FBspNode field | PortableModelNode field/use |
| --- | --- |
| `+00/+04/+08/+0C` | planeX/planeY/planeZ/planeW |
| `+10/+14` | zoneMask low/high words |
| `+18/+1C` | vertexPool/surface |
| `+20/+24/+28` | back/front/plane (coplanar BSP child) |
| `+2C` | collisionBound |
| `+30` | renderBound word, dynamically used as mover-root-chain link |
| `+34/+35` | zone0/zone1 |
| `+36/+37` | vertexCount/nodeFlags |
| `+38/+3C` | leaf0/leaf1 |

The `+28` BSP coplanar child and `+30` mover chain are distinct. Never use
either as an alternate Front/Back edge or conflate them during teardown.

## Exact new-node and zone/leaf store order

After ownership allocation, `1037BBE4..1037BCBA` writes the new node's Surf,
sets both bound/root words to -1, copies **T.ZoneMask** into the new node,
copies the low byte of polygon NumVertices, sets Front/Back/PlaneChild to -1,
and stores its plane/flags. Plane XYZ bit-copy polygon Normal. Plane W is a
Float store of retained x87/PC64 arithmetic:

```
W = F32((Base.y*Normal.y + Base.z*Normal.z) + Base.x*Normal.x)
```

NodeFlags initially stores `0x20`; polygon flag8 replaces it with `0x21`;
polygon mask `0x04000001` ORs4; polygon flag2 ORs2. These are source stores,
not inferred renderer or collision categories.

Zone/leaf selection at `1037BCBA..1037BD35` has two branches:

1. If C!=-1 and `different` is false, use the **last selected T**, even
   when its coplanar chain advanced beyond the initial C=N. Compute:

   ```
   dot = (T.planeZ*new.planeZ + T.planeY*new.planeY)
         + T.planeX*new.planeX
   k = dot < Double0 ? 1 : 0
   new.zone0 = T.zone[k];   new.leaf0 = T.leaf[k]
   new.zone1 = T.zone[1-k]; new.leaf1 = T.leaf[1-k]
   ```

   Products/additions retain the original x87/PC64 values with no Float
   spill before comparison. Zero takes k=0. This finite-domain transcription
   does not grant support for native unordered/NaN arithmetic.

2. Otherwise, duplicate **N.zone[P]** and **N.leaf[P]** into both new sides.
   This uses original N and original P, not T or forced placement2. Selected
   FilterPoly callers supply original P=0/1 for Back/Front.

Next, `1037BD35..1037BD77` forms the two selected zone bits and ORs them
into **N.ZoneMask**, not T.ZoneMask unless N==T. For authenticated zone
numbers0..63 the effect is:

```
N.zoneMask |= (uint64(1) << new.zone0) | (uint64(1) << new.zone1)
```

This notation does not authorize C++ shifts for unvalidated zone numbers.
The original uses its 64-bit shift helper. Crucially, the new node's mask
was already copied from T **before** this OR: do not recopy/recalculate it
after updating N. For example, a zero old parent mask and zone bits1/2
leave the new copied mask zero while N receives6.

## Vertex allocation, final attachment and partial writes

Vertex ownership allocation starts only after those node/zone writes.
The existing constructor contract describes its strict signed end limits,
wraparound cursor and backward marking of a successful run. Do not replace
those controls with a conventional inclusive-fit allocator.

The final attach occurs only after all point/vertex stores succeed:
`1037BF5D..1037BF75` writes the selected dynamic node index into
`T + 0x20 + 4*actualPlace`, i.e. Back/Front/PlaneChild for placement0/1/2.
It then fills the current mover-root-chain destination and advances that
destination to the new node's separate `+30` word. A portable destination
should retain typed Model-root or node-index identity, not a vector element
pointer that can dangle across allocations.

Failure effects are not insertion-local atomicity:

- **No free dynamic node:** node cursor search may advance/wrap, but no new
  node ownership/geometry or parent/root attachment is written.
- **No contiguous vertex run:** store new.VertexPool=-1, clear only the
  chosen node's owner/parent pair, and return (`1037BE34..1037BE65`). Earlier
  new-node stores and N.ZoneMask's OR remain. No parent/root link is attached.
- **Point helper returns -1:** the guard at `1037BEB8..1037BEE4` returns
  already allocated points to the free list in reverse prior-vertex order,
  then jumps to that same node-owner/parent clearing path. This body does
  **not** reset the successful vertexPool run index or clear its already
  marked vertex-owner run. Prior FVert records and appended Point/bookkeeping
  counts are not truncated. Earlier node/zone-mask stores remain unlinked.

The normal valid point-helper paths below do not explicitly return -1.
This caller guard is not evidence that a portable byte/work-budget refusal
should fabricate such a return. Port refusal must unwind the same outer
transaction, whose additional owning journal restores all partial writes.

## Point allocation and the typed owner/free-link union

`1037CFF0` receives five pointers: post-Setup Points count, world Points
array, tracker point-bookkeeping array `+44`, free-point-head `+40`, and
current-Mover word `+34`. At `1037D013..1037D044` it asserts:

```
world.Points.Num - postSetupPoints == pointBookkeeping.Num
```

The bookkeeping array is **not just integer free links**. Its native
4-byte word holds a Mover pointer while a point is owned, or the next
absolute world Point index while that slot is free:

- If freeHead!=-1 (`1037D047..1037D05E`), return that absolute point index
  and replace freeHead with bookkeeping[index-postSetupPoints]. No Point
  zeroing/append occurs on this reuse branch.
- Otherwise (`1037D063..1037D116`), append one zeroed12-byte world Point,
  then one zeroed4-byte bookkeeping entry. Each original array independently
  uses `newNum + floor(3*newNum/8) + 32` growth when necessary. The returned
  index is the pre-append world Points.Num.
- Finally (`1037D119..1037D127`), write the actual current Mover pointer
  into bookkeeping[result-postSetupPoints]. The caller next stores
  FVert.Side=-1, stores FVert.PointIndex, reacquires world Points.Data after
  the helper's possible growth, and writes the transformed XYZ coordinates
  (`1037BEE6..1037BF25`).

A 64-bit portable owner needs a typed/discriminated active-Mover versus
free-next-index representation. Casting a UObject pointer into int32,
leaving active owner edges unmarked, or retaining an append-only history of
owners is not equivalent. Free-list replacement removes the old active
owner edge; beforeimages/temporary candidate state need their corresponding
bounded roots until commit/rollback. Native 4-byte element width is not the
portable allocation size to charge.

## Still-required implementation and proof

These are static contracts, not executed original methods or passing port
tests. The complete typed tracker, pool admission, recursive FilterPoly,
AddPoly mutation/allocators, actual Update, active/index-preserving query
capability, same-caller journal/GC and independent positive/failure tests
remain required. Raw serialized-source validation and immutable proof
contracts must not be weakened to accommodate zero free pool entries.

This note does not recover the complete noninitial dependency-propagation
helper, flush/free transitions or destructor behavior. Their typed
implementation and verification remain separate requirements; fresh
constructor roots taking the real -1 short paths do not justify empty
providers for later updates or retirement. The post-partition startup
guard must remain until its genuine selected service and later startup
dependencies are implemented. No startup, crates, other-map or APK success
is claimed here.
