# First Training: reached local Player input/PlayerTick dependency

Native sequence foundation and independent source evidence,2026-10-26. The
coordinator is now implemented; configured Input/InputExt and playable local
input are not. Only first Maps/00_Training.dx is integrated/tested.

## Historical boundary before this increment

After exact DeusExPlayer native admission, unfiltered cold/warm Training
repeat2 reaches `Original local Player input/PlayerTick continuation is not
implemented` at239,200 instructions/19,364 writes,144 slots/144 calls,
13 stasis returns/12 deferred entries. Door repeat2 retains both56-unit moves/
two completions at239,536/19,432 before the same refusal. Whole-root undo and
cold same-filename replacement pass; no successful startup is published.

The existing NATIVE TICK BOUNDARY row still records the previously observed
UNATCOTroop3/ScriptedPawn entry, not the later throwing local-player service.
Use the exact execution error plus actual dispatch source as evidence; do not
mislabel that retained row as the failing player or completed NPC/world update.

## First independently read native region

Owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
External objdump read103A2524..103A2653 as data. No original DLL/EXE was loaded
or executed. Evidence: artifacts/player-local-input-first-audit-20261026.log;
full surrounding Pawn continuation is in ORIGINAL-PAWN-TICK-CONTINUATION.md.

- The actual captured-Pawn/source casts target APlayerPawn Class1058C160;
  source.Player at+5D4 must be nonnull before this local branch.
- 103A2577..25A2 separately tests actual ACamera Class1057B510 and its word+5FC
  bit0800. A camera with that bit clear returns through103A1AED; it is not a
  generic player success path. Engine export-name/EAT1354,ordinal1355,
  PrivateStaticClass@ACamera RVA27B510 identifies that class.
- 103A25A8..25B4 calls actual Player primary virtual+54 with raw Float delta.
- 103A25B7..25D9 uses exported ENGINE_PlayerInput1058368C and actual
  FindFunctionChecked/ProcessEvent with the same Float packet.
- 103A25DC..25FD repeats checked dispatch for ENGINE_PlayerTick105836B4.
  Export-name/EAT957/958, ordinals958/959, RVAs28368C/2836B4 identify the names.
- 103A2600..260D calls the Player+54 virtual again with raw Float-1.0.
- 103A2610..2641 has a conditional native Level network-copy tail. The native
  standalone-null route joins103A2690. Fields+3DC/+3E0 ->+880/+884 are not
  assigned speculative names by this initial bounded audit.
- The branch joins genuine ProcessState/timer at103A2690, NOT the ordinary
  Tick-probe/tickType1 gate at103A2643. Never call ordinary Tick as a substitute
  for original PlayerInput/PlayerTick or drop the pre/post native input calls.

The +5FC word is proven PlayerPawn.ShowFlags IntProperty (scalar/flags0,
base0/no subtype), NOT bUpdatePosition. Native Pawn size5D4 is followed by
Player(Object4), Password(Str12), six Floats, then ShowFlags at5FC; there is
no intervening Bool packing ambiguity. Original Camera5595 ->PlayerPawn5 is
native040F0004, payload1313258/129, matching superclass link. The helper and
binding use only bit0800 of the actual Int and read it only for a real Camera.

## Implemented coordinator, not a replacement input service

quest_player_local_tick.h preserves camera-only early ReturnOne, fresh Player
reads before BOTH native calls (callbacks may replace/null the pointer), raw
delta pre-input, checked PlayerInput then PlayerTick, raw-1.0 post-input and
the conditional network tail before joining state/timer. It has no ordinary
Tick/type1 gate, no optional reached providers and no post-input scope cleanup
after an exception. Every reached provider uses the enclosing work allowance;
the caller owns transaction rollback, not this coordinator.

The runtime binds genuine Camera Class/ShowFlags metadata, existing checked
native callbacks and actual loader-owned standalone NetDriver absence. Before
native ReadInput it authenticates native Player binding, actual registered
revocable viewport identity and its Player.Actor backedge. It explicitly
refuses the unprepared configured Input service; it does not invoke empty
input providers, silently manufacture an Input instance or clear Player.

The standalone helper passes374 checks/34 refusals: lazy camera masks, exact
provider order/Float packets including signed zero/NaNs, Player replacement/
null rereads, conditional network provider, missing/fault providers, all shared
work cutoffs and identity refusals. Provider test callbacks are observations,
not actual Input/InputExt or script-VM rollback acceptance.

## Required next implementation

Resolve the actual bound Player/viewport+54 implementation and complete owned
Input/InputExt lifetime, metadata/default/config and pre/post input semantics.
The export table contains UViewport.ReadInput (name/EAT1451) and UInput.ReadInput
(1449), but names alone do not prove the complete reached implementation.
Keep the original checked event gates and genuine selected script bodies on
the same transaction with exact native/script work, write and state accounting.
Authenticate the camera flag packet, real local standalone authority and any
reached additional scripts before implementing them. A full source/helper audit
and generated plus actual first-Training regression controls are still required.

Do not clear the bound Player, force nonplayer flags/Physics or substitute an
empty callback to make this branch appear successful. The separate original
HUD KEYNAME/KEYBINDING FExec dependency is documented in
ORIGINAL-PLAYER-INPUT-CONSOLE.md and remains unfinished too. Committing startup,
VR aim/weapon/Frob input, full frame scheduling and publication remain necessary
before live crate destruction, collectible loot, gravity or a new APK can be
called finished. No other maps are needed for this immediate milestone.

## Final verification,2026-10-27

Standalone helper374/34 and full production27,971/584 before final schema
control exit0. Full desktop/Android-native, strict GNU helper and ARM64 runtime
pass. CTest218 executed/257 registered/39 explicit owned-data skips/zero
failures138.01s. Separate owned crate415/5, deferred429/21, animation588/24,
FastTrace252/0 pass. Evidence: artifacts/local-player-final-*.log.

Actual cold/warm Training repeat2 reaches `Original viewport ReadInput configured
Input service is not prepared` at239,200 instructions/19,364 writes,144 slots/
144 calls. Door repeat2 retains both56-unit moves/two completions at239,536/
19,432. Two-Fire repeat2 retains228,837/26,047,48 rows/two cycles/two returned
notifies/four traces, lower-crate removal and nine births before later FExec.
Whole save/state/revision/hash/RNG/client/lookup/Actor/GC undo, Model proof
cold0->1/warm1->1 and generation1->2 cold replacement all pass. The retained
native boundary row remains an earlier NPC observation, not the failing local
input call. This is not successful startup or live player-input acceptance.

Final length/mtime/SHA256 audit confirms all45 originals and9 frozen compiler
inputs unchanged, with every job terminal. No original executable code, other
map or new APK was used. Next is actual Input/InputExt construction and viewport
input authority, detailed in ORIGINAL-TRAINING-CONFIGURED-INPUT-GAP.md; no empty
provider or forced absent Player was introduced to bypass it.
