# Original MakeNoise512 and Pawn.CanHear

Read-only retail audit, 2026-10-16. Native512 is **still unimplemented**.
The separate native ULevel clock phase and continuation are described in
ORIGINAL-LEVEL-CLOCK.md; final build/test evidence is recorded in STATUS.md.
This document does not claim a complete world tick, original player possession,
audible playback, AI hearing or playable crate destruction.

## Primary evidence and identities

The owned `D:\Steam\steamapps\common\Deus Ex\System\Engine.dll` SHA256 was
rechecked as `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
The matching Core.dll SHA256 is
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
No original executable or DLL is loaded or executed. Addresses below are
preferred-image PE32 virtual addresses, not portable layouts or live pointers.
Reproduce with read-only `objdump -d -Mintel --start-address=... --stop-address=...`;
use `objdump -s` for the constants. Native metadata remains owned package data.

- `Engine.Actor.MakeNoise`, native512: wrapper body `103bcd20..103bcdd9`.
- Native `AActor::MakeNoise`: export thunk `103031a7`, body
  `103bfad0..103c00e8`.
- Native `APawn::CanHear`: export thunk `1030286a`, body
  `103bf7b0..103bf9ef`. This is **not** Deus Ex `AICanHear`.
- `ENGINE_HearNoise` FName export is `105836a4`. MakeNoise's explicit
  IsProbing argument is literal342 (`0x156`), corresponding to probe bit42.
- Core `UObject::IsProbing`: export `1010258b -> 10115670..101156b6`.
- Actual `UModel::FastLineCheck`: `10301a37 -> 103f3280`; see
  [ORIGINAL-FAST-TRACE.md](ORIGINAL-FAST-TRACE.md). Actor collision rays,
  Model.LineCheck flags6 and the renderer's visibility helpers are not substitutes.

These relationships are actual native fields, not arbitrary nearest-name script
properties. Relevant retail offsets are:

| Native owner | Field | Offset/type |
| --- | --- | --- |
| Actor | XLevel / Level / Instigator | `+ac` / `+a8` / `+bc`, object pointers |
| Actor | Location | `+114,+118,+11c`, Float X/Y/Z |
| ULevel | Actors / Model / TimeSeconds | `+2c` array data / `+98` / `+dc` Double |
| LevelInfo | NetMode / Game / PawnList | `+518` Byte / `+544` / `+54c` |
| Pawn | bIsPlayer | `+318`, bit1 |
| Pawn | Alertness / Stimulus / HearingThreshold | `+39c` / `+3a0` / `+3ac`, Float |
| Pawn | Enemy | `+3cc` |
| Pawn | noise1spot / noise1time / noise1loudness | `+464` Vector / `+470` Float / `+478` Float |
| Pawn | noise2spot / noise2time / noise2loudness | `+47c` Vector / `+488` Float / `+490` Float |
| Pawn | nextPawn / PlayerReplicationInfo | `+498` / `+558` |
| GameInfo | bTeamGame | `+328`, bit9 |
| PlayerReplicationInfo | Team | `+340` Byte |

Do not copy these byte offsets into portable objects. Resolve and validate the
actual declaring schemas, class relationships and immutable native ownership.
`Instigator` is not Owner; native ULevel Double TimeSeconds is not the reflected
LevelInfo Float mirror. The two cache timestamps themselves are Float.

## Wrapper and entry ordering

The wrapper initializes its Loudness Float destination to0 at `103bcd29`,
evaluates the required input in the caller frame at `103bcd32..103bcd4b`, and
consumes EndFunctionParms. It then obtains the source's actual native XLevel,
asserts/checks `XLevel.Actors[0]` is LevelInfo, and returns if that LevelInfo's
NetMode is3 (`103bcdbb..103bcdc7`). No Instigator/cache work precedes that gate.

The non-client branch calls the native body with the captured Loudness Float.
The body captures the **initial** source.Instigator once into `ebx` and a local
at `103bfafa..103bfb00`. None is a genuine immediate return. A supplied real
Pawn is not interchangeable with the missing original VR player.

Native level time is loaded once from `source.XLevel.TimeSeconds` as Double at
`103bfb34..103bfb3a`. The unrelated `XLevel+1144` writes and conditional RDTSC
calls are cycle-counter profiling. They are not the gameplay clock and must not
be used to seed or advance it. Omitting those profiling statistics is an explicit
embedding limit, not permission to invent gameplay time.

## Exact cache suppression and replacement

Ordinary simulation arithmetic uses x87 PC64, round-to-nearest-even. Retained
intermediates must not be replaced by host double, host long double, SSE Float
expressions, epsilon comparisons or premature Float spills.

For each cache, in order1 then2, suppress the whole noise when all are true:

1. `cache.time > PC64(now - Double0.2)`.
2. Unspilled PC64 squared distance from `cache.spot` to current source.Location
   is strictly `< Float2500`.
3. `cache.loudness >= PC64(Double0.9 * LoudnessFloat)`.

The squared-distance instruction order is **`(dx*dx + dy*dy) + dz*dz`**.
Each subtraction remains PC64; there is no Float delta/sum spill. This differs
from CheckHearSound's z/y/x sum. Cache1 is `103bfb42..103bfbb8`; cache2 is
`103bfbdc..103bfc4e`. The second cache is not evaluated after a first-cache hit.

If not suppressed, replacement precedence is:

1. Cache1.time strictly `< PC64(now - Double0.18)` -> write cache1.
2. Else cache2.time strictly `< PC64(now - Double0.18)` -> write cache2.
3. Else cache1 spot distance `<2500` **and** cache1.loudness `<=L` -> write cache1.
4. Else cache2.loudness `<=L` -> write **cache1**, not cache2.
5. Else no cache write, but continue recipient routing.

The last branch is literal retail behavior at `103bfda1..103bfde4`; the pinned
engine differs. Replacement writes Float(now), source.Location's current stored
Float components and LoudnessFloat. The native does not update another cache
field, normalize Loudness, clamp a negative Loudness or require nonzero Loudness.
Writes occur at `103bfca2..103bfcc9`, `103bfce5..103bfd0a`,
`103bfd6c..103bfd99` and `103bfdb1..103bfdde`.

Constants and exact storage:

| Meaning | VA | Bits/type |
| --- | --- | --- |
| recent window0.2 | `1042b8e8` | `3fc999999999999a`, Double |
| stale window0.18 | `10431480` | `3fc70a3d70a3d70a`, Double |
| suppression factor0.9 | `1042f570` | `3feccccccccccccd`, Double |
| nearby squared distance2500 | `1043148c` | `451c4000`, Float |

Any portable finite/overflow/work-budget refusal must poison the enclosing root
transaction. It must restore the caches with every preceding script write,
native clock/RNG operation and subsequent failed callback.

## Live recipient selection and callbacks

After cache handling, the **captured initial Instigator** selects a routing
branch (`103bfde8..103bfe08`): player-aware when its bIsPlayer is set, or its
nonnull Enemy has bIsPlayer set. This branch choice is made once before walking
the list, not recomputed after callbacks.

Both branches seed the list through **source.nativeXLevel.Actors[0].PawnList**.
Unlike PlaySound264's nonlocal loop, MakeNoise does not seed from the script-
mutable Actor.Level field. The source's current Instigator is reread separately
to exclude that current actor from each iteration. If a prior callback changes
source.Instigator, the exclusion changes; the captured instigator supplied to
CanHear does not.

- Player-aware branch (`103bff5f..103c0078`) skips actual PlayerPawn class
  instances, then skips current source.Instigator, then checks HearNoise probing.
- Nonplayer branch (`103bfe0e..103bff30`) skips current source.Instigator, checks
  HearNoise probing, then requires listener.IsA(source.Class) **or**
  source.IsA(listener.Class). Compare actual UClass ancestry, not name suffixes.
- Neither loop inserts an extra same-Level, bIsPlayer-listener or bDeleteMe gate.
- CanHear receives a per-iteration copy of current source.Location, the captured
  LoudnessFloat and the initial captured Instigator.
- Accepted listeners receive synchronous actual `ENGINE_HearNoise` ProcessEvent
  with arguments **Loudness, sourceActor**, not Loudness/Instigator.
- `current.nextPawn` is read **after** callback return (`103bff2a` / `103c0072`).
  No frozen list snapshot, sorting or deduplication may replace the live walk.
  Bounded cycle/work exhaustion must roll back, not silently truncate delivery.

The explicit probe precedes CanHear, so a disabled listener must not acquire
Stimulus writes just because its eventual ProcessEvent would be suppressed.
Original Core.IsProbing returns true outside FName300..363 or when no StateFrame
exists; otherwise it reads the live positive uint64 mask at StateFrame+1c/+20.
Do not substitute fresh class/state metadata masks when an actual live frame
exists. Existing CurrentDispatchContext/liveProbeMask plumbing is relevant.

## Pawn.CanHear arithmetic and side effects

CanHear itself does not perform the caller's class-compatibility, PlayerPawn or
IsProbing tests. At entry it computes listener.Location minus the supplied
noiseLocation, retaining each subtraction/square and the **x/y/z** sum in PC64.
It then immediately Float-stores that sum at `103bf80f`; all following distance
tests and the quotient use this **stored Float**, not the retained geometry.
There is no ViewTarget, eye-height or collision-actor query.

The same-team branch requires all of:

- Listener.bIsPlayer.
- Actual listener.nativeXLevel.Actors[0].Game.bTeamGame.
- Supplied captured Instigator.bIsPlayer.
- Both PlayerReplicationInfo pointers nonnull and their Team Bytes equal.

These gates are `103bf81e..103bf8c6`. Team range accepts when stored distance
`<= PC64(PC64(L*L) * Float16000000)` and then goes directly to BSP
(`103bf8c8..103bf8dc`). It **does not** write Stimulus and **does not** take the
general coincident-distance shortcut.

The general branch (`103bf8f7..103bf998`) does:

1. Accept range when stored distance `<= PC64(PC64(L*L) * Float4000000)`.
2. If stored distance equals Float0, return true immediately, without Stimulus
   or BSP work. Subnormal geometry can round to this legitimate zero branch.
3. Compute retained `q = PC64(Float1200000 / distanceFloat)` and Float-store q
   **without popping/reloading the retained quotient** (`103bf935..103bf93e`).
4. Compare retained q against Float2. If q>2, choose `p=Float2`; otherwise choose
   **the Float-spilled q**, not the retained quotient.
5. Choose `a = (p > Float0.5 ? Float0.5 : p)` using the retail compare branch.
6. Retain `stimulus = PC64(PC64(p*L) + PC64(a*AlertnessFloat))`. The instruction
   order multiplies a*Alertness first, then p*L, then adds.
7. Float-store Stimulus at `103bf987`, again **without popping/reloading**.
   Compare the retained sum against HearingThresholdFloat; accept equality.
8. Only an accepted threshold reaches BSP. A threshold refusal still retains
   the Stimulus write if the surrounding root later commits.

Additional exact Float constants are:

| Meaning | VA | Bits |
| --- | --- | --- |
| team squared range16000000 | `10431478` | `4b742400` |
| general squared range4000000 | `10430a5c` | `4a742400` |
| perceived numerator1200000 | `1043147c` | `49927c00` |
| perceived cap2 | `10430aac` | `40000000` |
| alertness cap0.5 | `10426d74` | `3f000000` |

At `103bf998..103bf9d5`, the actual listener.nativeXLevel.Model.FastLineCheck
receives **End=noiseLocation, Start=listener.Location**. Stack construction
places listener.Location in the high/second by-value argument, and supplied
noiseLocation in the low/first argument. Preserve this direction: the bounded
retail BSP traversal has directed rounding/order and is not assumed numerically
symmetric. Return the native Bool result; do not add attenuation or an AI trace.

## Implementation and verification obligations

Implement native512 only with real loader-owned native level/time authority,
actual Pawn links, exact cached declaring fields and original event execution
in the same VM root journal. The clock phase alone neither implements noise
nor authorizes calling an arbitrary NPC the VR player's Instigator.

Required focused controls include both suppression caches and strict equalities;
Double-versus-Float time windows; x/y/z order; replacement-cache1 retail quirk;
no-write-but-route; zero/negative Loudness; live callback changes to nextPawn,
source.Location and source.Instigator; initial captured Instigator identity;
disabled HearNoise before Stimulus; class ancestry/player-aware branches; team
range versus general range; stored-zero shortcut; retained quotient/cap and
retained Stimulus threshold; directed native BSP; and complete rollback after
a later sound/fragment/Destroy failure. Generated tests and scalar oracles must
not be represented as an original possessed-player or on-device playthrough.
