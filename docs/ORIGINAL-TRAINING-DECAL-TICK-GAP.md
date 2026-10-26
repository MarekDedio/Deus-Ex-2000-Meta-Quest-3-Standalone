# First Training: exact inherited Decal native update

Implementation and independent source evidence, 2026-10-26. Only the original
first `Maps/00_Training.dx` is integrated/tested;
startup, live collectible loot and automatic gravity remain unfinished.

## Historical gate after both original game-rules updates

GameInfo and Mutator now pass their exact native gates. Actual cold/warm
Training repeat2 reaches `00_Training.Shadow0`, concrete `DeusEx.Shadow`, nearest
native `Engine.Decal`, at 237,922 instructions /19,290 writes,123 slots /
122 calls,13 stasis returns and12 deferred entries. Both refuse:

`Original Actor Tick reached an unimplemented native virtual override: Engine.Decal`

This conservative message does not itself prove a separate Decal Tick body.
The complete gameplay root still rolls back, and map replacement starts cold.

## Original authority and independent slots

Owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Owned Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
Original files are read as data only; no commercial EXE/DLL is executed,
loaded, linked, patched or rewritten.

The independent bounded package-table/payload reader confirms top-level
Decal Class ref809, immediate superclass ref2 Actor, ObjClass0/ObjOuter0,
RF_Native object flags040F0004, payload offset106694/length134 and matching
payload superclass2. The canonical chain is Decal -> Actor, not Info/rules.

Engine export-name/EAT index642 (ordinal643) is `??_7ADecal@@6B@`, RVA126F74,
preferred-image vtable10426F74. Independent raw .rdata values:

| Slot | Cell | Original inherited Actor implementation |
| --- | --- | --- |
| +10 |10426F84 |10302D74 ProcessEvent |
| +14 |10426F88 |10301F00 ProcessState thunk ->103E5B60 |
| +2C |10426FA0 |10302167 IsPendingKill |
| +64 |10426FD8 |10303DAA Tick thunk ->103A1AA0 |
| +8C |10427000 |10303242 performPhysics thunk ->103C9770 |

Fresh thunk bytes are ProcessState E95B3C0E00, Tick E9F1DC0900 and physics
E929650C00, matching the prior Actor audits. These five slots support only
their exact inherited implementations, not arbitrary native descendants,
scalar teardown, attaching decals or any later unvisited native service.

## Exact implementation and preserved selected script

The native Tick guard now admits only canonical native Engine.Decal with its
immediate actual Actor superclass and the existing bounded metadata ancestry.
The animation callback path independently permits the proven +10/+2C slots
and repeats that same canonical Decal/Actor check. Unknown nearer native
children still refuse. No Info/rules-chain proof is borrowed.

The actual selected DeusEx.Shadow.Tick must still run. The owned bytecode
inspector reports 532 logical bytes, nativeIndex0/flags2/Float DeltaTime and
real super Tick. Preserve its Owner=None Destroy279 path, lazy InStasis/moved
and bHasDecal gate, native723 render-age tests, Detach/Trace/pose/Attach and
all reached dependencies. Do not skip Shadow, force Physics0/static, alter
bHasDecal/defaults or invent successful drawing/trace to avoid missing work.

Existing decal graph/lifecycle/archive services are independent prerequisites,
not proof this live selected update has completed. Add generated actual Class
metadata/selected Float Tick/animation/owner/checkpoint and whole-root late,
write/work undo controls with malformed/nearer-native refusals. Then run the
unfiltered actual first map, paired doors and two actual crowbar attacks,
retaining cold/warm immutable-model and map-generation lifetime checks.

The production generated native-class matrix now includes an independent
Decal mode with real serialized cross-package Decal -> Actor metadata. It
exercises selected Float Tick/Physics0, owner/deferred LIFO, native parity and
checkpoint continuation, actual MAIN tween/checked ProcessEvent/IsPendingKill/
selected AnimEnd, exact callback-native999 refusal and later Tick failure,
all ordinary/animated write cuts and early/middle/final work cuts. Malformed
native and non-native Info superclasses must both refuse the exact Decal
ancestry error, and a nearer unknown native child remains closed. Base Actor
is not incorrectly required to refuse: it already has its own valid admission.
Generated Physics0/tween controls alone do not establish original Shadow redraw,
active decal physics, surviving gameplay or headset rendering.

### Reached original script-native721 adapter gap

Initial actual-map repeat2 passes the former Decal class gate and enters
DeusEx.Shadow.Tick, reaching Owner.InStasis at PC40 with237,936 instructions /
19,290 writes and123 slots /123 calls. Its exact next failure was
`Unsupported runtime native 721`. This is not an unimplemented stasis algorithm:
the existing native world-query implementation was not wired to this script
call form. The same batch therefore adds a fully authenticated native721 Bool
adapter to that existing query. See ORIGINAL-ACTOR-STASIS.md for the actual
Function/return packet and independent original exec wrapper bytes. Original
shadow flags/owner/redraw/trace/pose/Attach behavior remain unchanged.

## Final verification, Decal plus native721

All compiler inputs were frozen for the final builds and tests. Full desktop,
Android native link, strict GNU test syntax and strict ARM64 runtime syntax
pass. The production driver exits0 (15,645 cumulative checks/295 refusals
before its final malformed-schema control); this batch adds3,190/50 across
Decal and native721. Shared CTest executes217 of256 registered tests with39
explicit owned-data skips, zero failures,248.89s. Separately owned crate415/5,
deferred429/21, animation588/24 and FastTrace252/0 pass.

The actual first map now passes Decal and Shadow's native721 call. Cold/warm
repeat2 reaches Engine.GameReplicationInfo on GameReplicationInfo0 at239,200
instructions/19,362 writes,141 slots/140 calls,13 stasis returns/12 deferred
entries. This is an exact new native guard, not completed world startup.
Paired-door repeat2 preserves both56-unit moves/two completions at239,536/
19,430. Two actual crowbar attacks retain228,837/26,047 and48 rows/two cycles/
two returned notifies/four traces, lower-crate HP10->4->-2/removal, six original
WoodFragments, surviving Lockpick and two impact-spawner births. Their later
unchanged refusal is bound PlayerPawn ConsoleCommand FExec.

All modes verify whole save/state/revision/hash/RNG/client/lookup/Actor/GC
rollback, cold0->1/warm1->1 immutable Model proof, and generation1->2
same-filename replacement starting cold. Final audit confirms exact original
length/mtime/SHA256 for45 files and all6 frozen compiler inputs, with no live
build/test/probe process. Evidence: artifacts/decal-stasis-final-*.log.
No original executable code was run or modified; no other map or APK is used.
Live collectible loot, automatic stacked-crate gravity and published gameplay
remain unfinished. Next reached work: ORIGINAL-TRAINING-GRI-TICK-GAP.md.
