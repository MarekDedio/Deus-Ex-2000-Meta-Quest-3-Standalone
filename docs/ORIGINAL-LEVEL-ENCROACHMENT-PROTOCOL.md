# Detached original Level encroachment protocol

Implementation checkpoint, 2026-10-19. Scope is the complete shared native
dependency needed by the first `00_Training` map. No other real map was loaded,
integrated or tested. This is not a claim that native MoveActor/SetRotation,
Training physics, gravity, destructible crates or the whole tutorial work.

`native/quest_level_encroachment.h` implements the detached control flow of
`ULevel::CheckEncroachment(AActor*,FVector,FRotator,int)`. It deliberately has
no renderer, serialized-state shortcut, fake Move implementation, actor-list
fallback or publishing runtime entry. Its real dependencies remain explicit
providers. A location setter, fabricated empty requery or suppressed event is
not a valid implementation of those dependencies.

## Source and completeness boundary

The owned `Engine.dll` SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
The owned `Engine.u` SHA256 is
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
No commercial DLL or EXE was loaded or executed for this implementation.

The implementation author read the complete existing
`docs/ORIGINAL-LEVEL-ENCROACHMENT.md` audit and the entire ignored bounded
static evidence file
`artifacts/training-level-encroachment-20261019/level-check-encroachment-body.asm`.
The old truncated `original-checkencroachment-early-body.asm` is not the
authority. Export ordinal830, thunk `10302536`, reaches the full body
`1039A350..1039A90D`; catch fragments are `1039A910..1039A93F`.

The existing audit records export identity, native field/class identities,
event schemas, memory-stack imports and the complete lower dependencies.
This document describes the new detached implementation and its explicit
portable admission/cleanup policy; it does not replace that source audit.

## Public seam and dependency authority

`CheckOriginalLevelEncroachment(level, source, proposedLocation,
proposedRotation, touchFlag, queries, maximumLinkVisits=65536)` returns a
native veto Boolean. Proposed Location and Rotation are copies, like the
native by-value arguments. The detached entry rejects null Level/source.
Object identities and linked-hit handles are opaque integers, not persistent
indices, native addresses to dereference or permission to consult a cache.

`OriginalLevelEncroachmentQueries` exposes these live dependencies:

- Native admission flags, exact moving-brush predicate and actual Level.Hash.
- One memory mark and its mandatory cleanup; the lower source-pose/hash query
  returns a live linked handle. `hitActor` and `hitNext` are separate reads.
- Actual Level.Actors[0] raw reads plus its authentic LevelInfo class check,
  and the complete lazy source-local `IsBlockedBy` dependency.
- Raw source Location channels, narrow raw Location-only temporary writes,
  and fixed raw restoration. No Rotation/ColLocation/hash write is implied.
- Complete collision-aware `moveSmooth`, authentic selected
  `EncroachingOn`/`EncroachedBy` event dispatch, exact original overlap and
  BeginTouch/EndTouch helpers, and live four-slot touching reads.
- One caller-owned work hook, shared by all nested moves, events and queries.

The `encroachingOn` provider must dispatch the selected original event with
its native Bool result DWORD seeded to zero, then return that DWORD as an
integer. Any nonzero value vetoes; it is not normalized using positivity.
`encroachedBy` receives candidate as receiver and source as Other. Base Actor
bodies being empty does not authorize bypassing state/derived overrides.

The eventual real adapter must authenticate current Level, actual actor
identity and generation, exact native field/class authority and real hash
ownership, preserve root transaction boundaries, and enforce existing work
and memory admission. It must keep the original list and every requery list
live in the SAME mark/root. The lower hash query may pose the source only
during that query; it cannot leave the whole higher operation at the proposed
pose. No real publishing adapter or fake movement provider is added here.

## Exact normal protocol

### Admission and memory mark

At `1039A39A..1039A3D9`, any source native flags in mask `0D` admit the
operation: CollideActors=01, BlockActors=04, BlockPlayers=08. Otherwise only
the exact moving-brush predicate admits it: nonnull raw Brush, authentic
Engine.Brush ancestry and !bStatic. CollideWorld=02 alone is insufficient.
Static brushes with an admission bit are not globally excluded. This early
false path does not require a mark, hash, pose data, callback or touching
provider.

After admission, acquire one mark, then read actual Level.Hash
(`1039A3DF..1039A3F9`). Initial None hash means a null original list, not a
failure. A nonnull hash invokes the source encroachment query with copies of
the proposed Location/Rotation and literal final DWORD0. That DWORD is never
the higher-level touch flag. Keep the ORIGINAL head for both later passes.

### First original-list pass

For every nonnull original record, skip a first raw Actor equal to source
before any LevelInfo validation (`1039A448`). Nonself records use distinct
live LevelInfo reads, not one cached pointer:

1. Read Level.Actors[0] and require nonnull.
2. Reload Level.Actors[0] and require actual LevelInfo ancestry.
3. Reload original hit.Actor.
4. Reload Level.Actors[0] again and compare identities; skip if equal.

These correspond to `1039A454`, `1039A474`, `1039A4B6` and `1039A4B9`.
The raw comparisons do not restart if a provider changes another field.

Evaluate live source.IsBlockedBy(candidate). The complete separately
implemented matrix, including both-brush precedence and receiver-local
PlayerPawn semantics, is a dependency, not a broad collide-bit substitute.
Nonblockers go directly to the live original Next. Blocking candidates test
live source moving-brush status. Only if true do they reread original
hit.Actor and test that actor's current moving-brush status
(`1039A4E5..1039A534`). A moving-brush candidate takes the direct event path.

For source-moving/candidate-nonmoving pairs:

1. Evaluate `proposed - current source.Location` in Z,Y,X order with PC64
   retained subtraction, then spill X,Y,Z to Float (`1039A53A..1039A571`).
2. Reread original hit.Actor AFTER all source pose loads and Float spills;
   call its complete `moveSmooth(delta)` and ignore the integer result.
3. Reload actual Level.Hash and requery the SAME source/proposed pose with
   final DWORD0 (`1039A57C..1039A5BD`). None here is an unsafe native
   dereference, so the portable implementation refuses rather than inventing
   an empty list/clear result.
4. Scan the distinct requery chain. At each record, load requery hit.Actor,
   then reread ORIGINAL hit.Actor for that comparison (`1039A5CD..1039A5D3`).
   If different, follow requery's live Next. Do not cache the wanted actor,
   replace the original chain or process unrelated requery candidates.
5. If no match exists, continue the original list with no reverse attempt and
   no EncroachingOn for that record. It remains in the later original pass.
6. If a match exists, perform the reverse attempt described below, then the
   veto event path.

Direct blockers and still-present push candidates reread ORIGINAL hit.Actor
again for `source.EncroachingOn(other)` (`1039A77F..1039A7AB`). Any nonzero
result immediately pops the mark and returns true. Otherwise follow ORIGINAL
hit.Next, read only after all processing/callbacks (`1039A641`).

An ordinary true return does NOT roll back earlier pushes, candidate motion,
event side effects, fields or tags. The original reverse attempt is not a
candidate-position undo. Caller-root rollback belongs to exceptional/refused
work, not normal veto semantics.

### Reverse movement and raw Location scope

At `1039A6F6..1039A770`, multiply the saved Float forward delta by the literal
Float -1 at `104298F0`, retaining products in Z,Y,X order. Read the CURRENT
source Location's raw X,Y,Z words. The saved source pose may have changed
during forward movement/requery, so it is not the entry pose and must not be
reconstructed from authored/default/serialized data.

Arm restoration before the first temporary write. The source schedule is:

1. Write source.Location.X = proposed.X; spill reverse X to Float.
2. Write source.Location.Y = proposed.Y; spill reverse Y to Float.
3. Write source.Location.Z = proposed.Z; spill reverse Z to Float.
4. Reread ORIGINAL hit.Actor AFTER the override; invoke its
   `moveSmooth(reverseDelta)`, ignoring its integer result.
5. Restore the saved raw source Location before any event-argument reread.

The kernel uses retained integer-dyadic PC64 operations and explicit Float
stores, not host `double`, vector negation with an invented store schedule or
a snapshot of the candidate. Signed-zero multiplication is preserved. Saved
raw NaNs, infinities and signed zeros are not arithmetic operands and are not
filtered or canonicalized. The temporary override is ONLY Location; it does
not change source Rotation, ColLocation, hash placement or persistent script
state. Candidate side effects from either move remain live.

### Optional stale touches and mandatory second original-list pass

After the first pass finishes without a veto, any nonzero signed touchFlag
enables the four-slot cleanup (`1039A7E7..1039A81D`). For indices0..3, read the
source slot live. If nonnull, call source.IsOverlapping(peer). If false,
REREAD that same slot and invoke source.EndTouch(current,0). Do not skip a
newly null replacement, restart the scan or suppress reciprocal UnTouch.
Advance the index even when an EndTouch callback changes later slots.

The second pass always uses the ORIGINAL head, regardless of touchFlag or
later requery results (`1039A81F`). It repeats self and actual slot0 filters.
Its final raw-read order differs from the first pass:

1. Read slot0 and require nonnull.
2. Reload slot0 and require LevelInfo ancestry.
3. Reload slot0 for comparison.
4. Reload original hit.Actor for comparison.

These correspond to `1039A836`, `1039A856`, `1039A882` and `1039A887`.
Re-evaluate live source.IsBlockedBy(candidate); first-pass results are not
cached. If blocking, reread original hit.Actor and invoke
candidate.EncroachedBy(source), even if touchFlag=0. If nonblocking and
touchFlag!=0, reread original hit.Actor and invoke source.BeginTouch(other).
Otherwise do nothing. Follow ORIGINAL hit.Next only AFTER the event/touch
callback (`1039A8C2`/`1039A8DB`). There is no new overlap test, no delete/base
filter and no exclusion of an actor pushed clear during the first pass.
Finally pop the mark and return false.

## Bounded work and stronger exceptional safety

Every active ordinary provider invocation first charges the optional work
hook; the eventual runtime adapter must supply the existing enclosing ledger.
Missing active providers throw instead of fabricating native outcomes.
Inactive dependencies are not required or touched. The detached kernel's
separate `maximumLinkVisits` is cumulative across BOTH original-list passes
and ALL requery scans. Each nonnull visited record charges it before the first
Actor read. Exhaustion throws; it does not truncate, skip a cycle, return
native false or allocate a fresh budget for another requery. Empty lists and
unadmitted sources can succeed with a zero link allowance.

Portable arithmetic admits finite active proposed/source delta operands and
finite Float outputs. Each active component is validated at use, not by an
eager full-pose scan. The proposed component is checked before its matching
source-channel provider; source channels are read Z,Y,X. Subtractions remain
retained until all three loads finish, then Float overflow is checked in
X,Y,Z spill order. Invalid/overflowing active arithmetic is an exception, not
a claim that retail masked x87 NaN/overflow returned clear. Inactive pose and
saved raw Location words are not subjected to this arithmetic policy.

`popMark(token)` and `restoreLocation(actor, rawWords)` have fixed mandatory
NOEXCEPT, allocation-free, unmetered cleanup contracts. They are not ordinary
fault-injected callbacks. Their availability is checked before acquiring the
mark and before the first temporary Location write, respectively. Opaque mark
token zero is valid. Acquisition must either return a valid cleanup token or
fail without leaving an acquired resource. Cleanup is never charged to an
exhausted work budget, silently swallowed as success or retried after a
possibly completed cleanup. Throwing from mandatory cleanup violates the
provider contract and cannot produce a normal result.

The header's RAII scopes pop the mark on both normal results and all ordinary
exceptions, and restore temporary raw Location before unwinding to mark
cleanup. Root transaction rollback remains necessary for other exceptional
side effects. This is deliberately stronger than the recorded retail
exception metadata: its unwind actions are zero and its catch tail
logs/rethrows, with no source pose/mark cleanup action to attribute to it.

## Executed independent tests and integration boundary

The standalone optimized host suite passes35378 checks,1707 independent
protocol cases,1286 static/numeric/layout cohorts,14 live callback mutations,
356 active provider/work failure prefixes,407 explicit refusals and three
caller-owned MOCK roots. The separate instruction-label interpreter does not
call the production kernel; an explicit direct-blocker golden transcript
additionally checks the provider order. Null-input controls call production
itself rather than refusing in a shared harness before dispatch.

The fixtures exercise source admission, both pass-specific slot0 read orders,
original-vs-requery list identity, live Actor/Next/flags/slots mutations,
escaped vs still-present pushes, raw signed-zero/nonfinite saved Location,
signed touch flags, failed first/partial writes, missing cleanup admission,
lost recheck hash and cumulative cyclic-list refusal. Ordinary veto keeps
earlier effects; caller-owned mock transactions roll back exceptional coupled
effects. Detached callback fixtures are not real game script bodies.
Two additional reentrant production-kernel controls check nested mark success
and inner-query refusal: exact LIFO cleanup restores the outer list's mark,
then the original caller mark, while preserving the shared work ledger. The
fixture actually pops its mark stack; a retained history record is not used
as the outer mark's authority.

Fixture trace capacity is reserved by each ordinary provider before it can
fail, leaving room for two fixed cleanup records even if the next work hook
refuses before a provider runs. This avoids allocating in those cleanup logs;
it is not arbitrary allocator-fault proof of a real runtime adapter. The
mandatory noexcept/allocation-free provider contract still has to be met by
that eventual adapter. Strict ARM64 runtime/test syntax and Android native
compilation/link pass. Evidence:
`artifacts/training-level-protocol-unit-final-20261019.log` and
`artifacts/training-level-protocol-arm64-test-final-20261019.log`.

No real first-Training integration, complete collision-aware movement provider,
Quest APK or headset validation is added by this detached protocol. The host
runtime includes it for compilation, but native266/299 remain refused and no
world, actor transform, collision registry or startup flag is published by it.
The coherent host rebuild/CTest passes145 executed/169 registered with24
explicit owned-data skips, no failures in45.78s. The separate rebuilt actual
first-map regression passes951463/68 and confirms the same native266 startup
boundary; it does not exercise this kernel through a real movement provider.
See `artifacts/training-level-protocol-ctest-final-20261019.log` and
`artifacts/training-level-protocol-owned-final-20261019.log`.
