# Original collision hash registration and swept query

Read-only static audit of the user-owned Deus Ex GOTY Engine.dll and Core.dll,
2026-10-16. No original DLL or EXE is loaded or executed. Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Addresses are preferred PE32 virtual addresses, not portable object layouts.

This establishes native FCollisionHash's AddActor, RemoveActor and
ActorLineCheck behavior. The detached implementation is recorded below; it
is not original world startup,
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
minZ = F(Min.z - Q.z); minY = F(Min.y - Q.y)
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

1. Read link.Actor and its native CollisionTag, then reload the process-global
   CollisionTag for this link. If equal, skip this link
   (`103593A3..103593B3`). This is not the query-entry tag captured once.
2. Compare link.encodedCell to the current cell's shifted-add encoded value.
   Skip on inequality (`103593B9..103593BC`). A mismatched encoded cell does
   **not** tag the Actor.
3. Create a fresh FCheckResult with Next=None, Actor=None, Location=(0,0,0),
   Normal=(0,0,0), Primitive=None, **Time=+0**, Item=-1.
   Time initialization at10359401 is0, not1.
4. Reload the process-global tag again and write Actor.CollisionTag=current
   global tag **before** primitive dispatch (`103593F2..1035940F`).
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

After primitive dispatch and any hit copy, traversal loads **the current
link's live Next field** at103594C3. It does not retain the pre-dispatch Next,
restart from the bucket head, or traverse a snapshotted candidate vector.
Thus a nested query can change the global tag seen by subsequent outer links;
a callback's supported removal/reuse can change the Next that the outer walk
follows. The encoded query cell is restored from the outer stack, but that
does not freeze the link, Actor tag, process tag or subsequent bucket heads.
These observable reloads require shared live query/mutation state and a common
root rollback boundary, not one private deduplication set per query. Unsafe
link reuse/cycles must explicitly refuse within the portable work budget.

The tag is process-global across hash instances. It increments with native
32-bit wrap; the owned query body performs no reset scan on wrap. It is not
a persistent monotonic64-bit actor identifier. A portable detached query may
model the observed deduplication without native pointers, but lifecycle and
rollback must be explicit if it mutates authoritative actor tags/global state.

## Native hash lifetime and the real map-load boundary

The original native owner is ULevel.Hash at+F8, not a reflected Actor.Level
value or a renderer registry. ULevel vtable10428AFC binds SetActorCollision at
slot+58 to thunk103013C0, body1039D390..1039D498. Its four cases are:

| Requested state | Existing Hash | Native operation |
| --- | --- | --- |
| Enable/nonzero | None | Construct, publish Hash, register current actors |
| Enable/nonzero | Present | No operation |
| Disable/zero | None | No operation |
| Disable/zero | Present | Remove current actors, delete Hash, set None |

Enable with no Hash calls exported GNewCollisionHash thunk10301B59 at
1039D3D1. Its body103579E0..10357A4B allocates0x10014 bytes and invokes the
default FCollisionHash constructor at10357A23. SetActorCollision stores the
returned pointer at1039D3D6 **before** registration. Starting at native
Actors slot0, it rereads Actors.Num at Level+30 and the actor-array pointer
at+2C, visiting ascending slots. It calls AddActor only for a nonnull Actor
whose bCollideActors bit1 at+1DC is set (`1039D3DC..1039D404`). There is no
additional class, hidden, block, pending-delete or primitive-fallback gate
here; AddActor retains its own pending-delete gate and primitive authority.
The original allocator can return None; this is not authority for a portable
allocation failure to become a successful empty hash.

Disable with an existing Hash enumerates current native Actors in the same
ascending, reread-count order and calls RemoveActor on the same nonnull /
bCollideActors condition (`1039D447..1039D46B`). It then calls the deleting
destructor and clears Level+F8 (`1039D46D..1039D483`). This is not an all-bucket
repair or removal using previously cached actor bounds.

UGameEngine.LoadMap thunk103033A5, body1038C1F0, calls the newly selected
current Level's virtual SetActorCollision(1) at1038D1ED..1038D1FB. This is
**before** the64-by-64 zone-distance work at1038D202, GameInfo birth at
1038D59D, and the later InitExecution / InitGame / startup passes. Therefore,
if that call creates the first hash of the process, its49152 raw appRand draws
occur at this real LoadMap boundary, before GameInfo/startup callbacks. They
do not belong to the first trace, first actor interaction, or an arbitrary
portable package decode. The audited bodies do not prove that every possible
engine/editor entry path first creates its hash through this LoadMap call;
the process-global Inited gate, not an assumed first-map identity, decides.

The inspected ULevel default constructor10345D30..10345E96, public
ULevel(UEngine*,int) constructor1039CAC0..1039CD6A, and PostLoad body1039D230
do not themselves call this factory or allocate a hash. This negative
observation is bounded to those bodies; it is not a substitute audit of
UObject allocation/zeroing or every native constructor. ULevel.Serialize
1039D560..1039D7FF and its ULevelBase.Serialize1039C5F0..1039C78E archive the
level/model/actor graph but do not archive Level.Hash+F8. A native hash must not
be invented by copying an archived host pointer or by decoding a map alone.

ULevel.Destroy thunk10301488, body1039D900..1039D96B, directly calls the
existing Hash's deleting destructor at1039D936 and clears Level+F8 at
1039D938. Unlike SetActorCollision(false), Destroy performs **no** preliminary
RemoveActor enumeration. FCollisionHash destructor10357C90..10357DAB frees
its allocated link chunks and chunk array. Neither method resets global
permutations, Inited or CollisionTag; creating a subsequent hash likewise does
not reset the tag.

### Process state, actor tags and persistence limits

Inited, CollisionTag and all three permutation arrays lie in the PE .data
section's virtual-only zero-filled region: RVA179000, raw sizeD000, virtual
size11F2F4. Their initial process values are consequently zero; this is not a
map/default-property seed. Only the default constructor references Inited in
the audited Engine .text; it sets the gate before drawing. All four hash
query families share the same32-bit CollisionTag global:

| Query | Body | Global increment/read references |
| --- | --- | --- |
| ActorPointCheck | 103585B0 | 10358725,1035872E,1035878F |
| ActorRadiusCheck | 103589E0 | 10358B54,10358B5D,10358BBF |
| ActorEncroachmentCheck | 10358D50 | 10358E15,10358E24,10358E8F |
| ActorLineCheck | 10359160 | 10359182,1035919E,103593AD,103593F6 |

Original Engine.u actually declares Actor.CollisionTag: export reference4756,
IntProperty, dimension1, flags00002002, next field4753. ColLocation is
StructProperty reference4743, dimension1, flags00000002. These are genuine
original schema identities, not permission to expose a fabricated tag field
or to restore process-global query history from an actor record. AActor's
Serialize body10369AD0..10369B12 delegates UObject.Serialize; the complete
original UObject property save/load filters are outside this bounded audit.
No claim of original savegame compatibility or cold/warm tag reconstruction
is made from the raw property flags alone.

The current portable InitializePortableRuntime constructs the script session
and owns its explicit shared RNG seed. LoadPortableRuntimeMap links actual
ULevel / actor slots and separately builds the pinned ActorCollisionRegistry
in RelinkAuthoredActorBases. Neither operation is evidence that original
SetActorCollision has run. The non-publishing ProbeWorldStartup currently
begins GameInfo selection after world validation and does not implement this
earlier native hash-enable phase. Future integration needs the real native
level/engine/primitive edges, current slot-order registration, genuine shared
appRand state at the verified lifecycle boundary, and rollback of the entire
initialization. A detached hash packet cannot silently supply any of these.

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

## Detached scalar and state implementation (2026-10-16)

`native/quest_collision_hash_math.h` now implements shifted grid conversion,
explicit Float/reload/FISTP nearest-even signed32 conversion before clamp,
retained-X swept bounds, Start-first signed-zero box folding, the original
+1 primitive box/retained-Y radius, inclusive range products and shifted-ADD
encoded cells. Nonfinite active inputs, unsafe FISTP/Float storage and exhausted
work bounds explicitly refuse, not pre-saturate into a plausible grid value.

The explicit `InitializeOriginalCollisionHashTables(sharedSeed,drawBudget)`
performs the49152 X/Y/Z-order shared CRT draws and fixed-index swaps. Its local
seed commits only after the complete successful operation. It has no guessed
initial seed, hidden per-map invocation or implicit initialized gate. The
owning native lifecycle must still establish when it runs and journal the
shared RNG plus process initialized state. Independent uint64 CRT recurrence,
four full-table fingerprints and late draw-budget controls verify this helper.

`native/quest_collision_hash.h` implements a bounded detached process domain
with immutable validated permutation input, shared32-bit tag, opaque-handle
actor storage and independent hash instances. Head arrays and1024-slot chunks
use bounded heap allocations. One-based stable link handles preserve native
chunk/free reuse order; they are not byte addresses or saved UObject pointers.
Fresh links track undefined Actor/cell provenance separately from Next. Query
continuation into never-initialized metadata explicitly refuses rather than
treating zeroed host memory as a valid native Actor.

Implemented operations preserve duplicate Add/prepend order, current-bound
Remove/all Actor matches without encoded filtering, stale freed payload,
LIVE post-bounds Location-to-ColLocation copy, pending-delete short circuits,
original Time0 hit seed, per-link global-tag reloads, byte flags, reversed
output links and entire primitive-provided hit packets (even a different
returned Actor). Query vectors are copied at entry. There is no live collision,
deletion, class, source or block filtering added to registered query membership.

Nested query and supported registration mutation remain live, including
current-node reuse. Bounds/cell ranges stay captured while each later bucket
head and post-callback current-slot Next are reread. Storage generation checks
refuse destruction/replacement of the active owner/hash, including same-handle
actor resurrection; pending-delete state alone is not storage destruction.
Diagnostic Add/Remove/profiling counters and the moved-without-hashing log are
not modeled as gameplay state. No stale-membership repair is invented.

All transactional operations join one outer owned-state snapshot/work ledger.
A caught nested operation or invalid diagnostic read poisons that root. Any
later refusal restores actor live fields/ColLocation/tags, all hash heads,
chunks/free/stale metadata, new/destroyed storage and global tag. `WithRoot`
groups a complete caller operation so several successful natives can still
roll back after its last effect fails. Value results must be non-reference,
nothrow-movable/destructible, preventing a post-commit return failure. ReadHash
returns a detached heap-backed copy, not a live borrowed alias; copy-allocation
failure also poisons an active root.

Fixed maximums are65536 actors,64 hash instances,1048576 total allocated links,
262144 cells,4194304 work steps,1048576 hit allocations and64 nested operations.
Callers may only tighten these. Cells, steps, hits and nesting are cumulative
through the SAME root, not renewed in callbacks. Every visited cell/link,
including wrong-cell and already-tagged links, is charged. A chunk operation
also charges all1024 initialized Next slots. Explicit overflow-safe range
preflight and visit-time cumulative checks cover nested consumption.

External primitive services remain explicit providers: bounds must invoke the
actor's actual selected native primitive, and line must preserve Brush/Mesh/
Cylinder priority and actual owner transforms. Provider external script,
RNG/lifecycle, geometry and publication effects must join the eventual SAME VM
root; this detached owned-state snapshot does not journal arbitrary external
callback writes. The current runtime still uses the separately documented
pinned overlap registry. This checkpoint does not silently replace it, enable
native SetActorCollision, Move/FindBase/falling or bypass original startup.

## Verification checkpoint (2026-10-16)

- Scalar/table suite175138 checks/11306 explicit safety refusals/81630
  independent hardware-x87 comparisons passes. Hardware FISTP and staged Float/
  PC64 math are independent of production integer conversion; no original
  executable code is called. Dense tie/nextafter/signed-zero and broad finite
  exponent inputs, whole permutations and atomic seed refusals are covered.
- State suite10778465 checks/22 root refusals passes600 independent mutation
  operations and171 vector-oracle queries. The independent oracle uses bucket
  and free vectors, not production linked-list code. It checks every head,
  link Next, initialized/stale payload, allocation count and free position.
  Tests cover duplicate/stale/current-bound removal, encoded aliases, wrong-cell
  untagged encounters, wrap, nested same/other-hash live tags, follower removal,
  current-slot reuse and fresh later-cell heads, copied arguments, undefined
  storage refusal, callback destruction/replacement, late whole-root rollback,
  caught failure poisoning and shared work/cell/depth/hit/chunk limits.
- Actual pure cylinder bounds and LineCheck run through the hash for70 distinct
  actors: all70 dispatch/hits survive with exact standalone geometry packets
  and reversed encounter links. Feeding that complete hash list to the tested
  MultiLine collector preserves all70 primitive calls, then admits/sorts only
  its first64 records. This proves combined detached services, not original
  loader/VM primitive identity, on-device gameplay or crate acceptance.
- An initial Windows state-test stack overflow was traced by debugger to large
  fixture snapshots, then corrected by bounded heap-backed head/table storage.
  Strict ARM64 checks also identified two x86-only oracle helpers outside their
  compiler guard; they were moved into that guard without loosening warnings.
  Production finite/domain semantics were not relaxed to pass these repairs.

The coherent full host build and strict ARM64 syntax pass. CTest114 passed /
129 registered /15 explicit owned-data skips /zero failures in32.64s.
Original Engine/Core/Engine.u SHA256 values were rechecked unchanged. Evidence
logs are ignored local `artifacts/collision-hash-*-20261016.log`. No new APK or
native world/physics integration is claimed.
