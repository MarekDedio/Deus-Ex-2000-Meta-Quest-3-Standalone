# Original MoveActor based-child carrying

Static source audit, 2026-10-19. Scope is the prerequisite for first
`00_Training` movement only. No commercial DLL/EXE was loaded or executed;
no map was loaded or integrated. This document establishes the complete
child-carry segment, not a working native MoveActor, player movement,
platform physics, crate gravity or headset acceptance.

## Authority and bounded evidence

The parent operation is the actual exported ULevel.MoveActor identified in
ORIGINAL-LEVEL-MOVE-ACTOR.md: Engine export1298, thunk `10303C60`, body
`103990E0..10399F9F`. The complete based-child segment is
`103998A0..10399BB5`, read without missing branch bodies from ignored
`artifacts/training-move-actor-20261019/move-actor-full.asm`.

Owned Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Core.dll SHA256 is
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
All addresses are preferred original PE32 virtual addresses, not portable
object offsets or authorization to call original code.

Supporting complete arithmetic authority was reread from
`artifacts/decal-native-20261012-render/core-coords-mulrot-body.asm`,
`1011AC80..1011B08C`, and its exported thunk `10101596`.
ORIGINAL-BRUSH-COORDINATES.md and ORIGINAL-SINE-PROFILE.md establish the
separate composition/UnitCoords/table contracts and their verification and
provenance limits. The current value helpers were also read directly; their
names alone were not treated as an arithmetic proof.

The two-word constructor called at `10399A61` was independently statically
checked: thunk `10303085 -> 10359730..1035973F` writes the two passed words
at receiver+0/+4 and returns8. Both arguments here are zero. Its nearest
disassembler `execGetNextInt` label is not the constructor's identity.

## Inputs and outer gates

Use these values from the COMPLETE parent MoveActor operation:

- `L`: its retained original native ULevel receiver.
- `S`: its retained source actor, not the scratch source-argument stack slot.
- `A`: established actualDelta from the parent's sweep/backoff, stored Float3.
- `R`: the parent's original by-value requested Pitch/Yaw/Roll Int words.
- `bTest`: original signed native integer; any nonzero is true.

`103998A0` reads source StandingCount BYTE at native+D8 exactly once at this
segment's entry. Zero skips the entire child scan to `10399BB5`. It does not
read arrays or child poses and does not constrain the eventual scan count.
When nonzero, the next `bTest` test at `103998AE..8B3` jumps straight to
parent mutation `10399C9A` if nonzero, bypassing both carrying and the higher
CheckEncroachment call. A zero StandingCount instead reaches the later
ordinary bTest check in the parent operation.

No child class, deletion, collision, mobility, ownership, identity/self or
pose-equality filter is added at this gate.

## Live Level scan and retained child authority

Index starts at signed Int0. At EVERY iteration `103998BB..8CD`:

1. Read current L.Actors.Count, native+30; compare current index using signed
   `JGE`. Equal/exceeded exits the scan. Retail would also immediately exit
   on a negative count. A real portable array provider normally establishes
   its authentic nonnegative bounded count separately.
2. Only when in range, reread current array-data pointer at L+2C, then load
   the current slot at that index ONCE.
3. Retain that actor C. A null slot advances the index without a Base read.
4. Read C.Base at native+C4. Only exact Base==S enters carrying.

After the child move and optional Pawn.ViewRotation writes, increment the
saved current index, then reload count/array/slot for the next iteration.
There is no initial array snapshot, initial-count bound, deduplication,
StandingCount-times loop, restart after mutation, or rescan of current slot.

Recursive child moves can change Base, append/remove/replace world slots,
change the source pose or invoke events. Later iterations see those changes.
The current retained C remains authoritative AFTER its recursive move even
if its slot was replaced/removed or its Base changed. There is no second
slot/Base/validity/deletion check before class/ViewRotation processing.

In particular no explicit C!=S check exists. An authentic self-Base relation
would enter recursion; cycles/unbounded appends require a shared bounded
portable refusal rather than a new ordinary-success skip. Current-generation
lifetime admission is likewise a separate portable safety policy, not a
retail bDeleteMe branch.

## Per-child parent rotation sampling and TWO yaw deltas

For every qualifying child, `103998E7..9992A` does the following anew:

1. Read source current Yaw (`103998EA`, native+124).
2. Read source current Pitch (`103998F0`, native+120).
3. Save full raw yaw difference:
   `rawYawDelta = Wrap32(R.Yaw - sampledSourceYaw)`.
4. Initialize rotationalDelta XYZ to raw Float positive-zero words.
5. If requested Pitch differs from sampled source Pitch, enter rotation math.
6. Else compare requested full raw Yaw with the ALREADY sampled source Yaw;
   if different, enter rotation math.
7. Only if BOTH comparisons equal, read current source Roll (`10399922`,
   native+128), compare it with requested Roll, and enter math if different.
8. When all three compare equal, skip ALL offset/GMath/transform math.

Thus sampling a convenience Rotation XYZ packet, reading Roll eagerly, or
reading Yaw again for the equality check changes the original lazy schedule.
Pitch-only and Roll-only changes still enter the SAME yaw-only math branch.

The matrix's separate yaw is constructed at `10399930..957`:

```
quantizedYaw = Wrap32((U32(R.Yaw) & 0x0000FFFC)
                   - (U32(sampledSourceYaw) & 0x0000FFFC))
rotationArgument = { Pitch: 0, Yaw: quantizedYaw, Roll: 0 }
```

The AND immediate is **0000FFFC**, NOT FFFFFFFC: upper16 bits are cleared.
Quantize BOTH operands before subtraction. This is not a quantization of
rawYawDelta. For example requestedYaw4/currentYaw1 gives quantizedYaw4,
whereas `(rawYawDelta3 & FFFC)` would incorrectly give0. The quantized
operands are in0..65532, so their signed difference is bounded; full raw
yaw arithmetic and subsequent child/View additions still wrap32.

Do not replace requested/current raw equality with table-phase equality.
Different high words or low2 bits still enter rotation math even if their
eventual lookup phase matches. A zero quantized yaw does not skip that math.

## Offset, FCoords and exact Float boundaries

When rotation math is active, native x87 uses nearest PC64 arithmetic under
the explicit verified portable numeric profile. Denote Float storage by F
and retained PC64 arithmetic by P.

### 1. Float offset

`10399939..98B` reads and subtracts in this EXACT live order:

```
retainedZ = P(read S.Location.Z - read C.Location.Z)
retainedY = P(read S.Location.Y - read C.Location.Y)
retainedX = P(read S.Location.X - read C.Location.X)
offset.X = F(retainedX)
offset.Y = F(retainedY)
offset.Z = F(retainedZ)
```

The six pose reads are interleaved source/child, not two Vector snapshots.
All three differences stay retained until the X/Y/Z stores. In the exact
instruction schedule, Engine's imported GMath pointer is loaded at
`10399978` before those stores; the12-word UnitCoords copy occurs after them
at `1039998E`. These pose reads are entirely inactive on the equality branch.

### 2. Original rotation construction

Copy all48 bytes of actual GMath.UnitCoords (GMath+18), then call Core
FCoords::operator*=(FRotator) with the yaw-only local argument above. Copy
the returned48-byte coordinate value to the separate transform temporary.

Engine IAT10599B2C is imported GMath. Owned Core exports GMath at RVA B3218;
the original table address101B32A4 is therefore GMath+8C. Core's rotator
operator is IAT10599B48, thunk10101596, body1011AC80..1011B08C.

The operator ALWAYS executes complete yaw, pitch and roll compositions,
including the zero pitch/roll stages; it is not one direct two-dimensional
rotation. Lookup phase is `(Wrap32(angle+offset)>>2)&3FFF`, with cosine
offset4000. The original16384 Float table and nonzero half-turn residual
remain significant. No libm sin/cos, cardinal-zero repair, Euler shortcut,
normalization, inverse or graphics matrix is evidenced.

### 3. Inline VECTOR transform

`103999D0..10399A24` does not subtract coordinate Origin. Symbolically
tracking its x87 stack gives, independently for each row axis K:

```
transformed.K = F(P(P(offset.X * coords.KAxis.X
                       + offset.Y * coords.KAxis.Y)
                       + offset.Z * coords.KAxis.Z))
```

The multiplication/addition intermediates are PC64; products and the XY
sum do NOT Float-spill. All row sums are retained before final transformed
X, Y, Z Float stores at10399A1F/21/24. It is row-axis `(X+Y)+Z`, not the
separate FVector dot helper's `(Z+Y)+X`, transposed axes, or PointBy.

### 4. Rotational delta and final child translation

`10399A27..A54` subtracts stored transformed values from stored offset:
evaluate Z, Y, X retained differences, then Float-store X, Y, Z:

```
rotationalDelta = F3(offset - transformed)
```

Fresh child Hit and child requested Rotation are prepared next. Finally
`10399AD1..AF2` computes the argument translation in this component order:

```
childDelta.Z = F(P(A.Z + rotationalDelta.Z))
childDelta.Y = F(P(A.Y + rotationalDelta.Y))
childDelta.X = F(P(A.X + rotationalDelta.X))
```

These three stores occur separately as constructor arguments, Z/Y/X, and
the constructed vector is later copied XYZ into the call frame. All execute
even on the no-rotation branch, where rotationalDelta is raw positive zero.
Copying A unchanged can lose original signed-zero behavior; active overflow
or nonfinite refusal ordering must not be swapped by an XYZ initializer.

There is no radius/height, PrePivot, MainScale/PostScale, DrawScale, child
velocity or parent source-location mutation in this carry arithmetic.

## Fresh child packet, live child Rotation and recursive call

`10399A57..AB9` initializes ALL44 bytes of a separate local FCheckResult:
Next0, Actor0, Location+0 XYZ, Normal+0 XYZ, Primitive0, Time Float1,
Item signed-1. Each qualifying child receives a newly initialized packet,
even though the native stack address is reused. It is not the caller's
parent Hit packet, and no prior child's result words are retained.

AFTER offset/coordinate math and that initialization, read CURRENT C.Rotation
in this order: Roll (`10399AA4`), Yaw (`10399AAA`), Pitch (`10399AB1`).
Build child requested Rotation:

```
Pitch = sampled current child Pitch
Yaw   = Wrap32(sampled current child Yaw + SAVED rawYawDelta)
Roll  = sampled current child Roll
```

Pass the finalized childDelta, child Rotation and local Hit to the original
retained **L.MoveActor** virtual+88 at10399B51. Native options in argument
order are **0,0,1,0** (test0, ignorePawns0, ignoreBases1, noFail0).
There is no child.XLevel lookup or use of S.Level as this native receiver.
The child integer result and entire returned child Hit are ignored by this
outer carry segment. Ordinary false still proceeds to Pawn.ViewRotation.

The parent source pose/hash removal/publication has not yet occurred.
Recursive work must therefore use real complete MoveActor in the SAME root,
not a transform setter or an assumed collision-free local move.

## Post-call Pawn ViewRotation

At10399B57 retain/reload the SAME C from the saved child pointer, not the
possibly changed world slot. Walk its CURRENT actual UClass ancestry for
Engine.Pawn (native class1058C660). Non-Pawn skips view fields. Pawn status
is checked AFTER the recursive call, without a deletion/Base validity gate.

For a Pawn, `10399B80..BA6` reads CURRENT ViewRotation:

1. Yaw, native+3E0.
2. Pitch, native+3DC.
3. Roll, native+3E4.

Then write Pitch, Yaw, Roll at10399B9A/BA0/BA6, with unchanged sampled
Pitch/Roll and `Wrap32(currentViewYaw + SAVED rawYawDelta)`. Do not recompute
rawYawDelta from the possibly callback-mutated parent Yaw. All three writes
execute, including unchanged Pitch/Roll. Native integer additions are raw
wrapping32, not clamped angles, floating degrees or signed C++ overflow.

## Existing correct value helpers and integration boundary

The exact reusable arithmetic is already present in `quest_brush_coords.h`
and `quest_model_owner_coords.h`:

- `BrushCoordsDetail::Rotate(IdentityOriginalModelCoords(),
  {0,quantizedYaw,0}, explicitTable)` is the direct existing ordered
  FCoords*=FRotator value service for known UnitCoords. A public wrapper may
  expose that service; a new closed-form yaw implementation is unnecessary.
- `TransformOriginalVector(offset, coords)` has exactly the inline row-axis
  retained `(X+Y)+Z` and final Float storage schedule. Do NOT use PointBy.
- PC64 Load/Sub/Add64/Store32 value primitives supply the surrounding offset,
  rotational-difference and final-add stores, preserving their distinct
  source/read/store orders in the coordinator.

The public `BuildOriginalActorCoords({+0,+0,+0}, {0,quantizedYaw,0}, table)`
also produces the same VALUE for the established identity UnitCoords
profile. It performs an additional constant-zero Origin translation stage;
it is not an exact instruction-count/read-schedule replacement for copying
an arbitrary live UnitCoords. No nonzero parent Location belongs there.

Use explicit `OriginalSineProfile::Table()` only with its pinned generated
instruction-profile provenance and limits from ORIGINAL-SINE-PROFILE.md.
It does not claim capture of a live original process GMath. Actual ULevel,
actor/class/qualified Base/Rotation/ViewRotation authority, native generation,
live slots/counts, caller-root ownership and budget remain separate adapters.

Portable nonfinite/overflow or unsafe metadata/recursion exhaustion must
refuse and poison that shared root, not truncate a scan or return ordinary
success. The source contains no finite validator or automatic rollback;
normal child false results keep all ordinary native side effects. Before a
publishing integration, add independent arithmetic/spill and protocol tests
for callback-mutated counts/slots/Base/poses, quantize-before-subtract,
full-range wrapping, inactive Roll/location/table branches, zero signed
words, ignored child results, live post-call Pawn View writes, recursive
budget exhaustion and complete rollback on unsupported work.
