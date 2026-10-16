# Original collision hash registration and swept query

Read-only static audit of the user-owned Deus Ex GOTY Engine.dll and Core.dll,
2026-10-16. No original DLL or EXE is loaded or executed. Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Addresses are preferred PE32 virtual addresses, not portable object layouts.

This establishes native FCollisionHash's AddActor, RemoveActor and
ActorLineCheck behavior. It is not an implementation, original world startup,
MoveActor, FindBase, falling physics, or playable crate acceptance. In
particular, `native/quest_actor_overlap.h` describes a different pinned-engine
registry and must not be silently reused as the retail hash specification.

## Entry points and native storage

| Operation | Thunk | Body |
| --- | --- | --- |
| FCollisionHash default constructor | `10302991` | `10357AB0..10357C2D` |
| GetHashIndices | `103013E3` | `10357590..10357649` |
| GetHashLink | `1030134D` | `10357680..103576C1` |
| GetActorExtent | `10303A8F` | `10357E30..10357FF1` |
| AddActor | `10302027` | `103580B0..103582C0` |
| RemoveActor | `1030199C` | `103583A0..10358500` |
| ActorLineCheck | `10301348` | `10359160..103594F9` |
| CheckActorNotReferenced | `10301979` | `10359630`, just `RET 4` |
| UPrimitive.GetCollisionBoundingBox | `10302A9A` | `103D8460..103D8534` |
| UModel.GetCollisionBoundingBox | `10301794` | `103F0E70..103F10E6` |
| Core FBox += FVector | `10101BCC` | `10119430..1011951D` |

Native FCollisionHash has a vtable pointer at0, exactly16384 bucket heads
starting at+4, a free-link head at+10004, and an allocated-chunk array at+10008.
Each native12-byte collision link contains Actor at0, Next at4, encoded cell
at8. These pointer representations are not a portable serialization format.

Relevant source Actor native fields are Location at+114/+118/+11C,
ColLocation at+138/+13C/+140, Mesh at+174, Brush at+178, CollisionRadius at+1D4,
CollisionHeight at+1D8, collision flags at+1DC and CollisionTag at+FC.
The runtime must bind corresponding actual object/level identities rather than
reinterpret arbitrary byte offsets or substitute presentation actor order.

## The real grid conversion

All scalar floating arithmetic uses ordinary x87 PC64 nearest-even. `F(v)`
means an explicit IEEE Float store; retained operations are rounded to PC64.
For a supplied coordinate `p`, GetHashIndices computes:

```
scaled = F((p + Float32768) * Float(1/256))
integer = FISTP_int32(scaled)       // x87 rounding mode: nearest-even here
cell = min(max(integer, 0), 16383)
```

The add and multiply do not Float-spill between operations. There is an
explicit Float store and reload immediately before every FISTP. The constants
at1042A040 and1042A03C are respectively bits47000000 and3B800000.

This is not floor, truncation, a signed unshifted grid, a1024-cell wrap, or a
pre-conversion clamp. For example, p=+128 gives128.5 and rounds to cell128;
p=+384 gives129.5 and rounds to cell130. GetHashIndices processes X, Y, Z in
that order. It does not validate the source FVector or extents.

For masked x87 invalid conversions, an out-of-int32-range or nonfinite scaled
value produces the indefinite int32 value80000000, which then follows the
negative clamp to0; it must not be guessed as the high clamp16383. A portable
finite/overflow domain may explicitly refuse such active conversions instead
of emulating invalid-FP state, but must not silently saturate before FISTP.

## Global permutation tables, bucket and encoded cell

The three16384-element native tables are:

- HashX at10555FC4.
- HashY at10545FC4.
- HashZ at10535FC4.

The default constructor initializes them only when global Inited at10565FC4
is zero (`10357AF9..10357C0B`). It sets Inited=1, initializes every table entry
to its own index, then for i=0..16383 performs three swaps in this exact order:

```
swap(HashX[i], HashX[appRand() % 16384])
swap(HashY[i], HashY[appRand() % 16384])
swap(HashZ[i], HashZ[appRand() % 16384])
```

Engine IAT10599BD0 is named Core `?appRand@@YAHXZ`. The original shared
appRand stream and its0..32767 outputs are audited in SCRIPT-RANDOM.md. This
initialization consumes49152 draws once per process, not once per actor,
query, map, or hash instance. It is not a decreasing-range Fisher-Yates loop.
The exact tables depend on the genuine stream state at that native lifecycle
boundary; arbitrary fixed tables, identity tables, or a guessed seed are not
evidence of the original process tables. Later constructor calls zero only
their own bucket heads and do not redraw or reset the global permutations.

For integer cells x,y,z:

```
bucket = HashX[x] XOR HashY[y] XOR HashZ[z]
encodedCell = ((z * 256) + y) * 256 + x
```

The encoded value is made by shifts and **additions**, not nonoverlapping
bitwise OR fields. With14-bit coordinates the shifted fields overlap. The
native does not decode it or assert uniqueness. A link is a candidate only
when both its current bucket traversal and its stored encodedCell match.
GetHashLink writes that encoded value to its OUT parameter and returns the
address of the selected bucket head (`10357693..103576BB`).

## Actual registration bounds

GetActorExtent first selects the source actor's live primitive in this order:
Brush if nonnull, otherwise Mesh if nonnull, otherwise
`actor.XLevel->Engine->Cylinder`. The final fallback is native level/engine
identity, not a fabricated primitive. It invokes primitive virtual slot+64
with the actual Actor as owner, then converts the returned Float bounding box
minimum and maximum using the grid conversion above. Outputs are ordered
Xmin, Xmax, Ymin, Ymax, Zmin, Zmax; scalar conversions execute minimum XYZ
then maximum XYZ (`10357E85..10357FE4`).

Static vtable evidence confirms these concrete bindings:

| Primitive vtable | +58 LineCheck | +64 GetCollisionBoundingBox |
| --- | --- | --- |
| UPrimitive1042655C | 10301B9F | 10302A9A |
| UMesh104290B8 | 1030259F | 10302A9A |
| UModel104265D8 | 10303E5E | 10301794 |

Thus an ordinary Mesh uses the inherited cylinder-derived registration box
even though its own LineCheck implementation is different. A Brush uses the
actual transformed model box, not its actor cylinder.

### UPrimitive owner box has a +1 pad, with retained Y radius

For nonnull owner, UPrimitive.GetCollisionBoundingBox computes at103D846F:

```
retainedR = CollisionRadius + Float1
storedR = F(retainedR)              // FST, does not pop retainedR
storedH = F(CollisionHeight + Float1)

Max.x = F(storedR + Location.x)
Max.y = F(retainedR + Location.y)
Max.z = F(storedH + Location.z)
Min.x = F(Location.x - storedR)
Min.y = F(Location.y - retainedR)
Min.z = F(Location.z - storedH)
IsValid = 1
```

Float1 at10429C50 has bits3F800000. This is **+1**, not the pinned registry's
raw cylinder box or brush+0.1 rule. X reloads the Float radius while Y retains
the pre-store PC64 radius. A null owner instead copies the primitive's native
BoundingBox at+28, but AddActor/GetActorExtent supplies its nonnull Actor.

UModel.GetCollisionBoundingBox obtains the actual owner's virtual ToWorld
coordinates at+88 and transforms all eight corners of the model's stored
BoundingBox. It folds them into a new Float box in X-corner outer,
Y-corner middle, Z-corner inner order (`103F0EE5..103F1080`). It does not
replace this with CollisionRadius/Height or invent an extra pad. Null owner
copies the stored model box. Exact actor ToWorld construction and model
transform acceptance are separate prerequisites; this audit is not authority
to guess them or force the cylinder path when Brush is present.

## AddActor: real membership and insertion order

AddActor first asserts `Actor->bCollideActors` using collision flag bit1
(`103580D6..103580FC`; diagnostic string1047A3D8). It then returns immediately
if Actor flag byte+28 has bit80, the native pending-delete gate. It does not
read bounds, update ColLocation, or insert links on that branch.

On the ordinary branch it calls virtual+20 CheckActorNotReferenced. **In this
owned retail build that method is only RET4**, so it supplies no scan,
duplicate prevention or assertion. AddActor computes the live primitive bounds
described above, and loops inclusive:

```
for x = Xmin .. Xmax
  for y = Ymin .. Ymax
    for z = Zmin .. Zmax
      prepend { Actor, oldBucketHead, encodedCell(x,y,z) } to bucket(x,y,z)
```

The loop is X outer/Y middle/Z inner (`10358141..1035828F`). One link is
inserted for every visited cell, including bucket collisions. Inserting an
actor again does not remove previous links. Links of a later Add are ahead
of earlier actors in a shared bucket; successive same-Add cells that hash
to that bucket are reversed there too. No actor class, bBlockActors,
bBlockPlayers, bHidden or primitive existence fallback gate is applied beyond
the primitive selection already described.

After the loops, Add copies current Location bit patterns into ColLocation
(`10358294..103582AD`), including when an inverted min/max range inserts no
links. The native has no explicit dimension/finite validation. A portable
supported domain must refuse unsafe values, not normalize or repair them.

Links come from a free-list. When empty, a native0x3000-byte chunk is allocated
and its1024 consecutive12-byte links are chained in increasing address order
(`103581A5..10358238`). Add pops the free head and prepends the chosen bucket
link. The allocation address itself is not a portable deterministic identity.

## RemoveActor: recomputed live bounds, not a registration snapshot

RemoveActor likewise asserts bCollideActors first, then skips everything if
native pending-delete flag bit80 is set (`103583C1..103583F3`). It compares
Location to ColLocation XYZ. If any component differs, it only logs
`%s moved without proper hashing` at1047A420; it does not restore Location,
scan all buckets, repair membership, or use ColLocation as removal bounds.
The comparison is numerical equality, not bit equality; signed zeros compare
equal. Unsupported NaNs must not be treated as a supported movement protocol.

It calls GetActorExtent on the **current primitive and current coordinates**,
then loops the same inclusive X/Y/Z cells (`10358473..103584E6`). For each
selected bucket it walks the entire chain with a pointer-to-pointer:

- If link.Actor differs, advance the cursor to that link's Next field.
- If link.Actor matches, unlink it and push it onto the free-list. Continue
  from the same pointer-to-pointer, removing every matching Actor in that
  bucket. **No encoded-cell comparison occurs in removal.**

The same bucket may therefore be revisited by another cell, and an Actor link
registered in a different encoded cell is still removed whenever its bucket
is visited. Old links in buckets never reached by the recomputed bounds can
remain. This is not the pin's removal using a cached registration box, and
not an all-buckets actor purge. After the loops it calls the retail no-op
CheckActorNotReferenced. It does not update ColLocation.

## ActorLineCheck swept query bounds

Arguments are MemStack, End, Start, Extent, BYTE flags. The function increments
global CollisionTag at10565FC8 **before** building the query box. It starts an
invalid box, applies Core FBox += **Start first, then End**, and expands it
by supplied Extent. In Core's finite ordinary comparisons, equal components
keep the old box value; signed-zero ties therefore retain Start for both
minimum and maximum. A generic min/max with a different equality selection
is not the exact bit-level construction.

Let that stored Float endpoint box be Min/Max and Q be Extent. The actual
query grid inputs have asymmetric intermediate stores (`103591F9..10359331`):

```
minY = F(Min.y - Q.y); minZ = F(Min.z - Q.z)
Xmin = gridRetained(Min.x - Q.x)
Ymin = grid(minY); Zmin = grid(minZ)

maxZ = F(Max.z + Q.z); maxY = F(Max.y + Q.y)
Xmax = gridRetained(Q.x + Max.x)
Ymax = grid(maxY); Zmax = grid(maxZ)
```

`gridRetained` uses the retained subtraction/addition as input to +32768 and
*1/256, with its first Float store only immediately before FISTP. It is not
`grid(F(Min.x-Q.x))`. Conversion order is minX/minY/minZ, maxX/maxY/maxZ;
the separate Y/Z expansion stores execute in the order shown.

The original contains no sweep DDA and no bCollideActors, deletion, actor
class, source-owner-chain, bBlockActors, bBlockPlayers or TraceFlags filtering
at this stage. Those are either registration state or higher-level query
consumer responsibilities. Negative/inverted/unsupported query extents are
not permission to substitute a different spatial algorithm.

## Candidate order, tags and hit-list construction

The query visits inclusive X outer/Y middle/Z inner, then each selected
bucket chain head-to-tail (`1035933C..103594E1`). For every link:

1. Read link.Actor and its native CollisionTag. If equal to the current
   global CollisionTag, skip this link (`103593A3..103593B3`).
2. Compare link.encodedCell to the current cell's shifted-add encoded value.
   Skip on inequality (`103593B9..103593BC`). A mismatched encoded cell does
   **not** tag the Actor.
3. Create a fresh FCheckResult with Next=None, Actor=None, Location=(0,0,0),
   Normal=(0,0,0), Primitive=None, **Time=+0**, Item=-1.
   Time initialization at10359401 is0, not1.
4. Write Actor.CollisionTag=current global tag **before** primitive dispatch.
5. Select live Brush, else Mesh, else actual XLevel.Engine.Cylinder, and call
   its virtual+58 LineCheck(Hit, Actor, End, Start, Extent, flags&255).
6. If primitive returns clear/nonzero, append nothing. The Actor remains
   tagged, so later links in this query do not call it again.
7. If primitive returns blocked/zero, copy the entire44-byte hit into
   MemStack storage aligned to8, then overwrite its Next with the previous
   result head and make it the new head (`10359494..103594BE`).

The returned list is therefore the **reverse of successful primitive
encounter order**. It is not sorted by time or actor address. No64-hit limit
exists in ActorLineCheck itself; ULevel.MultiLineCheck's later bounded
collection/sort contract is separate. The function does not validate that a
primitive returned Actor==link.Actor before copying its hit in this build.

The tag is process-global across hash instances. It increments with native
32-bit wrap; the owned query body performs no reset scan on wrap. It is not
a persistent monotonic64-bit actor identifier. A portable detached query may
model the observed deduplication without native pointers, but lifecycle and
rollback must be explicit if it mutates authoritative actor tags/global state.

## Bounded portable foundation and non-substitutions

The retail body has no explicit cell/link/candidate budget. A bounded portable
implementation should preflight/tick cells, every visited link (including
wrong-cell/already-tagged links), primitive callbacks, hits and allocations.
All limits must produce explicit root-transaction refusal, never a truncated
clear query or an unannounced all-actors fallback. A16K-per-axis clamp still
allows an enormous inclusive cell product, so overflow-safe checked products
and bounded traversal are required.

Preserve valid registration history, duplicate Add behavior, current-bound
Remove behavior, within-bucket order, encoded-cell aliases, first real
candidate tagging and reversed output list. Do not turn an unsafe pointer,
cyclic link, missing genuine primitive, unsupported model transform or
unavailable process-table lifecycle into a successful cylinder approximation.
Every failure must restore authoritative hash membership/free-list, native
tags, shared RNG draws, partial results and actor writes in the same root
transaction where relevant. Detached implementation/testing packets must not
be described as original runtime startup or collision integration.
