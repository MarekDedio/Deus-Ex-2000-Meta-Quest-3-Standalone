# First Training: the next original SetLocation boundary

Later implementation evidence: TRAINING-SET-LOCATION.md records native267
integration, actual first-map checks and the original Login now returning JC
and reaching possession/HUD initialization. The missing-service statements
below describe the earlier static-audit boundary. Generic hash ActorPointCheck
for broader CheckActors queries remains unsupported; native267's authentic
point queries use CheckActors0, while AdjustSpot uses the real line service.

2026-10-22. Read-only original-binary audit; native267 is NOT implemented by
this increment. Only the whole first `Maps/00_Training.dx` is the actual-map
target. No original EXE/DLL was executed or loaded.

The actual standalone-profile Training startup now constructs the real
`00_Training.DumpLocation0` and reads its original false session flag. Original
`DeusEx.DeusExGameInfo.Login` continues to PC442/opcode97 and explicitly refuses
`Unsupported runtime native 267`. This is the real player placement call, not
an account/login screen. Full startup and playable crate loot/gravity remain
unfinished.

## Original authority

Owned `System/Engine.dll` SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

The authored `Engine.Actor.SetLocation` declaration has native index267,
function flags0x401 and logical size5. `NewLocation` is scalar StructProperty,
flags0x80, referencing `Core.Object.Vector`; ReturnValue is scalar Bool,
flags0x580. Preserve the actual declaring metadata and canonical Vector layout.

`execSetLocation` thunk10302BB2 reaches103E3120..103E3185. It evaluates the
absolute FVector, finishes arguments, reads the receiver's actual XLevel and
invokes Level virtual+8C: `FarMoveActor(actor, newLocation, 0, 0)`.

`FarMoveActor` thunk10301122 reaches10398BA0..10398E35. Its sequence is distinct
from native266/ordinary delta movement:

1. Reject static/nonmovable source Actors outside editor mode.
2. Remove source from the actual hash if bCollideActors and Hash are present.
3. Unless no-check, use original FindSpot when bCollideWorld, or when
   bCollideWhenPlacing and actual LevelInfo.NetMode is not3. Extent is actual
   source CollisionRadius, CollisionRadius, CollisionHeight; placement options
   are0,0. A no-collision source does not acquire fabricated placement checks.
4. On successful placement, non-test/non-no-check calls higher encroachment
   with the same source, adjusted position, freshly read Rotation and touch1.
5. On non-test success, use the live Level Actor slots to detach based children
   with SetBase(None,true) when StandingCount is nonzero; set bJustTeleported.
6. On success, including test mode, copy adjusted position to both Location
   and OldLocation. Source Base is not automatically cleared.
7. Freshly recheck bCollideActors/Hash and reinsert even after ordinary failure.
8. On success call SetActorZone(actor,bTest,0), then return the native result.

All callbacks/hash/coordinate/zone changes must share the caller's VM root and
undo ownership. Existing `native/portable_actor_move.inl` provides useful
encroachment, native coordinate, hash and SetBase/zone paths, but calling
MoveLevelActor with a calculated delta is not this operation.

## Missing placement prerequisites

Original FindSpot thunk103030DF reaches10398480..103989F8. It uses Level
SinglePointCheck virtual+BC and AdjustSpot virtual+B4, with axis/corner
adjustment and a final displacement constraint. These exact kernels/providers
still need implementation and focused validation. Existing primitive point
queries are prerequisites, not proof of a complete Level wrapper.

`native/quest_spawn_placement.h` explicitly ports the pinned SurrealEngine
placement heuristic. Its candidate search is not evidence of this retail
FindSpot behavior. Do not bypass placement, force true, directly overwrite
Location, or substitute that search and claim original SetLocation acceptance.

Next integration: authenticate and implement these bounded original placement
queries, then wire FarMove into existing native transaction machinery and
rerun the original first-Training Login without branch changes.

## Further static placement audit (not runtime acceptance)

Additional bodies: AdjustSpot thunk10302E0A ->10398360..10398434;
SinglePointCheck thunk10303E77 ->1039AAC0..1039AC1F;
MultiPointCheck thunk10302A95 ->1039ACC0..1039AE27.

FindSpot constructs a shared zero Hit with Time1/Item-1. If all extent
components compare zero/unordered under the original x87 C3 test, it calls
SinglePointCheck and returns exactly result==1, without adjustment. For
nonzero extent, its second integer enables an initial point-check fast path;
native267 passes0 and skips that initial check. Copy requested Location to a
mutable candidate S. Diagonal radius is a Float store of
`sqrt(Double((Ez*Ez + Ey*Ey) + Ex*Ex)) + Double(2)` with original spill order.

For sign-1 then+1, always perform three sequential AdjustSpot calls: endpoint
S+(sign*Ex,0,0)/radiusEx, then S+(0,sign*Ey,0)/radiusEy, then
S+(0,0,sign*Ez)/radiusEz. Every call starts from the candidate changed by its
predecessor. Then point-check S; exact1 copies S back to Location and succeeds.
Otherwise execute eight corner adjustments, outer X/middle Y/inner Z signs
-1 then+1, endpoints based on current S and using the diagonal radius. There
is no clear-point early exit inside either adjustment loop. Reject without
changing requested Location when squared displacement exceeds
Double1.5 * Float-squared-extent (unordered also rejects under x87 CF).
Otherwise the final point check uses freshly obtained LevelInfo; copy S back
only on exact1. Exact arithmetic/store order needs tests, not algebraic
reassociation or a replacement candidate grid.

AdjustSpot calls original SingleLineCheck with sourceNone, startS,
end=endpoint, flags6, extentZero and final integer0; it ignores the Bool result.
When Hit.Time compares below1/unordered, update S by
`((Double1.05 - Hit.Time) * Hit.Normal) * radius`. There is no clamp, epsilon
or normalization. X/Y factor-normal products spill Float, then multiply radius
and spill Float, then add S and spill Float. Z remains retained through both
multiplication/addition until its final Float store. The existing
`portable_retail_single_line.inl` is the relevant bounded line-query provider.

Original constants: Double1.05 at1042EAB0 (bits3FF0CCCCCCCCCCCD), Double1.5 at
1042EAC0, Double2 at1042EAD0, Double1 at104298A0, Float0 at10426D54. Preserve
the original branch treatment of unordered values, or explicitly refuse
unsupported active inputs; do not silently replace it with ordinary equality.

SinglePointCheck saves a native scratch mark and calls MultiPointCheck. No
hits returns1 WITHOUT writing caller Hit. Otherwise copy the first full44-byte
packet, traverse Next, and replace only for a strictly nearer Hit.Location
(squared distance to requested point); ties keep the earlier packet. Pop the
mark and return0. MultiPoint first queries Hash.ActorPointCheck only when
CheckActors and actual Hash exist. The world native Model.PointCheck has
ownerNone and literal flags0. A blocked world packet is prepended to actor
hits and its Actor becomes supplied LevelInfo.

Native267's actual FindSpot passes CheckActors0; AdjustSpot's flags6 line
queries still consult the actual native collision machinery. Broader point
queries need the not-yet-implemented hash ActorPointCheck, not a silent empty
result. Existing primitive point queries are reusable only with the correct
world Model/null-owner identity, not a LevelInfo collision cylinder.
