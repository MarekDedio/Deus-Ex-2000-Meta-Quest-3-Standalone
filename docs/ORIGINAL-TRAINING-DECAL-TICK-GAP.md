# First Training: next reached shadow Decal native gate

Read-only next-step evidence, 2026-10-26. No Decal Tick admission is added by
this audit. Only the original first `Maps/00_Training.dx` is integrated/tested;
startup, live collectible loot and automatic gravity remain unfinished.

## Reached after both original game-rules updates

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

## Required next implementation

Admit only canonical native Engine.Decal with its immediate actual Actor
superclass and bounded metadata ancestry. Add the independently proven
+10/+2C permission to the animation callback path separately. Keep unknown
nearer native children rejected. Do not borrow the Info/rules-chain guard.

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
