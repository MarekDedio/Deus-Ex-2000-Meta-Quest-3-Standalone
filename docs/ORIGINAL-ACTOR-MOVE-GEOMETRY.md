# Original MoveActor prequery and backoff arithmetic

First-Training movement prerequisite,2026-10-19. This implements the complete
prequery Delta/size/direction/extended-Delta arithmetic and subsequent two-unit
hit correction, not a publishing MoveActor, child carry, zone service or crate
physics. No other real map is loaded/integrated by these helpers or tests.
No commercial DLL/EXE is loaded or executed. The owned Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

## Source and complete numeric boundaries

Engine MoveActor export1298/thunk10303C60 reaches103990E0. Root read the
complete numeric/branch spans from the owned binary via static LLVM objdump:

- `artifacts/training-move-geometry-prequery-20261019.asm`:
  eligibility/small branches1039912D..1039923D, packet initialization and
  prequery arithmetic10399240..10399406, and query argument preparation.
- `artifacts/training-move-geometry-backoff-20261019.asm`:
  complete hit correction10399795..1039989D.
- `artifacts/training-move-actor-20261019/move-actor-full.asm`:
  separate whole-function audit. Numeric helpers alone do not supply it.
- Core appSqrt IAT10599CA4 is `?appSqrt@@YANN@Z`, thunk101032BA to
 10123290..10123296: FLD Double argument, FSQRT, RET. Its PC64 result is not
 rounded to Double on return. The independent full-function audit also pins
 the threshold's bytes and appSqrt identity; see ORIGINAL-LEVEL-MOVE-ACTOR.md.

The exact small threshold at1042AC40 is Double bits3F1A36E2EB1C432D (0.0001).
Each component must be strictly inside its absolute threshold. Both original
small tests inspect X,Y,Z lazily, stopping at the first outside component.
This is not a new general zero-motion early-success path: eligibility,
Rotation, StandingCount and moving-brush branches remain the full caller's
responsibility. Exact-zero query suppression is a different later condition.

## Preparation packet

`PrepareOriginalActorMoveGeometry(delta)` returns `OriginalActorMoveGeometry`:
the small flag, Float size, direction and extended Delta. Small input supplies
literal positive-zero size and copies the original three direction words,
including signed zeros/subnormals. Otherwise, using E for retained PC64 and
F/D for explicit Float/Double stores:

```
sum = E(E(delta.z*delta.z + delta.y*delta.y) + delta.x*delta.x)
root = appSqrt(D(sum))
D(root)                 // original scratch FST, not reloaded
size = F(root)          // original FST, not reloaded
inverse = F(E(1/root))  // NOT 1/size
direction = F3(delta * inverse)
```

Direction products are evaluated Z,Y,X then stored X,Y,Z. Extended Delta
retains doubled direction.Z, separately spills doubled Y and X to Float,
then stores extended Z,Y,X. X adds doubledX to delta.X; Y/Z add their doubled
directions to delta.Y/Z. A generic SafeNormal or different squared-sum/root
store schedule is not substituted. Caller endpoint preparation must later
sample the then-current actor Location; this helper does not cache a pose.

## Two-unit correction packet

`BackoffOriginalActorMove(requested, geometry, hitTime, noFail)` returns staged
Delta and Time only. It does not update actor pose, choose a blocker or
recompute Hit.Location/Normal/Item. FCOMP/TEST C0 continues for ordered Time<1
and unordered Time. Ordered Time>=1 (including positive infinity), or any
nonzero signed noFail, returns the raw original Delta/Time with no inactive
geometry validation. This preserves the native Boolean option semantics.

When active:

```
distance = E(E(2 + size) * hitTime)
storedDistance = F(distance)  // original FST; comparison still uses distance
if distance <= 2:
    return positive-zero Delta and positive-zero Time
backoff = F3(direction * 2)
scaled  = F3(extendedDelta * hitTime)
actualDelta = F3(scaled - backoff)
actualTime  = F(E(E(storedDistance - 2) / size))
```

The zero gate uses retained distance, but the final Time uses its separately
stored Float scratch. Both distinctions have independent hardware witnesses.
All three backoff/scaled/subtraction operations keep their original Z,Y,X
order. There is no epsilon, clamp to[0,1], inferred positive-only dimension
policy, packet-normalization pass or direct actor transform write.

## Portable domain and executed evidence

Only active arithmetic requires finite Float operands and finite Float stores.
An overflowing scratch store refuses even if a later branch would overwrite
it. Missing/inactive data is not eagerly examined. The pure functions return
staged values and cannot partially publish an actor or caller hit on refusal;
their real enclosing MoveActor must still join the complete VM/hash root.

The independent optimized-GCC hardware-x87 oracle uses its own PC64 precision
scope, explicit Float/Double stores, FSQRT and separate arithmetic. It calls
no production numeric helper. The executed suite passes213020 checks/489
explicit refusals,53342 preparations and52901 corrections, including11044
retained-root,10214 stored-distance and5999 retained-gate witnesses. Wide
finite packets, threshold neighbors, signed zeros, subnormals, extreme values,
nonzero signed noFail and inactive nonfinite data are covered. Non-x86 targets
run deterministic cases and explicitly skip the hardware differential.
Strict ARM64 test syntax passes. Evidence:
`artifacts/training-move-geometry-unit-20261019.log` and
`artifacts/training-move-geometry-arm64-20261019.log`.

This evidence is numeric only. Real movement/callback/child/zone integration,
native266/299, begun world/player startup, falling/landing/Base support,
held-weapon damage, surviving usable loot and headset acceptance remain
required. See ORIGINAL-LEVEL-MOVE-ACTOR.md and TRAINING-FIRST-PRIORITY.md.
