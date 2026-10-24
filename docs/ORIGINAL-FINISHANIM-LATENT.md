# Original FinishAnim261 and native latent poll385

Read-only owned GOTY audit, 2026-10-24. Immediate integration scope is the
first `00_Training.dx` and its actual knife/crowbar, not other maps. This file
records the next genuine equip/state prerequisite. It adds no implementation,
changes no compiler input, runs no build and proves neither complete Actor.Tick
nor live Quest combat. No commercial EXE/DLL was executed or loaded.

Read alongside [main animation and live melee](TRAINING-LIVE-MELEE-BRIDGE.md),
[Actor Tick ordering](ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md),
[state control](STATE-CONTROL-MODEL.md),
[Sleep](SCRIPT-SLEEP.md) and
[original player pickup](ORIGINAL-TRAINING-LOOT-COLLECTION.md).

## Evidence and actual identities

Fresh SHA256 checks against `D:\Steam\steamapps\common\Deus Ex`:

| Owned file | SHA256 |
| --- | --- |
| `System/Engine.dll` | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| `System/Engine.u` | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| `System/DeusEx.u` | `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480` |
| `System/Core.dll` | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |

Our package inspector reads the actual `Engine.Actor.FinishAnim` declaration:
native index261, FunctionFlags`0x409`, logical script length0, no child fields,
no arguments, OUT aliases or return property. Flags include native, final and
latent. The actual exported execFinishAnim thunk103037BA jumps to103DFF60;
its body ends at103DFFCD. Exported execPollFinishAnim thunk1030240A jumps to
103E00A0; its body ends at103E00B2. Engine's registration at103E00D0 pushes
index`0x181`/385 and the member-function cell10481CEC, whose value is1030240A,
to imported Core.GRegisterNative. Thus385 is the actual native latent poll ID,
not the public FinishAnim command261 or portable enum ordinal3.

Actual Actor field associations corroborate the existing owned field-chain
and native-copy audits:

| Native storage | Declared original field |
| --- | --- |
| Actor+28 bit08 | `Engine.Actor.bAnimFinished`, scalar Bool, flags0 |
| Actor+28 bit10 | `Engine.Actor.bAnimLoop`, scalar Bool, flags0 |
| Actor+54 | `Engine.Actor.AnimSequence`, scalar Name, flags21 |
| Actor+58 | `Engine.Actor.AnimFrame`, scalar Float, flags1 |
| Actor+5C | `Engine.Actor.AnimRate`, scalar Float, flags1 |
| Actor+60 | `Engine.Actor.TweenRate`, scalar Float, flags0 |
| Actor+220 | `Engine.Actor.AnimLast`, scalar Float, flags0 |
| Actor+0C -> +24 | Actual `FStateFrame.LatentAction` UInt32 |

Portable UProperty offsets are not native object-memory offsets. A binding must
authenticate these declaring fields and retain native-frame ownership through
the host's typed representation, rather than interpreting saved UProperty
offsets as portable addresses.

## Exact FinishAnim command order

The retail body is a synchronous native command, not an animation advance:

1. At103DFF60..68, increment the caller FFrame.Code by one byte, consuming the
   zero-argument EndFunctionParms terminator. It performs no Step, evaluates no
   expression and does not touch the unused result pointer.
2. At103DFF6B..74, read the live Actor packed flags. **Only if bAnimLoop is set**,
   it stores the same DWORD with bits08 and10 cleared (`AND AL,E7`). That clears
   both `bAnimFinished` and `bAnimLoop`, preserves `bAnimNotify` and every other
   bit, and occurs even if the later sequence gate returns. If not looping,
   FinishAnim does **not** clear a previously true bAnimFinished.
3. At103DFF77..7C, read the actual AnimSequence FName index. None/index0 returns
   immediately, without reading frame/rates/last or dereferencing StateFrame.
4. At103DFF7E..AF, compare live AnimFrame with Float0 at10426D54:
   - For a finite nonnegative frame, read live AnimRate and compare with
     Double0 at10429838. Exact positive or negative zero returns.
   - For a finite negative frame, read live TweenRate and compare with that
     Double0. Exact positive or negative zero returns.
   - A finite nonzero rate, including a negative value, proceeds. Do not add a
     positive-rate-only requirement or inspect the inactive other rate.
5. At103DFFB1..BF, compare live AnimFrame with live AnimLast. Only strict
   `AnimFrame < AnimLast` reaches the following store. Equality/past-end returns,
   even when bAnimFinished is false or rate is nonzero.
6. At103DFFC1..CA, dereference the actor's actual StateFrame pointer and store
   native UInt32`385` at+24. This is the **only** latent write by the command.
   It overwrites the old action; it does not create a new frame or rewrite
   selected/running nodes, nullable Code, statement position, locals or timer.

Inactive branches retain the old latent slot verbatim. No Actor.Mesh, native
mesh sequence, blend fields, OldAnimRate, role, elapsed time, delete flag,
begun-play flag or existing clock are read. There is no ProcessEvent callback.
A missing/unrepresented actor StateFrame must be refused **only if the final
install branch is reached**; it is not justification to reject a legal inactive
call. A Class/CDO or retired identity is not an actual native Actor receiver.

These are x87 comparisons, not arithmetic with a new rounding policy. The raw
instructions consume C0/C3, including unordered NaN behavior. A portable finite
subset must refuse reached unsupported nonfinite values explicitly and lazily;
it must not claim the retail binary has a finite-only guard or validate unused
rates/last in an earlier returning branch. Signed frame zero follows the
nonnegative branch. No division, square root or animation-time spill belongs
in this command.

The original packed flag store clears both flags together. A portable typed
implementation may need two reflected field journal entries, but no callback
or intermediate publication may observe a half-cleared pair, and any budget or
write-cut failure must undo the complete outer root. Preserve all unrelated
flags and update only corresponding optional clock mirrors; do not call
AuthoredClock/SynchronizeClock and thereby inspect or overwrite blend state.

## Exact poll385 and ProcessState relationship

103E00A0..B2 tests only live `bAnimFinished`. False returns without mutation;
true stores native UInt32`0` in the actual actor StateFrame latent slot.
It ignores its FFrame/result inputs and elapsed time, does not read AnimFrame,
AnimRate, AnimLast or Mesh, does not advance animation, does not clear
bAnimFinished and does not modify the actor-owned latent timer.

The existing original ProcessState audit establishes the caller ordering:
missing StateFrame or null Code returns before role/simulated-state and pending
kill gates or a native latent poll. An eligible executable frame invokes its
current native latent handler once. After that synchronous poll, ProcessState
rereads deletion/frame/Code/latent; only a live executable zero-action frame
runs subsequent statements in the same phase. A FinishAnim newly installed
during those statements yields after its complete enclosing statement. It
does not immediately poll again in the same slice.

The ordinary main animation phase precedes ProcessState in original Tick.
It stores bAnimFinished and invokes selected AnimEnd before the subsequent
poll. That callback can replace the state/frame or start another PlayAnim,
clearing bAnimFinished again. Poll must observe the **fresh** result, not a
captured terminal animation statistic or an old frame. Conversely, a preexisting
true bAnimFinished can release poll385 even when unusual live frame/rate values
would not appear terminal. Do not silently correct those source semantics.

The ordinary loop-ending animation branch103A1E20 tests actual native latent385
before setting bAnimFinished; it must not compare enum3 or a class name. The
new main-animation binding currently preserves loader serializedStack's raw
latentAction, maps only already supported Sleep384/WaitForLanding528, and
refuses ambiguous modern Continue lazily. Native261 should add an authenticated
modern FinishAnim ->385 mapping, not make all modern frames claim native0.

## Current implementation gaps and minimal faithful changes

This audit found the following concrete current-source boundaries:

- `quest_portable_vm.cpp` rejects latent function declarations unless their
  native index is256 or527.261 needs a deliberately admitted exact native409
  zero-input declaration, including raw bytecode and direct Execution routes.
- The runtime's animation-native allowlist omits261. Its current
  `ActorAnimationCommandKind::FinishAnim` pure policy is **not** a binding:
  it always sets `clock.main.finishAnimWaiting=true`, including inactive cases.
  Its release policy consults clock animation/frame, unlike native poll385's
  bAnimFinished-only test. Do not route native261 through that policy.
- `StateLatent::FinishAnim=3` already exists structurally in `quest_state_frame.h`
  and codecs represent it. The VM's pre/post-statement waiting classification,
  host PollState and runtime save-schema validation currently accept only
  Continue/legacy Stop/Sleep/WaitForLanding. Add real FinishAnim support at all
  those runtime boundaries, without requiring `latentTimeLeft` for it.
- The real actor frame must be used on installation. For supported modern
  frames, record the exact modeled FinishAnim state in the existing host
  before-image and actual write accounting; no replacement StateObject, fake
  running program, fabricated Code or allocation by this native is justified.
  Raw stopped/runnable frame conversion needs explicit ownership/provenance;
  a raw UInt32385 must not be interpreted as a portable enum or silently cleared.
- Poll385 must journal only its reached native latent clear and retain the
  current selected/running nodes, Code, PC, locals, live mask and stale timer.
  State transitions/relabels already cancel latent through actual ControlFrame;
  cancellation must continue to win over any previously captured wait.
- `Vm::Execution` currently exposes CallEvent/CallFunction/WriteReference only.
  Public AdvanceState creates another root/Machine. A startup/weapon coordinator
  needs an internal same-Execution state-phase method sharing the existing
  Machine's budgets, journal and first-failure poisoning, or an equivalent
  bounded internal adapter. Never call public AdvanceState/ResumeState from
  inside the startup transaction. Share implementation with the public route
  so role/deletion/poll/selected-change rules cannot drift. Keep a child phase's
  Waiting/Stopped status distinct from overall root acceptance.

Adding support for existing portable enum3 alone does not require changing its
ordinal or old structural wire layout. Runtime admission must nevertheless
validate actual actor/class/state/code/local identities and restore the wait
without inventing a timer. If a further change records exact native latent
provenance, it needs an explicit versioned representation and backward policy:
old modern Continue cannot prove native0 because InitExecution's constructor
did not initialize that native slot. GotoState/GotoLabel and completed native
polls are genuine clearing operations; Sleep/WaitForLanding/FinishAnim install
known384/528/385. Retain unknown separately rather than deriving it from null
Code, frame presence, `finishAnimWaiting`, or a generic Continue ordinal. Raw
serializedStack already contains its actual UInt32 and must remain verbatim.

Tests should include every command return gate, loop/nonloop finished-flag
behavior, signed zeros, equality at last, lazy unused malformed fields, missing
frame only on reached installation, replacing an old latent action, no timer or
mesh reads, both poll outcomes, callback frame replacement and failed-write/root
undo. Save/load and warm replacement must retain FinishAnim/known-native
provenance as admitted, and malformed/legacy-ambiguous cases must not turn into
successful continuation.

## Small first-Training weapon diagnostic after this prerequisite

Use the existing actual begun/nonstartup GameInfo.Login root **before** normal
SetSessionPlayer/Possess, retaining the unchanged later continuation and whole
root rollback. The actual authored actors already observed in first Training
are `00_Training.WeaponCombatKnife0` and `00_Training.WeaponCrowbar1`; resolve their
live identities/classes/states rather than spawn a replacement or grant a new
inventory descriptor. Do not widen the damage-birth-only loot loop into an
unauthenticated arbitrary-item path.

Suggested bounded option: `--exercise-training-weapon ACTOR_PATH`, choosing one
actual authored Training weapon, with a bounded list/count of finite explicit
phase deltas. This is a diagnostic-input proposal, not an implemented option.

1. Capture the real returned JC's inventory, grid, inHand/inHandPending,
   ClientinHandPending, SelectedItem, Weapon/PendingWeapon and both actual player
   and weapon states. Supply the actual target through authenticated member
   FrobTarget and call selected original ParseRightClick, just as the stronger
   crate-loot probe does. Preserve original reach/occlusion limitations of this
   supplied-target diagnostic; do not claim controller testing.
2. Actual weapon Pickup inherits Engine.Inventory.Pickup.Frob, not the lockpick's
   Engine.Pickup.Pickup.Frob. It executes ValidTouch/GameInfo.PickupQuery,
   Engine.Weapon.SpawnCopy, GiveAmmo, GiveTo/BecomeItem, original ownership and
   grid placement. GiveAmmo can create the real AmmoNone Inventory actor; retain
   and observe its identity/list/state instead of skipping dummy ammo. In
   standalone, ShouldRespawn=false keeps the actual weapon identity.
   BecomeItem itself assigns Mesh=PlayerViewMesh and DrawScale=PlayerViewScale,
   so actual knife/crowbar attack meshes arise from original script, not from
   a diagnostic Mesh assignment. Completion still needs an actual run.
3. Call selected original exec PutInHand with that **actually owned** Inventory.
   It preserves RestrictInput/spy-drone/corpse/ammo/decoration guards and calls
   SetInHandPending. Do not call SetInHand directly or write Pawn.Weapon. Then
   explicitly exercise original UpdateInHand as a bounded selected-method
   diagnostic, observing its normal pending transition. A real scheduler caller
   for that method remains separately to be established; this does not claim
   full player Tick.
4. UpdateInHand internally invokes SetInHand, sets SelectedItem and calls
   inherited Engine.PlayerPawn.SwitchWeapon with the actual InventoryGroup.
   Original inventory WeaponChange selects the actual owned weapon; Pawn's
   ChangedWeapon assigns Weapon/PendingWeapon, calls RaiseUp -> selected
   DeusExWeapon.BringUp -> Engine.Weapon.BringUp, SetHand, PlaySelect and
   GotoState('Active'). Retain real old inHand/pending state; do not assume None
   or force away a genuine prior item transition. No root window is needed for
   the legitimate pre-possession None-guarded belt/client branches.
5. Advance the actual weapon's ordinary main animation then its real state
   phase in native order using the same Execution. The selected
   DeusEx.DeusExWeapon.Active program reaches FinishAnim261 at logicalPC211
   (and266 after PlayPostSelect) for a non-ScriptedPawn owner. Implement the
   genuine385 wait/poll to finish Select and let original Finish choose Idle.
   **Engine.Weapon.Active.Fire is an authored empty function**: forcing class
   DeusExWeapon.Fire while still Active would bypass real equip semantics.
6. Once the original state has made the weapon eligible, call the player's
   selected original Fire, not ParseLeftClick: DeusExPlayer.Fire(Float optional,
   flags202) guards RestrictInput then calls Engine.PlayerPawn.Fire. The latter
   uses the actual Pawn.Weapon and state-aware Weapon.Fire. Its hand-to-hand
   path enters NormalFire and PlaySelectiveFiring chooses Attack/Attack2/Attack3.
   Advance the actual selected mesh through its0.5 HandToHandAttack notify.
   Do not force an attack name, apply instant presentation damage as well, or
   skip an overriding state Fire method.
7. HandToHandAttack's real bOwnerWillNotify/owner/network gates, selected
   TraceFire/TraceShot/ProcessTraceHit and original crate TakeDamage must run.
   The main animation phase is only a prerequisite: reached trace, skill,
   augmentation/effects services can reveal further genuine gaps. NormalFire
   also reaches Sleep/GetShotTime and FinishAnim261 atPC405 before original
   ReadyToFire/Finish; repeated eligible attacks need that complete cycle.

Capture returned/captured flags and write ranges for each original action,
authored Mesh and sequence, exact source/selected function identities, latent
action before/after, state code/PC, ownership/grid/ammo births and relevant RNG.
Test a cutoff after completed pickup/equip and inside subsequent wait/notify
effects; later failure must undo **all** earlier phases. If the diagnostic
completes, continue normal SetPlayer/Possess and retain its genuine later
startup boundary rather than publish a partial session. This audit claims no
successful weapon diagnostic, committed pickup, held attack render or Quest
crate destruction.

## Reproduction without commercial code execution

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103DFF60 --stop-address=0x103DFFCE 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103E00A0 --stop-address=0x103E00E9 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -s --start-address=0x10481CEC --stop-address=0x10481CF0 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:\Steam\steamapps\common\Deus Ex' 'Engine.Actor.FinishAnim' 'DeusEx.DeusExPlayer.PutInHand' 'DeusEx.DeusExPlayer.SetInHandPending' 'DeusEx.DeusExPlayer.UpdateInHand' 'Engine.PlayerPawn.SwitchWeapon' 'Engine.Pawn.ChangedWeapon' 'DeusEx.DeusExWeapon.BringUp' 'DeusEx.DeusExWeapon.Active' 'Engine.Weapon.Active.Fire' 'DeusEx.DeusExPlayer.Fire' 'Engine.PlayerPawn.Fire' 'DeusEx.DeusExWeapon.NormalFire'
```

Only this documentation file was written by this audit. Frozen native/core
compiler inputs and current build jobs were left untouched.
