# Original complete ULevel::MoveActor contract

Static audit of the owned GOTY PE32 files, 2026-10-19. This records the complete
outer operation required by first `00_Training` startup and later movement; it
does **not** implement native266/299, complete world startup, falling, crate
physics or headset acceptance. No original DLL/EXE was loaded or executed, and
no other map was loaded or integrated.

Source SHA256 values:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

## Evidence and reproduction boundary

The entire native body, every ordinary return and its catch tail are in ignored
`artifacts/training-move-actor-20261019/move-actor-full.asm`:
`103990E0..10399F9F`. Export1298, decorated
`?MoveActor@ULevel@@UAEHPAVAActor@@VFVector@@VFRotator@@AAUFCheckResult@@HHHH@Z`,
has thunk `10303C60 -> 103990E0`. Do not interpret the disassembler's nearest
unrelated `AInventory` symbol as the function identity.

The dump was produced with NDK27's `llvm-objdump.exe`, `--disassemble
--x86-asm-syntax=intel --no-show-raw-insn --start-address=0x103990e0
--stop-address=0x10399fa0` against the hashed Engine.dll. This is static byte
inspection, not an invocation of an exported native function.

Additional bounded evidence in that directory:

- `move-actor-thunk.asm`: exported identity to body.
- `appsqrt-body.asm`: complete Core appSqrt body, including its return.
- `numeric-threshold.log`, `numeric-zero.log`: original constant words.
- `original-bump-schemas.log`: authored BumpWall/Bump packets, inspected by the
  port's own package/bytecode reader.
- `move-actor-eh-handler.asm`, `move-actor-eh-info.log`: generated exception
  handler and unwind metadata.

Import/export identities can be reproduced with `llvm-readobj --coff-imports
--coff-exports`. The existing ignored
`artifacts/training-level-encroachment-20261019/engine-export-import-identities.log`
contains the complete identity listing used here. Relevant Engine IAT entries
are `10599A1C` appFailAssert, `10599A74` UObject::IsA, `10599A88`
FindFunctionChecked, `10599B28` FVector(Float,Float,Float), `10599B2C` GMath,
`10599B48` FCoords::operator*=(FRotator), `10599BD8` GMem, `10599C94`
GIsEditor, `10599CA4` appSqrt, `10599CB0` FMemMark::Pop, `10599DAC`
GTimestamp, and `1059A034` FRotator(Int,Int,Int).

The previous ORIGINAL-ACTOR-MOVE.md establishes the native wrappers and exact
declared fields. This fuller audit corrects two important readings: BumpWall's
packet is **Location, Normal**, and the ordinary encroachment-veto return
does **not** pop MoveActor's GMem mark.

## Arguments, authorities and complete return set

Let `L` be the native ULevel receiver, `S` the source actor, `D` the by-value
requested Delta, `R` the by-value requested Rotation, and `H` the caller's live
44-byte FCheckResult. Stack arguments are S`+08`, D`+0C..14`, R`+18..20`,
H-reference`+24`, followed by four integer words `+28/+2C/+30/+34`, here named
`bTest/bIgnorePawns/bIgnoreBases/bNoFail` by observed behavior. Nonzero means
true; the body does not clamp these integers to 0/1. Scratch subsequently
reuses some argument slots; their old meanings must not be reread later.

`L` is not S's reflected Actor.Level. Native266/299 obtain actual S.XLevel and
dispatch its virtual+88; recursive child moves use the original L receiver.
Actor.Level is separately read by the blocking predicate. Likewise, an actual
slot0 LevelInfo check is not a search for a suitable actor or a use of a
reflected Level overlay. Class predicates follow actual UClass ancestry.

| Return path | Addresses | Result and caller Hit effects |
| --- | --- | --- |
| Ineligible outside editor | `1039912D..157` | false; H untouched; no mark |
| Small motion, same R | `103991C8..1ED -> 10399228..23D` | true; H untouched; no mark |
| Small motion, no dependants/dynamic brush | `103991EF..225 -> 228..23D` | assign R only, true; H untouched; no mark |
| Encroachment veto | `10399C5F..C97` | false; initialized/selected/backed-off H remains; children/callback effects remain; **no mark pop** |
| Full-operation tail | `10399F03..F5F` | SetActorZone, pop mark, then H.Time > Double0; otherwise false |

The final comparison tests x87 AH mask41. Ordered positive H.Time returns
true, ordered zero/negative and unordered return false. Positive partial
movement therefore can return true; the result is not requestedDelta==actualDelta.

## 1. Eligibility and actual small-motion exits

At `10399104..12A`, the source is asserted nonnull. At `1039912D..157`,
`(bStatic || !bMovable) && !GIsEditor` returns false. The bits at S+28 are
authored Bool storage, not UObject flags: Static=1, Movable=0x80000. There is
no entry bDeleteMe, bBegunPlay, world-possessed or rendering gate.

The first test inspects absolute D.X, then Y, then Z, stopping at the first
component not below original **Double**0.0001 at `1042AC40`. Bytes
`2D 43 1C EB E2 36 1A 3F` are bits `3F1A36E2EB1C432D`; a Float threshold
is different. Negative values are conditionally FCHS'd before the threshold
comparison. Native uses x87 C0 tests rather than a finite-input validator;
portable active-nonfinite refusals must be labeled safety policy, not retail
behavior. For finite inputs the comparison is strict.

When all three pass, compare all three raw Int Rotation words. Equal R
returns true immediately. Otherwise read StandingCount byte S+D8. Nonzero
forces the full path. With zero dependants, only a dynamic brush forces the
full path: Brush pointer nonnull, actual Engine.Brush ancestry, and !bStatic,
in that order. Otherwise assign only the three Rotation words and return
true; the tiny nonzero translation is discarded, with no hash/zone/events.

## 2. Full-path packet, mark and numeric preparation

At `10399240..294`, initialize every H word: Next=0, Actor=0, Location=+0,
Normal=+0, Primitive=0, Time=Float1, Item=-1. Counter/timing changes to
L+1130/L+112C use imported GTimestamp/RDTSC and are profiling, not gameplay
authorities. An omission may be documented as profiling only.

At `103992D3..2E8`, capture GMem, its current Top word and current chunk
word+0C into a mark. It covers the movement list and later nested allocations;
it is not an actor/value/hash/RNG transaction or a lifetime-long pose scope.

The second small test repeats the same lazy XYZ threshold. Small D uses
stored size Float+0 and raw Float3 D as direction. Otherwise:

1. Compute retained PC64 `(Dz*Dz + Dy*Dy) + Dx*Dx` (`1039935B..374`).
2. Store **Double** argument, call original appSqrt. Engine IAT10599CA4 names
   `?appSqrt@@YANN@Z`; Core thunk `101032BA -> 10123290..296` is exactly
   FLD the Double stack argument, FSQRT, RET. Its result remains PC64.
3. At `1039937D`, FST a Double scratch **without reloading**; at `10399380`,
   FST Float size **without reloading**; divide Double1 by the retained root
   and Float-store inverse. Direction components use that Float inverse and
   each have their own Float store. A generic SafeNormal is not this schedule.
4. At `103993BC..406`, ExtendedDelta=Float3(D+2*direction). Doubled X/Y are
   Float-spilled before addition; doubled Z stays retained through its sum.

ActualDelta starts as raw requested D. The implemented full numeric helper
(ORIGINAL-ACTOR-MOVE-GEOMETRY.md) is a value service, not authorization to skip the rest
of this operation. Source-backed numeric dependencies for this section are
only the original appSqrt and plain FVector constructor; no libm normalize,
epsilon expansion or target-double reassociation is evidenced.

## 3. Sweep and first blocking selection

At `1039940E..46E`, skip MultiLineCheck when both collide flags are false,
when S is a dynamic brush, or when every D component compares equal to
Float0. Otherwise prepare and call `L.virtual+C8` at `103995C8`:

```
MultiLineCheck(GMem, Float3(currentS.Location+ExtendedDelta),
               currentS.Location, (radius,radius,height),
               actorCheck, worldAnchor, byte0)
```

worldAnchor is actual L.Actors[0], asserted nonnull and actual LevelInfo,
only when current bCollideWorld and !dynamicBrush; otherwise None.
actorCheck is 1 only when current bCollideActors and !dynamicBrush, else0.
Those dynamic-brush predicates are separately evaluated at their source
sites, not one cached earlier result. Height/radius/Location reads are live.
Read Height, radius for Y, then radius again for X; X is copied raw while Y/Z
pass through Float stores. Endpoint arithmetic first samples Location Z/Y/X
and retains each sum. Read start's raw Location X before endpoint's X Float
store, then start Y before endpoint Y, then start Z before endpoint Z. Do not
cache one pose/radius snapshot or move all endpoint stores ahead of start
reads. There is no source-actor argument and no prequery self filter.

Store the returned head even if no blocking scan will occur. Scan only when
current S flag mask0E is nonzero. Walk the linked result order and its live
Next fields; there is no outer sort/cap/deduplication here. Each candidate:

1. If bIgnorePawns, skip a nonstatic actual Pawn OR Decoration. Static
   instances bypass this skip. The source checks Pawn before Decoration.
2. If bIgnoreBases and S.IsBasedOn(candidate), skip.
3. Always skip candidate.IsBasedOn(S), which includes identity/self.
4. Set potentialTouch=1 (`10399683`) before applying the blocking matrix.
5. Apply the original IsBlockedBy matrix. On the first blocking result,
   copy all44 bytes, including Next/Primitive/Item, into H and stop.

The inlined matrix `10399678..795` matches ORIGINAL-ACTOR-BLOCKING-PREDICATES:
candidate==S.Level selects S.bCollideWorld first; candidate IsBrush selects
its block bit using S.LocalPlayerPawn and requires S.bCollideWorld; source
IsBrush selects its bit using candidate.LocalPlayerPawn and requires candidate
bCollideWorld. These are the broader Brush-presence/class predicates, not
dynamicBrush. Only the ordinary reciprocal case selects each block bit using
the opposite actor's LocalPlayerPawn **or Projectile** ancestry. Neither
branch adds bCollideActors, Owner, same-level or deletion exclusions.

## 4. Hit backoff and exact unmodified fields

At `103997A7..7C3`, compare H.Time to Double1 using C0. Ordered >=1 skips
correction; unordered takes it unless bNoFail is nonzero. bNoFail skips only
backoff, not query, hit selection, parent movement or final H.Time return.

At `103997C9..89D`:

```
distance = PC64((Float2 + storedFloatSize) * H.Time)
storedDistance = Float(distance)       // FST, retained value still compared
if distance <= Float2:                // mask41 also admits unordered
    actualDelta = Float3(+0)
    H.Time = Float+0
else:
    back = Float3(direction * Float2)
    scaled = Float3(ExtendedDelta * H.Time)
    actualDelta = Float3(scaled - back)
    H.Time = Float(PC64((storedDistance-Float2)/storedFloatSize))
```

All three back products and all three scaled products spill Float before
their componentwise subtractions. Final distance correction reloads the
stored Float distance, not its retained comparison value. H.Location/Normal/
Actor/Primitive/Item/Next are not recomputed to match actualDelta. A portable
finite-active domain may refuse the original's nonfinite arithmetic/stores;
it must leave inactive words untouched and poison the outer root on failure.

## 5. Live based-actor carrying before parent mutation

At `103998A0..9BB5`, StandingCount=0 skips this scan. If StandingCount!=0
and bTest!=0, jump directly to parent mutation (also bypassing encroachment).
Otherwise scan L.Actors from index0 upward, loading the current array/count
at each iteration. For each current nonnull slot whose current Base==S:

- Compute wrapping rawYawDelta=requestedYaw-currentS.Yaw anew for this child.
- Initialize rotationalDelta to +0. If ANY requested Rotation word differs
  from current S.Rotation, form yaw-only FRotator with
  `(requestedYaw & 0xFFFC) - (currentYaw & 0xFFFC)`, wrapping at32 bits.
  Float-store offset=`currentS.Location-currentChild.Location`; copy
  GMath.UnitCoords; call Core FCoords::operator*=(FRotator) at IAT10599B48;
  transform the offset by its axes; Float-store
  rotationalDelta=`offset-transformedOffset`.
- Fresh child H has the same complete zero/Time1/Item-1 initialization.
  Child R uses current Pitch/Roll and wrapped currentYaw+rawYawDelta. Call
  **L.MoveActor**, not a separately looked-up child.XLevel, with Float3
  `(actualDelta+rotationalDelta)`, fresh H and options **0,0,1,0**.
- Ignore that result. Afterwards, if the retained child has actual Pawn
  ancestry, add the saved rawYawDelta to current Pawn.ViewRotation.Yaw;
  copy its current Pitch/Roll through unchanged. No deletion/validity skip
  is inserted between the call and these class/field operations.

All this precedes the parent's own removal or pose writes. Do not snapshot
child slots, source yaw/position across callbacks or the array's initial
count. Child moves can mutate source/children, append actors, cause events
and consume shared work. Cycles/unbounded appends require an explicit bounded
portable refusal, not altered ordinary success behavior.

FCoords*=FRotator, UnitCoords, original sine-table policy and retained vector
math have their separate ORIGINAL-BRUSH-COORDINATES/ORIGINAL-SINE-PROFILE
audits. The child inline transform is `103999D0..9A24`; its output components
are `(offset.x*Axis.x + offset.y*Axis.y) + offset.z*Axis.z`, PC64 until each
Float XYZ store. The subsequent complete detached carrying protocol and its
independent branch/rounding/live-list/recursive-callback tests are documented
in TRAINING-CHILD-CARRY.md. Its recursive Move providers are explicitly MOCK;
actual runtime wiring and same-root movement/hash integration remain. Renderer
matrices do not establish this contract. Pitch/roll changes still enter this
yaw-only construction.

## 6. Higher encroachment and its veto asymmetry

At `10399BB5..C61`, skip this call if bTest, bNoFail or actual Pawn ancestry.
Otherwise Float-add actualDelta to the **then-current** source Location,
after child calls, and invoke `L.virtual+B8 CheckEncroachment(S, proposed,
requestedR, int0)`. The final zero disables that operation's conditional
touch pass, not its query/push/blocking events; see ORIGINAL-LEVEL-ENCROACHMENT.

A nonzero result updates profiling and returns false at `10399C97`. The
parent's own Remove/Location/Rotation/Add and zone tail have not run. Earlier
child movement and callback side effects are ordinary retained effects, not
an automatic rollback of a native false result. H's already established
packet/backoff also remains.

**There is no MoveActor mark Pop on this return.** Its only observed Pop
call is in the full tail `10399F15..F18`; the veto goes directly to the common
false epilogue at `10399C85`. A portable scoped resource cleanup may be a
deliberate stronger safety policy, but must not be attributed to retail's
ordinary veto path or confused with undoing ordinary gameplay effects.

## 7. Parent hash/transform publication

At `10399C9A..D17`, read current bCollideActors and L.Hash; if both active,
call current Hash.RemoveActor(S), virtual+0C. Then Float-add actualDelta
to current Location and store X, Y, Z in that order. Assign requested
Rotation's three Int words. Reread current bCollideActors and L.Hash and,
if active, call Hash.AddActor(S), virtual+08. Do not replace these operations
with a render collision registry or rebuilding unrelated actors.

**bTest performs these pose/hash mutations.** It is not a read-only preview:
it suppresses child carrying, higher encroachment and the following bump/
touch work, then still reaches SetActorZone(S,bTest,0). A public rollback-only
diagnostic is a separate portable transaction policy.

## 8. Bump and live touch relationship maintenance

Unless bTest, at `10399D22..E65`, nonnull H.Actor first causes fresh validation
of L.Actors[0]. If H.Actor equals that actual slot0, dispatch S.BumpWall with
the packet **(H.Location,H.Normal)**. Native `10399D94..E0C` copies H+08 as
the first vector and H+14 as the second. The genuine authored event confirms
this order: Engine.Actor.BumpWall children HitLocation, HitNormal, both
StructProperty/Core.Object.Vector dim1 flags80; event flags800/native0.
It is not HitWall, whose signature/meaning belongs to moveSmooth elsewhere.

For non-world H.Actor, skip Bump only if S.IsBasedOn(H.Actor). Otherwise
dispatch retained H.Actor.Bump(S) first. Then reread caller H.Actor, and
dispatch S.Bump(that current actor). Both use FindFunctionChecked before
ProcessEvent. Engine.Actor.Bump has one Actor Other, dim1 flags80,
flags800/native0. There is no outer begun/deletion gate around these calls.

At `10399E65..ED0`, enter the original query list when potentialTouch!=0 OR
either current S.bBlockActors or S.bBlockPlayers is false. For each live link,
the x87 C0 test admits Time **strictly** below current H.Time **or unordered**.
Ordered equality/greater stops the entire scan; a callback-created NaN in
current H.Time continues it. For each admitted link:

1. Skip candidate.IsBasedOn(S).
2. When bIgnoreBases, skip S.IsBasedOn(candidate).
3. Apply live S.IsBlockedBy(candidate); if false, call S.BeginTouch(candidate).
4. Read link.Next after those calls and continue. The first link failing that
   comparison terminates the entire scan, not just that link.

Do not reapply bIgnorePawns here, cache the pre-callback blocking flags, sort
the list again or replace native Touching storage with event-history bits.
At `10399ED2..F03`, scan the current four Touching slots in index order. A
nonnull peer with !S.IsOverlapping(peer) causes S.EndTouch(the slot reread
after the predicate,0). Later slots are read after earlier callbacks. Full
BeginTouch/EndTouch/IsOverlapping contracts and real field adapters are
separate ORIGINAL-ACTOR-TOUCH and ORIGINAL-ACTOR-OVERLAP work.

## 9. Zone, mark, exceptions and transaction distinction

At `10399F03..F0F`, always invoke **L** virtual+AC
`SetActorZone(S,bTest,0)`, including bTest!=0. Then call imported
FMemMark::Pop at `10399F18`, update profiling and return H.Time>Double0
(`10429838` contains all-zero Double words). The final Hit.Time read occurs
after zone callbacks, not immediately after backoff.

Movement-time `(bTest,0)` is not the existing startup `(1,1)` zone adapter.
The latter's audit records old-zone ActorLeaving, S.ZoneChange, new-zone
ActorEntered, Pawn FootZoneChange/HeadZoneChange, native deletion handling
and PRI updates. The subsequent complete ordinary SetActorZone protocol and
typed rollback-only runtime providers are in TRAINING-ACTOR-ZONES.md; actual
movement-root wiring/live callback execution remains separate. Birth
force-refresh must not be silently substituted here.

EH stub `10420500` selects metadata `10447478`. Its two unwind-map entries
at `10447498` and `104474A0` have zero action pointers. Catch tail
`10399F64..F98` invokes the original unwind logging/rethrow machinery; it
does not supply an observed GMem-pop, pose restoration or actor rollback
action. Portable exception-safe arena/temporary-pose restoration and full
outer VM/hash rollback are stronger safety guarantees, not retail EH parity.

## Complete remaining first-Training integration seam

The implemented original services are reusable components, not full Move:
actual Level.Hash lifecycle/CRT initialization and line/encroachment queries;
raw Cylinder/Model/Mover providers and source-pose scopes; exact MultiLine
collector/order; original receiver-local blocking/Base/LocalPlayer helpers;
qualified Touching/Overlap and dispatch; supplied-value PC64 coordinates,
TwoWallAdjust, moveSmooth protocol, the movement numeric packet, ordinary zone
protocol/typed diagnostic providers and detached complete based-child carry.
The detached higher Level encroachment protocol likewise still requires its
real collision-aware moveSmooth provider. Check current code/tests for each
component's stated supported domain rather than treating this list as a
publishing runtime integration claim.

The complete detached coordinator is now implemented in
`native/quest_level_move_actor.h`. Its independent-provider/hardware tests and
remaining real-service boundary are recorded in TRAINING-LEVEL-MOVE-ACTOR.md.
It does not by itself route native266/299 or publish a Training move.

Still required before routing native266/299:

- Bind that coordinator to actual native eligibility/editor authority, exact
  qualified scalar writes and native retained Level/list/hash identities.
- Recursive real MoveActor and moveSmooth in that same root; no direct
  transform stand-in, no zero-delta-only success and no forced begun state.
- Real higher CheckEncroachment binding, including push/recheck/reverse-push,
  EncroachingOn Bool seed0, EncroachedBy and ordinary-veto side effects.
- Movement-time SetActorZone(S,bTest,0), authentic BumpWall/Bump overrides,
  callback effects and reentrant actor lifecycle support. Existing cold-world
  event suppression is not permission to omit relationship writes or selected
  function/schema authentication.
- Shared bounded work and peak memory across all nested services; stable
  generation/identity guards; no budget reset inside recursion. Poison/refuse
  must roll back actors/fields/clocks/hash/tags/RNG/arrays/publication/saves
  together, while an ordinary native false keeps original permitted effects.
- Actual first-Training rollback diagnostics, progression to the next genuine
  startup boundary, then build/device verification. Isolated numeric/protocol
  tests cannot prove a successful startup, playable stairs or crate gravity.

This audit deliberately does not load campaign maps, integrate their actors
or expand the short-term target beyond first Training's dependencies.
