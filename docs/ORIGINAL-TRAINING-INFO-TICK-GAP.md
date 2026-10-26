# First Training: exact inherited mission Info update

Implementation and independent source evidence, 2026-10-26. Only the first
Maps/00_Training.dx is integrated/tested. Published startup, live collectible
loot and automatic stacked-crate gravity remain unfinished.

## Historical reached gate before this increment

After exact GameReplicationInfo admission, the actual update reaches
00_Training.Mission000, class DeusEx.Mission00, nearest native Engine.Info.
Discovery reports239,200 instructions/19,363 writes,142 slots/141 calls,
13 stasis returns/12 deferred entries. It refuses the exact unsupported Info
virtual guard, with whole-root rollback and cold map replacement preserved.

## Owned original proof

Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
Original binaries are inspected as data, never loaded or executed.

Canonical top-level native Engine.Info is export91, immediate base Actor2,
object flags040F0004, offset85749/size91; serialized superclass agrees with
its export. Canonical Actor2 is native040F0004, base Core.Object import-3,
offset61225/size1236. A separate exact Info -> Actor proof is needed: the
rules and GRI wrappers prove their own longer chains, not the Info boundary.

DeusEx.Mission00 is ref19290 -> MissionScript171, flags000F0004,
offset2584578/size646; MissionScript171 -> import-520 (Engine.Info),
flags000F0004, offset319130/size465. Matching payload superclass links and
absence of RF_Native make both script-only, not new native overrides.
DeusEx.u SHA256:
`257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480`.

Engine.dll export-name/EAT647, ordinal648, `??_7AInfo@@6B@`, RVA1272D0:

| Slot/cell | Pointer / original Actor service |
| --- | --- |
| +10/104272E0 |10302D74 ProcessEvent |
| +14/104272E4 |10301F00 ->103E5B60 ProcessState |
| +2C/104272FC |10302167 IsPendingKill |
| +64/10427334 |10303DAA ->103A1AA0 Tick |
| +8C/1042735C |10303242 ->103C9770 performPhysics |

These are independently matching inherited Actor methods, not permission for
unknown nearer native Info children or unrelated scalar teardown.

## Why the mission object is necessary for crates

There is no direct Mission00.Tick or MissionScript.Tick export. Original
Mission00.Timer is1114 logical bytes/native0/flags2 and calls the real
MissionScript.Timer first. Its first-map branch counts Containers tagged
Cratepick1/Cratepick2 and triggers DL_Lockpick plus MS_CratesBroken when fewer
than two remain. Skipping this actor would lose original tutorial progression
even if a crate visually disappeared. MissionScript.Timer itself is58 logical
bytes/native0/flags2. These are static observations, not live Timer acceptance.
Evidence: artifacts/gri-next-info-proof-20261026.log.

MissionScript.PostPostBeginPlay is11 logical bytes/native0/flags2 and calls
SetTimer(checkTime,true). Base Timer retries InitStateMachine while flags=None
and invokes FirstFrame for a present Player with PlayerTraveling. Preserve
this initialization and the base Timer call as well as selected Mission00 code.

## Exact admission and verification requirements

Add exact canonical native Info/Actor admission and separately checked
ProcessEvent/IsPendingKill callbacks, preserving selected script, state,
timer, lifespan, physics and nearest-native dispatch. Update existing
GameInfo/Mutator/GRI/DeusExLevelInfo parent-Info refusal fixtures to legitimate
positive Info coverage; do not preserve an obsolete refusal just to keep test
counts unchanged. Base ReplicationInfo and unknown native children remain
closed; malformed Info/Actor links/native flags must still refuse.

Use the existing generated selected-Tick/animation/timer/owner/checkpoint and
whole-root undo matrices, then repeat unfiltered actual Training, paired doors
and real crowbar attacks. Completing this guard alone does not publish startup,
controller weapon input, collectible loot, automatic gravity or an APK.

The implementation now has its own exact native Info/Actor guard, used by both
Tick and animation admission. Both canonical nodes require genuine Class
metadata, valid links, RF_Native and no class-default owner, with the immediate
Info -> Actor link intact. Existing nearest-native dispatch still prevents
unknown native descendants from borrowing Info's proof. No original selected
Timer, state, lifespan, physics or callback routing is changed. This permission
does not independently certify native teardown or every future reached service.

## Generated production and actual first-map verification

The independent Info mode emits real native Info/Actor Class metadata and a
cross-package script child. It runs selected Float Tick, Physics0, owner/
deferred LIFO/parity/checkpoint, actual MAIN tween/checked ProcessEvent/
IsPendingKill/selected AnimEnd, selected looping Timer and ordinary/animated/
timer-root late/write/work whole-root undo. Exact threshold, missed-period
coalescing, stored residual, save/reload non-replay and next expiration pass.
Wrong native/script immediate bases still derive from actual Actor but refuse,
as does missing Actor RF_Native; unknown nearer native children remain closed.

The old GameInfo/Mutator/GRI parent-Info cases are now positive canonical Info
update/checkpoint/full-undo controls. DeusExLevelInfo's fourth real Info actor
is placed before late-failing C, proving rollback after that native Info update.
Base ReplicationInfo remains refused. Together this batch adds4,713 checks/
120 refusals net; full production23,460/482 before final schema control exits0.
These generated callbacks do not prove original1114-byte Mission00.Timer has
run through every flag/tutorial branch.

Full desktop, Android native link, strict GNU driver and strict ARM64 runtime
checks pass. CTest217 executed/256 registered/39 explicit owned-data skips/
zero failures105.45s. Separate owned crate415/5, deferred429/21, animation588/
24 and FastTrace252/0 pass. All compiler inputs stayed frozen during every
build/test/probe. Evidence: artifacts/info-final-*.log.

Unfiltered actual cold/warm first Training repeat2 passes Mission000/Info and
reaches JCDentonMale0, class DeusEx.JCDentonMale, nearest native
DeusEx.DeusExPlayer, at239,200 instructions/19,364 writes,144 slots/143 calls,
13 stasis returns/12 deferred entries. Exact-expected-boundary repeats exit0
after verifying this refusal and full undo, not completed startup. Paired doors
retain both56-unit moves/two completion events at239,536/19,432 before the same
next boundary. See ORIGINAL-TRAINING-PLAYER-TICK-GAP.md for next-step proof.

Two actual crowbar Fire calls repeat228,837/26,047,48 rows/two cycles/two
returned notifies/four traces, lower-crate HP10->4->-2/removal and nine births
(six WoodFragments, surviving Lockpick, two impact spawners). Their later
bound ConsoleCommand FExec refusal is unchanged. Whole save/state/revision/
hash/RNG/client/lookup/Actor/GC undo, cold0->1/warm1->1 immutable Model proof
and generation1->2 cold same-filename replacement all pass. Final length/mtime/
SHA256 audit confirms all45 originals and6 frozen inputs unchanged; all jobs
terminal. No original executable code, other map or new APK was used. Live
controller destruction, collectible loot and automatic gravity are unfinished.
