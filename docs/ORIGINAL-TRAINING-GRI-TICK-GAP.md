# First Training: exact inherited GameReplicationInfo update

Implementation and independent source evidence, 2026-10-26. Only first
`Maps/00_Training.dx` is integrated/tested. Live published gameplay, collectible
loot and automatic stacked-crate gravity remain unfinished.

## Reached gate before this increment

After exact Decal admission and the original native721 script adapter, cold
and warm Training updates reach `00_Training.GameReplicationInfo0`, exact
native `Engine.GameReplicationInfo`, at239,200 instructions /19,362 writes,
141 slots /140 calls,13 stasis returns and12 deferred entries. The conservative
native-class guard refuses Engine.GameReplicationInfo. Full gameplay rollback
and cold same-filename map replacement pass; the frame still does not complete.

## Independent owned source proof

Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
Original binaries are inspected only as data, never executed or loaded.

Actual top-level native Class metadata, including matching first payload
superclass references and ObjClass0/ObjOuter0:

| Class | Ref/base | Object flags | Payload offset/length |
| --- | --- | --- | --- |
| GameReplicationInfo |108/805 |040F0004 |86078/262 |
| ReplicationInfo |805/91 |040F0004 |106561/93 |
| Info |91/2 |040F0004 |85749/91 |
| Actor |2/-3 |040F0004 |61225/1236 |

The extra native ReplicationInfo link is required. Neither the two-link
GameInfo/Info guard nor direct Decal/Actor guard proves this ancestry.

Engine.dll export-name/EAT645 (ordinal646),
`??_7AGameReplicationInfo@@6B@`, RVA127428 gives vtable10427428:

| Slot/cell | Pointer / original Actor service |
| --- | --- |
| +10/10427438 |10302D74 ProcessEvent |
| +14/1042743C |10301F00 ProcessState ->103E5B60 |
| +2C/10427454 |10302167 IsPendingKill |
| +64/1042748C |10303DAA Tick ->103A1AA0 |
| +8C/104274B4 |10303242 performPhysics ->103C9770 |

Fresh thunk bytes match prior Actor audits: ProcessState E95B3C0E00,
Tick E9F1DC0900, physics E929650C00. These slots do not authorize arbitrary
native ReplicationInfo descendants, scalar teardown or skipped callbacks.

## Original selected timer remains live

Owned original PostBeginPlay is110 logical bytes/native0/flags102 and calls
SetTimer native280 with Float0.2 and Bool true. Original Timer is384 logical
bytes/native0/flags102: its NetMode3 branch updates the countdown, then it
clears PRIArray32, enumerates actual PlayerReplicationInfo through native304
AllActors, updates scores/UpdateTimer and copies Game.NumPlayers when Game
is present. No local GameReplicationInfo.Tick export replaces inherited Tick.
Evidence: artifacts/gri-owned-script-proof-20261026.log and the bounded owned
class reader; original files are read only as data.

An inherited Tick vtable is not proof this object is inert. Its non-Pawn timer
must retain AdvanceTimer -> execution.CallEvent("Timer",false,{}) and all
selected original behavior. It must not be rerouted through the Pawn-only
native callback wrapper or suppressed because this is standalone.

## Exact admission, verification requirements

Authenticate the full canonical native GameReplicationInfo -> ReplicationInfo
-> Info -> Actor chain, preserve selected script/state/timer/lifespan/physics,
and separately admit the proven animation callbacks only for that exact
native class. Keep base ReplicationInfo/Info and unknown nearer native classes
closed. Do not skip this object because the target is standalone; standalone
still reaches it in the actual original first-map update.

Add generated cross-package metadata, Float event, owner/deferred/checkpoint,
actual animation callback and whole-root late/write/work undo controls with
malformed intermediate chains. Repeat unfiltered Training, paired doors and
actual crowbar attacks, preserving cold/warm model-proof lifetime checks.
This is not a completed startup, collectible loot, gravity or headset claim.

The implementation uses a separate exact GRI guard in both Tick and animation
admission. It checks canonical GRI/ReplicationInfo/Info/Actor identities,
immediate links and all four native Class metadata packets. The existing
bounded nearest-native selection still refuses unknown nearer children, and
neither base ReplicationInfo nor Info is admitted. The two-link rules helper
is unchanged. No timer, state, lifespan, physics or original script is replaced.

## Generated production verification

The shared matrix now has an independent exact GRI mode with genuine serialized
cross-package native GRI/ReplicationInfo/Info/Actor links, selected Float Tick,
owner/deferred LIFO and parity/checkpoint continuation. It retains real MAIN
tween/checked ProcessEvent/IsPendingKill/selected AnimEnd and ordinary/animated
late/write/work whole-root undo. Seven malformed/base controls cover missing
native flags on ReplicationInfo, Info and Actor, skipped/rewired intermediate
links, and separately selected native ReplicationInfo/Info parents. An unknown
nearer native child still refuses at its actual boundary.

The generated selected Timer runs after Tick and observes exact stored rate/
residual; equality expires, missed periods coalesce, saved continuation does
not replay and the next period expires normally. Timer native999, later Tick
failure and every timer-root write cutoff plus early/middle/final native-work
cutoffs fully undo save/revision/parity/GC state. This is generated callback
coverage, not a claim the original384-byte GRI.Timer has run live.

These controls add3,102 checks/67 refusals. The complete production driver
reaches18,747/362 before its final malformed-schema control and exits0. Full
desktop/Android-native/strict GNU+ARM64 checks pass; shared CTest217 executed /
256 registered /39 explicit owned-data skips /zero failures,106.72s. Separate
owned crate415/5, deferred429/21, animation588/24 and FastTrace252/0 pass.

## Actual first-map verification and next boundary

Unfiltered cold/warm Training repeat2 now passes GRI and reaches Mission000,
class DeusEx.Mission00, nearest native Engine.Info. Both attempts retain239,200
instructions/19,363 writes,142 slots/141 calls,13 stasis returns/12 deferred
entries. Paired-door repeat2 retains both56-unit moves/two completion callbacks
at239,536/19,431 and the same next guard. The exact-expected-boundary tests exit0
only after verifying this refusal and full rollback; startup still does not
complete. See ORIGINAL-TRAINING-INFO-TICK-GAP.md for independent next-step proof.

Two actual crowbar Fire calls repeat228,837/26,047,48 rows/two cycles/two
returned notifies/four actual traces. Lower-crate HP10->4->-2/removal, six
WoodFragments, surviving Lockpick and two impact-spawner births remain exact.
Their later bound PlayerPawn ConsoleCommand FExec refusal is unchanged.

Every mode retains whole save/state/revision/hash/RNG/client/lookup/Actor/GC
undo, immutable Model proof cold0->1/warm1->1, and generation1->2 same-filename
replacement starting cold. Final audit confirms unchanged length/mtime/SHA256
for45 original files and all6 frozen compiler inputs, with all jobs terminal.
Evidence: artifacts/gri-final-*.log; compiler inputs stayed frozen throughout.

No other map or original executable code was run. No APK was packaged. The
current Quest trigger still reaches legacy Pawn-only damage, not this genuine
weapon path; committing startup, original player-input/inventory/frame schedule
and actor publication bridges remain necessary. Live crate destruction,
collectible loot and automatic stacked-crate gravity are unfinished.
