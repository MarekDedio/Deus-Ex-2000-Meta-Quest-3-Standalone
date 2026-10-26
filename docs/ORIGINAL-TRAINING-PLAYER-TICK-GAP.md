# First Training: reached original player Tick boundary

Implementation and independent source evidence,2026-10-26. Scope is only
Maps/00_Training.dx. This
is JC's original engine update, not an account/login or character-creation UI.

## Historical reached boundary before this increment

After exact Info admission, cold/warm Training repeat2 reaches actual
00_Training.JCDentonMale0, class DeusEx.JCDentonMale, nearest native
DeusEx.DeusExPlayer. Both stop at239,200 instructions/19,364 writes,
144 slots/143 calls,13 stasis returns/12 deferred entries, with whole-root
undo and cold same-filename replacement. Paired doors retain both56-unit
moves/two ends at239,536/19,432 before that same native boundary.

## Owned original native slots

DeusEx.dll export-name/EAT100, ordinal101, `??_7ADeusExPlayer@@6B@`,
RVA27568 gives vtable10027568:

| Slot/cell | Thunk -> IAT | Actual imported implementation |
| --- | --- | --- |
| +10/10027578 |1001AF98 ->10027268 | Actor.ProcessEvent10302D74 ->10369100 |
| +14/1002757C |1001AF92 ->10027264 | Actor.ProcessState10301F00 ->103E5B60 |
| +2C/10027594 |1001AF7A ->10027254 | Actor.IsPendingKill10302167 ->1030DF80 |
| +64/100275CC |1001AF50 ->10027238 | Actor.Tick10303DAA ->103A1AA0 |
| +8C/100275F4 |1001AF14 ->10027210 | Pawn.performPhysics10302860 ->103C9A70 |

Tick thunk bytesFF2538720210 and physics thunk bytesFF2510720210 resolve
through the actual imports, not similar-looking local function addresses.
Import Hint/Name RVAs in table order are2CFC6,2CFA6,2CEFC,2CDBA,2CC6A;
Engine export-name/EAT IDs1441,1443,1258,1667,2057 respectively.

There is no custom native DeusExPlayer Tick wrapper here. Its +64 inherits
Actor.Tick, but its +8C is APawn.performPhysics, NOT Actor physics. Neither
the ScriptedPawn NPC wrapper nor generic Actor physics may replace these.

## Actual canonical class chain

All export and serialized superclass links agree:

| Class | Ref -> immediate base | Object flags | Payload offset/size |
| --- | --- | --- | --- |
| DeusEx.JCDentonMale |7041 ->240 Human |000F0004 |592711/167 |
| DeusEx.Human |240 ->1 DeusExPlayer |000F0004 |328837/336 |
| DeusEx.DeusExPlayer |1 ->import-42 Extension.PlayerPawnExt |040F0004 |245313/17523 |
| Extension.PlayerPawnExt |70 ->import-24 Engine.PlayerPawn |040F0004 |37480/144 |
| Engine.PlayerPawn |5 ->1 Pawn |040F0004 |62623/6756 |
| Engine.Pawn |1 ->2 Actor |040F0004 |58581/2644 |
| Engine.Actor |2 ->import-3 Core.Object |040F0004 |61225/1236 |

The two script-only JC/Human links are not native overrides. The full five-node
native chain includes Extension.PlayerPawnExt; skipping that class or treating
DeusExPlayer as an immediate Engine.PlayerPawn child is incorrect.

## Exact implementation and remaining services

Authenticate every canonical native node/link/type/native flag/default-owner
condition and keep actual nearest-native selection. Reuse genuine Actor base
Tick only for the exact independently proven DeusExPlayer boundary. Its
ProcessState and APawn physics need their own matching permissions at existing
guards, not a generic native-player or Actor-physics alias. Existing checked
animation callbacks are independent proof, not permission for complete Tick.

The implementation now checks all five canonical native Class nodes and their
immediate links in one dedicated guard, with genuine metadata, RF_Native and
no default-owner receivers. Tick enters the existing Actor base path, without
ScriptedPawn's NPC prefix. The independently proven ProcessState and APawn
performPhysics entries authenticate the same exact player chain at their own
call sites. Animation's previously audited player callbacks now also require
the complete chain. No other native parent/child is admitted by this proof.

Current portable_actor_pawn_tick.inl still explicitly refuses local Player
input/PlayerTick and remote view-copy continuations. PlayerPawn falling
preprocessing also remains a separate reached service. Static vtable inheritance
does not implement any of those branches, and actual live Role/Player/Physics
must determine the next one. Do not clear Player, alter flags/physics or force
an NPC route merely to get past a refusal.

Use generated full-chain/selected-event/owner/animation/timer/checkpoint and
whole-root rollback controls, then unfiltered actual Training, paired doors
and original weapon regressions. Even completing this permission alone would
not publish startup, VR controller input, collectible loot, gravity or an APK.

## Generated production verification

The independent player fixture serializes the full five-node canonical native
chain across Engine.u, Extension.u and DeusEx.u, including actual Class export/
payload superclass links. Shared generic Pawn declarations and observable
selected script bodies are reused, but the player class has no ScriptedPawn
wrapper-only fields. Test bIsPawn/bIsPlayer remain true and Player naturally
None; these are not bound-input success fixtures or changes to owned defaults.

Selected Tick/state/native looping Timer run in order with exact stored
residual/rate/LifeSpan, raw signed Rotation->ViewRotation, stopped-state and
timer checkpoint non-replay, next expiration, MAIN tween/checked AnimEnd and
idle APawn outer/rotation/timer stores. Physics9/roll0 proves that inactive
APawn route only, NOT walking/falling. Each selected Tick/Timer/Expired fault,
every ordinary/animated/outer write cutoff and early/middle/final work cut fully
undo save/state/revision/parity/GC. Genuine simulated/remote-player continuations
still refuse. No fake local input callback is supplied.

Ten independent negative fixtures cover missing native flags on Extension,
PlayerPawn, Pawn and Actor, skipped/rewired intermediate links, unaudited
Extension/PlayerPawn parents and an unknown nearer native player child. The
nonnative Actor fixture puts the player first so the precise player guard is
reached, not Core.Object through an earlier ordinary Actor. The deliberately
rewired PlayerPawn->Actor fixture omits nonexistent Pawn field ownership so
save preflight does not mask the intended ancestry refusal.

The new matrix adds4,511 checks/102 refusals. Full production27,971/584 before
its final malformed-schema control exits0. Full desktop/Android-native/strict
GNU test and strict ARM64 runtime checks pass. CTest217 executed/256 registered/
39 explicit owned-data skips/zero failures72.41s. Separately owned crate415/5,
deferred429/21, animation588/24 and FastTrace252/0 pass. Final source/compile
evidence is in artifacts/player-tick-final-*.log.

## Actual first Training outcome

Cold/warm unfiltered Training repeat2 now passes the native player guard and
reaches `Original local Player input/PlayerTick continuation is not implemented`
at239,200 instructions/19,364 writes,144 slots/144 calls,13 stasis returns/
12 deferred entries. Both original door panels retain56-unit moves/two ends
at239,536/19,432 before that same refusal. The existing NATIVE TICK BOUNDARY
row retains a previous UNATCOTroop3 observation; it is not the later throwing
local-player call. Next: ORIGINAL-TRAINING-LOCAL-PLAYER-TICK-GAP.md.

Two actual crowbar Fire calls retain228,837/26,047,48 rows/two cycles/two
returned notifies/four actual traces, lower-crate HP10->4->-2/removal and nine
births (six WoodFragments, surviving Lockpick and two impact spawners), before
the unchanged later bound ConsoleCommand FExec refusal. Whole-root save/state/
revision/hash/RNG/client/lookup/Actor/GC undo, immutable Model cold0->1/warm1->1
proof and generation1->2 cold same-filename replacement pass. This is still a
nonpublishing diagnostic, not completed player input, startup, live controller
crate destruction, collectible loot, automatic gravity or headset acceptance.
Only first Training is loaded; no original executable code or new APK is run.

Final audit confirms exact length/mtime/SHA256 for all45 checked original
files and all7 frozen compiler inputs, with every build/test/probe terminal.
The source freeze was lifted only after prior jobs ended for test-fixture
repairs, then reestablished before recompilation and final verification.

## Unchanged owned identities

- Engine.u:1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA
- DeusEx.u:257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480
- Extension.u:9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7
- Engine.dll:0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4
- DeusEx.dll:B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04
- Extension.dll:93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C

Only owned-package inspection and external PE disassembly were used. No original
commercial DLL/EXE was loaded or executed and no original file was modified.
