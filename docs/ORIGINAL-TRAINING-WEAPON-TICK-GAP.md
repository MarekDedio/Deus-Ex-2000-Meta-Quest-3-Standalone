# First-Training Weapon inherited native Tick/physics admission

Implemented 2026-10-26 from the static evidence below. Only the first
`00_Training.dx` is integrated/tested here. Exact native Weapon admission
does not establish completed world Tick, live crate destruction, collectible
loot, automatic gravity or headset acceptance.

Before this change, after actual Pawn rotation was bound, first-Training execution reached
`00_Training.WeaponCombatKnife0`, concrete `DeusEx.WeaponCombatKnife`, nearest
native `Engine.Weapon`. The closed gate reports `Original Actor Tick reached
an unimplemented native virtual override: Engine.Weapon`. This is the runtime's
generic unsupported-native-class message, not by itself evidence that Weapon
actually overrides Actor.Tick with a different machine body.

## Owned-source authority

GNU objdump reads original PE32 sections/export tables statically; the existing
bounded package-table reader reads owned Engine.u. No commercial EXE/DLL is
executed, loaded as a module, linked, patched or rewritten. Owned source:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Actual Engine.u exports are Actor ref2/base-3, Inventory ref6/base2 and
Weapon ref9/base6, all top-level Class records with flags040F0004. Weapon's
payload is at70981, length2103; Inventory at69379/1385, Actor at61225/1236.
References/lengths are decimal; flags, machine addresses and offsets are hex.
The actual metadata ancestry is Weapon -> Inventory -> Actor, not Pawn.

## Actual original slots, not a presumed override

Export-name index680 is `??_7AWeapon@@6B@`, EAT index680/ordinal681/RVA1287A4,
preferred-image vtable104287A4. Independent raw .rdata words show:

| Slot | Cell | Pointer | Body / meaning |
| --- | --- | --- | --- |
| +14 |104287B8 |10301F00 | original AActor.ProcessState thunk |
| +64 |10428808 |10303DAA | actual exported AActor.Tick thunk ->103A1AA0 |
| +8C |10428830 |10303242 | AActor.performPhysics thunk ->103C9770 |

EAT/name index1667 is actual `?Tick@AActor@@UAEHMW4ELevelTick@@@Z`, RVA3DAA.
The complete thunk bytes at10303DAA are E9F1DC0900, a jump to103A1AA0.
Thunk10303242 bytesE929650C00 jump to103C9770. Thus Weapon's relevant
Tick/physics slots are inherited Actor methods, not a new guessed Weapon
algorithm or permission to call the different Pawn coordinator.

Comparison: Inventory's exported vtable is104286F8 (EAT/name650, ordinal651,
RVA1286F8). Its +64 cell1042875C also contains10303DAA; +8C cell10428784
contains10303242. Before this change the runtime permitted exact nearest-native
Engine.Inventory but did not permit its native child Weapon.
That conservative class gate explained the historical failure even though these
two particular inherited slots are the same. Unknown nearer native children
must continue to refuse; do not replace the bounded check with a broad
all-Inventory-descendants allowance.

## Implemented dispatch and generated controls

`native/portable_actor_tick_tail.inl` now admits only canonical native
Engine.Weapon with its immediate actual native Engine.Inventory -> Engine.Actor
ancestry. The existing bounded metadata walk still selects the nearest native
class; an unknown nearer native Weapon child still refuses. This is inherited
Actor native dispatch, not a fabricated Weapon override or Pawn alias.

The normal stasis/owner/parity/animation and actual selected script Tick,
state, timer, lifespan and physics services remain in place. In particular,
the original `DeusEx.DeusExWeapon.Tick` retains its actual Owner/Pawn casts,
super Tick and lock-state stores; its script is not emptied or bypassed.

Generated production controls check serialized cross-package native ancestry,
actual selected bytecode/Float delta/Physics0, owner-first C,B,A deferred LIFO,
saved native parity/order continuation, the intended late unsupported native999
callback, every successful write prefix, early/middle/late native-work limits,
whole save/revision/parity/GC undo, unknown nearer native classes and missing
or non-native Inventory bases. Weapon controls add 989 checks / 19 refusals
(3,953 cumulative / 67 at their output); the complete driver reports
8,825 cumulative / 163 before its final malformed-schema control and exits0.
Physics0 generated controls do not prove active Weapon physics.

The actual first-map repeat2 now passes the former knife gate and reaches
UNATCOTroop0 after 113 slots / 113 calls, 233,606 instructions / 18,817 writes.
The new failure is the independent Model extent-trace work cap, not another
unimplemented Weapon method or the shared native-work allowance. See
[collision work evidence and next steps](ORIGINAL-TRAINING-COLLISION-WORK-GAP.md).

Verification also passes full desktop rebuild, Android-native link, strict
ARM64 syntax and CTest216 executed/255 registered/39 explicit skips/zero
failures102.63s. Separate owned crate415/5, deferred429/21 and animation588/24
controls pass. Paired-door repeat2 retains both56-unit panel movements and2
completion callbacks before the new collision cap at233,942/18,885. Two-Fire
repeat2 retains228,837/26,047,48 observations/2 cycles/2 notifies/4 traces/2
crate changes/9 births before later FExec; whole undo passes. All45 original
baseline files and4 frozen compiler inputs retain exact size/timestamp/SHA256;
all build/test/probe jobs are terminal. No APK was packaged.

## Preserved requirements

Authenticate the exact actual Engine.Weapon native class/ancestry and its
qualified selected Tick/state/native physics services. Preserve
the real owner/parity/stasis/animation/state/timer/lifespan/physics path and
actual selected weapon script. Do not skip the knife slot, stub its Tick,
force Physics0, empty its script or bypass any later missing dependency.

Required evidence includes generated cross-package native Weapon controls,
owner-first/deferred processing, selected script Tick with actual Float
packet, whole-root late/write/work undo, unknown-nearer-native refusal, then
repeat actual first Training plus original paired-door and crowbar regressions.
Neither the historical generic error nor this admission proves that unvisited
Weapon/native teardown/services are done.
