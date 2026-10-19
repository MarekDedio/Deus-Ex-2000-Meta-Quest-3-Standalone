# Original Actor.Move266 and ULevel.MoveActor contract

Static, read-only retail GOTY evidence, not a claim that native266 is usable
or that original startup/falling physics has completed. No original DLL/EXE
was executed. Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`;
Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.

## Immediate first-Training caller and rotation follow-on (2026-10-18)

Read-only inspection of the owned Engine.u bytecode pins the current startup
failure to Engine.Mover.BeginPlay PC166. Its Move argument is
`BasePos + KeyPos[clamped KeyNum] - Location`. Do not assume that argument is
zero from a door's initial rendered position: original AMover.Spawned at
103AF790..103AF7CA copies Location to BasePos and Rotation to BaseRot, and the
correct loaded/born chronology still needs its explicit native binding.

After Move, PC195 calls native299 SetRotation with BaseRot plus the selected
KeyRot. Its native exec thunk103015AA targets103E31B0..103E3275: evaluate the
required Rotator, finish arguments, prepare zero Delta/fresh Hit and call
actual XLevel.MoveActor virtual+88 at103E3264 with all four options0. Return
the native MoveActor Bool. Therefore both wrappers require the same complete
MoveActor transaction; direct transform writes or a zero-only native266 prefix
do not complete this Training startup requirement. This is static evidence,
not a new movement/rotation implementation or successful world-startup probe.

The pinned SurrealEngine revision
`677ee14c5b83486e6634687953779aafb7973ad6`, UActor_Phys.cpp/TryMove,
is explanatory reference only. It has materially different mutation,
attachment and callback ordering. Do not substitute that order for retail.

## Metadata and wrapper

Engine.u Actor.Move export4571 is native266, flags`0x401`:

| Property | Type | Dimension | Flags | Constraint |
| --- | --- | --- | --- | --- |
| Delta | StructProperty | 1 | `0x80` | Core.Object.Vector |
| ReturnValue | BoolProperty | 1 | `0x580` | — |

Actor.execMove thunk`0x10303ACB` reaches`0x103E3030`–`0x103E30ED`.
It evaluates the required Delta, finishes arguments, then captures the
actor's then-current Rotation. It prepares FCheckResult and calls **actual
native Actor.XLevel** virtual`+0x88`, not reflected Actor.Level:

```
MoveActor(actor, Delta, currentRotation, Hit,
          bTest=0, bIgnorePawns=0, bIgnoreBases=0, bNoFail=0)
```

ULevel.MoveActor thunk`0x10303C60` reaches
`0x103990E0`–`0x10399F5F`. The option labels above describe their observed
behavior; the exported signature itself has four unnamed integer arguments.
Its frame slots are actor`+8`, Delta`+0C..14`, Rotation`+18..20`, Hit`+24`,
options`+28,+2C,+30,+34`. Native266's return is the native result, not a
comparison of requested and final positions.

## Authored fields and native identities

Relevant exact scalar property declarations:

| Declaring class/field | Type | Dimension | Flags | Constraint |
| --- | --- | --- | --- | --- |
| Actor.bStatic | Bool | 1 | `0x03` | — |
| Actor.bMovable | Bool | 1 | `0x01` | — |
| Actor.bCollideActors | Bool | 1 | `0x23` | — |
| Actor.bCollideWorld | Bool | 1 | `0x21` | — |
| Actor.bBlockActors | Bool | 1 | `0x21` | — |
| Actor.bBlockPlayers | Bool | 1 | `0x21` | — |
| Actor.Location | Struct | 1 | `0x23` | Core.Object.Vector |
| Actor.Rotation | Struct | 1 | `0x23` | Core.Object.Rotator |
| Actor.Level | Object | 1 | `0x02` | Engine.LevelInfo |
| Actor.XLevel | Object | 1 | `0x2002` | Engine.Level |
| Actor.Base | Object | 1 | `0x22` | Engine.Actor |
| Actor.StandingCount | Byte | 1 | `0x02` | — |
| Actor.Touching | Object | 4 | `0x02` | Engine.Actor |
| Actor.Brush | Object | 1 | `0x2A` | Engine.Model |

Actor native`+0x28` is the first **authored Bool bitfield**, not UObject
ObjectFlags. The linked original Actor child chain puts bStatic at bit1 and
bMovable at bit`0x80000`. Native`+0x1DC` bits1/2/4/8 are respectively
bCollideActors/bCollideWorld/bBlockActors/bBlockPlayers, independently checked
by execSetCollision. Native Level`+0xA8`, XLevel`+0xAC`, Base`+0xC4`,
StandingCount`+0xD8`, Touching`+0xE8`, Location`+0x114`, Rotation`+0x120`,
Brush`+0x178`, Radius`+0x1D4`, Height`+0x1D8` are distinct authorities.

A dynamic brush here means nonnull Brush AND actual Engine.Brush ancestry
AND !bStatic. A nonnull Brush field on a different actor class is not enough.
Class checks use genuine UClass ancestry, not a leaf-name match.

## Ordered retail operation

### 1. Eligibility and genuine small-motion branches

At`0x1039912D`, return false when `(bStatic || !bMovable) && !GIsEditor`.
GIsEditor is the actual imported global at IAT`0x10599C94`. This return
does not write the caller Hit. There is **no** initial bDeleteMe or
bBegunPlay gate in this body.

Each absolute Delta component must be strictly less than **Double0.0001**
(`3F1A36E2EB1C432D`, `0x1042AC40`) to enter the small branch. If requested
Rotation exactly equals current three-int Rotation, return true without
writing Hit. Otherwise if StandingCount is zero and this is not a dynamic
brush, write Rotation alone and return true. All other cases enter the full
operation. These branches are part of the full contract, not a sufficient
implementation of Move266.

### 2. Hit initialization and movement query preparation

At`0x10399240`–`0x10399294`, reset all44 FCheckResult bytes: Next/Actor/
Location/Normal/Primitive zero, Time Float1, Item int−1. Profiling/counting
updates after this point are not gameplay state; any portable omission must
be documented as profiling only.

If all absolute Delta components are below the same Double0.0001 threshold,
stored size is Float+0 and direction is the original Delta Float vector.
Otherwise compute retained PC64 `(Dz²+Dy²)+Dx²`, store Double, call appSqrt,
retain its PC64 root, FST Float size **without reloading it**, then compute
retained Double1/root and FSTP Float inverse. Direction is
Float3(Delta*storedFloatInverse). This differs from blindly calling a generic
normalization helper with a different sum/store order.

ExtendedDelta is Float3(Delta+2*direction). X/Y doubled directions spill Float
before addition; the doubled Z stays retained through its addition. The
query endpoint is Float3(current Location+ExtendedDelta), start=current
Location and extent=(CollisionRadius,CollisionRadius,CollisionHeight).

At`0x1039940E`–`0x103995CE`, the query is skipped if both collision flags
are false, the source is a dynamic brush, or all three Delta components are
exactly zero. Otherwise call actual native ULevel.MultiLineCheck virtual
`+0xC8` with native memstack/end/start/extent, actor-check flag from
bCollideActors, actual XLevel.Actors[0] LevelInfo when bCollideWorld (else
None), and final byte0. It does not pass a source actor; later filtering
removes self/base contacts. Keep the distinction between this native slot0
and the source's reflected Actor.Level.

### 3. First blocking hit, in original returned order

Only scan for blocking when `(bCollideWorld || bBlockActors ||
bBlockPlayers)`. Walk the returned linked hits in their original order.
At`0x103995EA` onward:

- If bIgnorePawns is nonzero, skip a nonstatic actor with actual Pawn **or
  Decoration** ancestry. Static instances do not take this skip.
- If bIgnoreBases is nonzero and source.IsBasedOn(candidate), skip.
- Always skip candidate.IsBasedOn(source). IsBasedOn includes self identity,
  so this also filters the source itself.
- Set the potential-touch flag for a remaining candidate, whether or not it
  will block.
- Apply the blocking matrix below. First blocking result is copied in full
  (44 bytes) into caller Hit, then stop scanning.

The inlined matrix matches original IsBlockedBy:

| Ordered condition | Blocks when |
| --- | --- |
| candidate==source.Level | source.bCollideWorld |
| candidate is a brush | source.bCollideWorld and candidate's player/actor block bit selected by source.GetPlayerPawn() |
| source is a brush | candidate.bCollideWorld and source's player/actor block bit selected by candidate.GetPlayerPawn() |
| otherwise | candidate's selected block bit AND source's selected block bit |

For the ordinary case, each side selects bBlockPlayers when the **other**
side has nonnull GetPlayerPawn or Projectile ancestry, else bBlockActors.
The brush branches select via GetPlayerPawn only, without the Projectile
test. GetPlayerPawn is actual PlayerPawn ancestry plus nonnull native Player,
not bIsPlayer. Do not add Owner exclusion, leaf-class guesses or a same-level
filter absent from the retail body.

### 4. Two-unit backoff and Time correction

At`0x10399795`–`0x103998A0`, if Hit.Time<1 and !bNoFail:

```
distanceRetained = (storedFloatSize+Float2)*Hit.Time
storedDistance   = Float(distanceRetained)    // FST; retained still compared
if distanceRetained<=2:
    actualDelta=Float3(0); Hit.Time=Float0
else:
    actualDelta = Float3(Float3(ExtendedDelta*Hit.Time)
                        - Float3(direction*Float2))
    Hit.Time = Float((storedDistance-Float2)/storedFloatSize)
```

Otherwise actualDelta remains requested Delta. This does not recompute
Hit.Location/Normal/Item to equal the committed actor position. bNoFail skips
this correction, not the query or blocking-hit selection.

### 5. Based actor movement BEFORE parent mutation

At`0x103998A0`–`0x10399BB5`, only if StandingCount is nonzero and !bTest,
scan actual native ULevel.Actors from index0 upward. Re-read current count
and slot after synchronous callbacks; do not substitute a snapshot of the
portable basedActors list. For each nonnull actor whose current Base equals
source, move it recursively before changing the source transform.

Yaw delta is signed32 wrapped requestedYaw−currentYaw. If any requested
rotation component differs, a yaw-only coordinate transform uses
`(requestedYaw & 0xFFFC) - (currentYaw & 0xFFFC)` to rotate the offset
between source and child, producing a Float rotational delta. The complete
FCoords/trigonometric store contract needs separate proof before a portable
rotating-brush adapter claims parity. Child movement gets actualDelta plus
that rotational delta, child Rotation with the raw yaw delta added, a fresh
Hit, and options`0,0,1,0`. The child Move result is ignored. If child has
actual Pawn ancestry, add raw yaw delta to child.ViewRotation.Yaw after the
recursive call. No callback-deletion shortcut occurs in this native body.

### 6. Encroachment BEFORE parent mutation

At`0x10399BB5`–`0x10399C9A`, unless bTest, bNoFail or actual Pawn ancestry,
call actual native CheckEncroachment virtual`+0xB8` with source,
Float3(current source.Location+actualDelta), requested Rotation and final
integer0. True returns false from MoveActor before parent hash/transform
mutation. The zero final integer does **not** remove the encroachment query.
Child and callback changes already occurred, so a later portable refusal must
roll back the entire root, not only the source transform.

### 7. Parent collision and transform mutation

At`0x10399C9A`–`0x10399D17`: if current source.bCollideActors and actual
ULevel.CollisionHash exists, call native RemoveActor. Then Float-add
actualDelta componentwise to current source.Location and assign requested
Rotation. Re-read current bCollideActors/hash and call AddActor. Do not repair
stale reflected hash membership via a render collider or silently rebuild
unrelated actors.

**bTest still performs this mutation.** It is not the pin's dry-run early
return: it suppresses child carrying, encroachment and later Bump/Touch
callbacks, but writes transforms/hash and still calls SetActorZone.

### 8. Bump, Touch and UnTouch

Unless bTest, at`0x10399D17`–`0x10399F03`:

- If nonnull Hit.Actor equals actual XLevel.Actors[0], invoke source.BumpWall
  with (Hit.Normal,Hit.Location), not HitWall.
- Otherwise if nonnull Hit.Actor and !source.IsBasedOn(Hit.Actor), invoke
  Hit.Actor.Bump(source) first, then source.Bump(current Hit.Actor).
  This source-based-on-hit direction differs from the pinned implementation.
- If potential-touch flag is set OR either source block flag is false, scan
  original hit links while hit.Time<current Hit.Time (strict). Exclude
  candidate.IsBasedOn(source), and when bIgnoreBases also exclude
  source.IsBasedOn(candidate). If !source.IsBlockedBy(candidate), call native
  source.BeginTouch(candidate). Predicates are evaluated after prior callbacks.
- Walk the live four Touching slots in index order. For each nonnull peer not
  currently overlapping, call source.EndTouch(peer,0). Do not snapshot peers,
  pre-sort them, or deduplicate links.

There is no outer bBegunPlay gate. Actual Actor.ProcessEvent supplies each
event's begun/deletion/probe behavior. Native relationship storage changes
must not be dropped simply because an event is presently suppressed.

Canonical Engine.Actor event schemas (all flags`0x800`, native0):

| Event | Inputs (each dim1 flags`0x80`) | Return |
| --- | --- | --- |
| BumpWall | HitNormal Vector, HitLocation Vector | none |
| Bump | Other Actor | none |
| Touch / UnTouch | Other Actor | none |
| EncroachingOn | Other Actor | Bool flags`0x580` |
| EncroachedBy | Other Actor | none |

### 9. Zone and native return

Call native SetActorZone virtual`+0xAC` with `(source,bTest,0)`, even for
bTest. Pop native memmark, then return `Hit.Time>Double0` (strict). A partial
move can therefore return true. Profiling counters/RDTSC are not the return
authority.

## Native helper details and remaining dependencies

- IsBasedOn thunk`0x103020DB` →`0x1034C8D0`–`0x1034C8F0` tests self first
  then follows Base until None. A cycle needs a portable shared-budget refusal,
  not retail's unbounded loop.
- GetPlayerPawn thunk`0x10302E4B` →`0x1034C540`–`0x1034C577` checks actual
  PlayerPawn ancestry and native Player`+0x5D4` nonnull, on this receiver only.
  It is NOT script native720's current-Level scan. The latter must not be used
  for movement blocking; see ORIGINAL-ACTOR-BLOCKING-PREDICATES.md.
- IsBlockedBy thunk`0x10301785` →`0x1034C5E0`–`0x1034C828` supplies the
  matrix above; it does not itself require bCollideActors.
- BeginTouch thunk`0x1030177B` →`0x10369570` invokes one-sided helper
  `0x10369630`, then invokes the other side only if the first returns true.
  It uses the **last** empty slot, stores before Touch, and rereads that exact
  slot after callback; removed link means false and no second-side insertion.
  Its full-capacity replacement rules include native byte`+0x30` and Pawn
  priority, so an all-slots-full path still needs its complete retail adapter.
- EndTouch thunk`0x10302365` →`0x10369970`–`0x10369A48` clears the first
  matching self slot, calls self.UnTouch unless bNoNotifySelf, then clears the
  first peer slot and calls peer.UnTouch. Retail does not use the current
  pin-based portable touchEventSent or per-side bDeleteMe shortcuts.
- MultiLineCheck thunk`0x10302AA9` →`0x1039B220` is not a trivial concatenation
  of world and actor hits. It shortens the query after Model collision,
  renormalizes hit fractions, has64 local result slots and mover/coplanar
  collision rules before sorting and linking. Complete actor-cylinder/brush
  sweeps, ordering and native identities must be integrated separately.
- CheckEncroachment thunk`0x10302536` →`0x1039A350` has its own swept/overlap,
  push/recheck/EncroachingOn/EncroachedBy/Touch semantics. Ordinary veto
  preserves earlier successful pushes/events; only portable errors trigger
  outer rollback. Its complete later source audit is
  ORIGINAL-LEVEL-ENCROACHMENT.md. The early false mask alone is insufficient.

The verified null-owner Model extent kernel is reusable, but neither it nor
the simplified render registry proves full MoveActor. Owner/mover transforms,
native Model/cache authority, full actor trace service, native relationship
storage/callbacks, FCoords child rotation and one enclosing transaction remain
required. All refusals after child movement/callbacks must poison and roll back
the root VM transaction, including values, links, actors, collision registry,
RNG, clocks, publication and saves. No zero-only Move implementation or fake
world-startup success is authorized by this audit.
