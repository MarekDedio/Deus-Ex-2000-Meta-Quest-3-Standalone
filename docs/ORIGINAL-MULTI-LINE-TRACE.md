# Original ULevel.MultiLineCheck contract

Static, read-only audit of the retail GOTY Engine.dll and its imported sort
chain. No original executable or DLL was executed. This document does not
claim an implemented native world collision service, Move266, FindBase or
falling physics. The verified pure null-owner Model kernel is only one input
to this operation.

Primary binary identities:

- Engine.dll SHA256
  `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- Core.dll SHA256
  `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
- Owned bundled MSVCRT.dll SHA256
  `E390B0529FB9EE1214BB68302A4658B578134D92AB76BC5EB48F3BADD843D210`.

## Entry and native packet

Export `?MultiLineCheck@ULevel@@UAEPAUFCheckResult@@AAVFMemStack@@VFVector@@11HPAVALevelInfo@@E@Z`
thunk`0x10302AA9` reaches body`0x1039B220`–`0x1039B7C1`.
ULevel virtual`+0xC8` receives:

```
MultiLineCheck(MemStack, EndFloat3, StartFloat3, ExtentFloat3,
               CheckActorsInt, suppliedLevelInfoOrNone, ExtraNodeFlagsByte)
```

There is no source actor argument. Owner/base/self/blocking filtering happens
in the caller (see ORIGINAL-ACTOR-MOVE.md), not here. A source-excluding ray
query therefore cannot replace this packet.

The local collector has64 FCheckResult records, each44 bytes. A nonnull world
hit occupies the first slot before actor hits. Field layout is Next`+0`,
Actor`+4`, Location`+8..10`, Normal`+14..1C`, Primitive`+20`, Time`+24`,
Item`+28`. The operation copies whole records; it does not reconstruct Item
or Primitive from a render triangle identifier.

The entry loop at`0x1039B24C`–`0x1039B261` advances by44 bytes64 times but
contains no hit-field stores or constructor calls. In particular the first
world packet is **uninitialized** before Model.LineCheck. An empty Model
returns RootOutside without writing Hit; RootOutside=false therefore leads
the retail collector to read undefined Location/Time after writing Actor only.
A future portable Model adapter must retain per-field provenance and refuse
undefined observed fields, not seed a fabricated usable packet. Unobserved
Item/Primitive/Next payload is not permission to claim an authored surface
index; extent checks do not write Item. The actor hash has its own distinct
explicit zero-Time seed, documented in ORIGINAL-COLLISION-HASH.md.

## 1. Optional world Model collision and shortening

At`0x1039B263`–`0x1039B400`, initialize Scale Float1, HasWorldHit=false,
NearWorldHit=false and count0. If suppliedLevelInfo is nonnull, obtain
**that object's native XLevel.Model**, call its primitive LineCheck with
ownerNone, original End/Start/Extent and zero-extended ExtraNodeFlagsByte.
This need not be the same ULevel as the receiving object's actor hash.
Do not substitute reflected level membership or an arbitrary loaded Model.

If LineCheck reports clear, append no world hit and leave End/Scale unchanged.
If blocked, set the result Actor to the supplied LevelInfo identity, set
HasWorldHit and retain the other primitive-provided fields. Compute D from
the **unspilled** componentwise Hit.Location−Start differences:

```
D = appSqrt(StoreDouble((dx²+dy²)+dz²))  // PC64 sqrt result retained
scaleRetained = ((D+Float5)*WorldHit.Time)/(D+Float0.0001)
Scale = Float(min(Float1,scaleRetained))
```

The FST Double D at`0x1039B348` is not a reload; its retained root is reused.
There is no lower clamp on Scale. Constants are Float5`40A00000` at
`0x1042EAE4`, Float0.0001`38D1B717` at`0x1042D544`, Float1`3F800000`.

Shorten local End to Start+(originalEnd−Start)*Scale with original spill
boundaries: X/Y differences Float-store, their scaled offsets Float-store,
then Float-store sums with Start. Z difference and scaled Z offset remain
retained until adding Start and Float-storing End.z. This is not a uniform
double Lerp or a Float3 product on all three axes.

NearWorldHit is true only if WorldHit.Time<**Double0.01** AND retained D<
Float30. Double0.01 is`3F847AE147AE147B` at`0x10429858`; Float30 is
`41F00000` at`0x1042EAE0`. WorldHit.Time itself is not multiplied by Scale
or rewritten by the shortening phase. Finally increment collector count.

## 2. Actor collision query and fixed collector admission

At`0x1039B401`–`0x1039B484`, only if CheckActorsInt is nonzero and the
receiving ULevel's **native CollisionHash** exists, call hash virtual`+0x10`
(ActorLineCheck) with supplied MemStack, shortened End, original Start,
original Extent and original final byte. Otherwise proceed directly to sort.

Walk the returned linked hit records in their existing order while nonnull
and count<64. Do not re-sort this input before the collector's rules. When
NearWorldHit is true, admit **only actual Engine.Mover descendants**; skip
all other actor hits. This is a genuine original filter, not a generic
distance-to-wall clamp.

For an admitted hit, the mover contact correction below may change its
Location and Time. Then multiply that actor hit's Time by stored Float Scale,
store Float into the input record, copy all44 bytes into the next collector
slot, increment count and follow its original Next. Skipped hits do not
consume collector slots. At64 total slots, stop without inspecting remaining
links. A portable root must independently bound ActorLineCheck traversal/work;
it must not silently promote the collector cap into an unlimited hash query.

## 3. Coplanar nearby mover correction

At`0x1039B4ED`–`0x1039B6A2`, correction occurs only when HasWorldHit,
candidate actor has actual Engine.Mover ancestry, candidate Normal xyz equals
WorldHit.Normal xyz by exact Float comparisons, and the retained squared
distance between their Locations is strictly less than Float4
(`40800000`, `0x1042EADC`). No surface-name or approximate normal tolerance
replaces these predicates.

Let Q=shortenedEnd−Start. The code stores Q.z and Q.y Float. It FSTs Q.x
to Float without popping the retained difference. The length calculation is
then:

```
squareRetained = (retainedQx * storedQx + storedQy²) + storedQz²
L = appSqrt(StoreDouble(squareRetained))
storedLength = Float(L)                  // FST without reload
inverseRetained = Double1 / L
```

Q.y/Q.z times the retained inverse Float-store; Q.x multiplication reloads
storedQx but remains retained. Double each normalized component and store all
three Float offsets. Replace candidate Location by componentwise Float
`WorldHit.Location - doubledDirection`.

Next form Float3(candidate.Location−Start), calculate retained
`(Dz²+Dy²)+Dx²`, store Double and appSqrt. Divide its retained root by
storedLength, Float-store candidate.Time. Then the ordinary collector phase
multiplies Time by Scale. Candidate Normal/Actor/Item/Primitive remain as
provided. A zero length can produce retail masked nonfinite arithmetic; a
finite bounded embedding must explicitly refuse it instead of inventing a
usable fraction or treating the actor as clear.

## 4. Final sort and linked output

If count0 return None. Otherwise at`0x1039B70F` call imported **Core.appQsort**
(IAT`0x10599ED0`) on the local44-byte records with the actual count and
comparator export thunk`0x10303684` →`0x1039C060`–`0x1039C08E`.
The comparator compares stored Float Time ascending: −1 for less, +1 for
greater,0 for equal. It has no Actor pointer, world-first or original-order
tie break.

Allocate count*44 bytes from the supplied MemStack at alignment8, copy sorted
records, overwrite each Next with the next allocated record and final Next
with None, return the first. There is no separate non-gameplay deduplication.

### Sort dependency, including ties

Core.appQsort thunk`0x1010194C` →`0x10123640`–`0x1012365D` forwards the
four arguments unchanged to imported MSVCRT.qsort (Core IAT`0x102AB51C`).
The supplied bundled MSVCRT export RVA`0x2543D` has imagebase`0x78000000`:

- main qsort`0x7802543D`–`0x780255E1`;
- small sort`0x780255E2`–`0x78025639`;
- byte-record swap`0x7802563B`–`0x78025666`.

For ranges of at most8 records it uses **unstable maximum-selection sort**:
choose the first maximum using strict comparator>0, swap it with high, reduce
high, repeat. For larger ranges it swaps the middle record with low, uses
that low record as pivot, scans left while<=pivot and right while>=pivot,
swaps un-crossed pairs, then swaps pivot with the final right cursor. It
pushes the larger partition and immediately processes the smaller partition
on a bounded explicit stack. Exact partition boundaries use byte distances;
for44-byte records the branch compares `right-low-1` to `high-left`.
No stable sort or pointer tie rule is justified by this code.

The bundled DLL establishes a reproducible owned dependency, but the original
Windows loader may choose a system/KnownDLL MSVCRT instead. This audit did not
execute the game or inspect a loaded process's module resolution. A portable
sort adapter must name/pin its original CRT policy, independently test tie
permutations at the8/9 boundary and64 cap, and avoid claiming universal retail
tie behavior across unidentified CRT versions.

## Implementation and evidence boundary

The complete bounded Engine function was read through its terminal return;
its dependencies were inspected only to the scope stated above. The detached
collector below now implements its arithmetic/admission/ordering, not the
native primitive/hash services or movement integration. Required work remains:

- native ULevel/Model/CollisionHash identities and memstack lifetime authority;
- actor hash traversal, original actor-cylinder/brush primitive selection,
  mover transforms, candidate ordering and bounded query budgets;
- exact shortening/mover arithmetic and explicit nonfinite-result policy;
- pinned sort permutation and independent CRT algorithm controls;
- whole-root failure rollback when used inside Move/FindBase/physics.

The collector itself does not mutate actors, reflected Location, Base or
Touching. Mutations to temporary input hit records are not actor-transform
publication. No fake block/clear result, sorted render collider list, fixed
rootModel guess or stable sort can establish this native contract.

## Detached collector and pinned-sort checkpoint (2026-10-16)

`native/quest_multi_line_trace.h` implements the complete provider-driven
collector: world call first, original fraction/end shortening, strict near
filter, actual-ancestry provider calls in original order, mover correction,
temporary actor-hit Time mutation before copying,64-record admission,
bundled-CRT sorting and regenerated output links. The supplied world identity
labels only the world hit; it is not used as a source-exclusion filter.

Providers remain explicit dependencies. World must query supplied native
LevelInfo.XLevel.Model with ownerNone and zero-extended byte flags; actors must
use the receiver's actual CollisionHash and return its complete native order;
isMover must use actual class ancestry. No stub provider, first-Model guess,
render ray, class-name match or pin-based overlap registry establishes these
contracts. Per-provider work and all failures must share the eventual caller's
VM-root ledger/transaction. The collector itself dispatches no script events
and writes no UObjects; it owns temporary hit copies and returns no partial
result on refusal.

Detached arenas use one-based portable Next/first handles, zero=None. They
are not original byte addresses or saved pointers. Active links alone are
checked; unvisited tails after64 admitted records remain unread. A repeated
admitted record is scaled again before each copy, matching temporary native
aliasing; skipped cycles exhaust the collector work budget. Fixed maximums
are1048576 temporary actor records and4194304 collector steps; callers may
only tighten them. Providers must also bound allocation/query work before
returning an arena. No unbounded fallback, silent truncation or success on
unsupported primitive/transform data is introduced.

`native/quest_collision_hit_sort.h` pins the owned bundled-MSVCRT permutation
policy above. Whole packets move with cached immutable Float Times. Count0/1
does not read Time. Count>=2 validates all Float Times before mutation as an
explicit finite-domain policy; it does not claim retail's lazy comparator
input-access behavior for malformed nonfinite data. The64-record cap is
checked before reads. The44-byte partition-size comparison becomes **strict
`right-low > high-left`** at index granularity, not >=.

Independent byte-record sort tests pass4461952 checks/143179 permutations/
250 explicit refusals: exhaustive ternary ties through10, unique permutations
through8, signed-zero patterns, all counts0..64, finite-bit stress, exact
8/9/64 tied fixtures and whole-payload preservation. No original CRT code is
executed. Independent hardware-x87 collector tests pass1507683 checks/
18 explicit safety refusals:14000 generated queries,13994 finite successful
comparisons,59881 mover corrections and719 near-world filters. Complete
packet arithmetic and tied-group payloads match the independent reference;
the separate byte oracle proves exact tied permutations.

Deterministic controls cover copied native by-value inputs across providers,
input/provider order/flags, inactive NaNs,
XYZ short-circuit comparisons, negative unclamped scale, exactly/above1 clamp,
Double.01 and distance thresholds, equal normals/distance4 boundaries,
zero-length active/inactive correction, arena ownership, non-storage Next
order, repeated-record scaling, both64-slot caps, ignored malformed tails,
skipped cycles, missing/throwing services and tightened budgets. Broad finite
Float exponents exercise overflow and retained-versus-spilled interpolation.
This is not an on-device or native world/startup/physics acceptance.

The coherent full host build and strict ARM64 syntax pass. CTest112 passed /
127 registered /15 explicit owned-data skips /zero failures in37.75s. Engine,
Core and bundled MSVCRT hashes were rechecked; original executable code is
never loaded. Ignored local evidence: `artifacts/multi-line-*-20261016.log`.
The production runtime/accepted NPC APK are unchanged at this checkpoint;
no fake hash/owner-transform provider is wired into gameplay.

## Actual first-Training service seam (2026-10-18)

The current runtime now binds this collector to actual current-Level Model and
native hash providers through bounded rollback-only diagnostics. No renderer
ray or source-exclusion filter replaces those services. The embedding currently
admits only actual current native slot-zero LevelInfo as world input; retail's
other-Level supplied-world possibility remains outside that supported domain.
World extent Item stays explicitly unknown; a deterministic `-1` payload is a
portable undefined-field policy, not an authored surface index. Blocked empty
world packets with undefined observed Time/Location refuse.

An optional enclosing-root work hook now charges every admitted collector step,
including before later failure. Independent shared-ledger controls prove no
fresh per-call allowance or partial result publication. Real first-map tests
compare actual loader-Model packets and independently ordered actor packets,
including genuine crate-anchor admission, world shortening and full rollback.
See TRAINING-RETAIL-COLLISION.md for current evidence and exact boundaries.
Move/FindBase/physics/startup publication remain unfinished; no APK or playable
crate acceptance is claimed for this prerequisite.
