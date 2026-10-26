# First-Training Weapon native admission: next reached gate

Read-only next-step evidence, 2026-10-26. Only the first `00_Training.dx`
is integrated/tested here. No Weapon admission/code is changed by this audit.
The current reached error must not be treated as completed world Tick, live
crate destruction, collectible loot, automatic gravity or headset acceptance.

After actual Pawn rotation is bound, repeated first-Training execution reaches
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
contains10303242. Current runtime already permits exact nearest-native
Engine.Inventory but does not automatically permit its native child Weapon.
That conservative class gate explains the current failure even though these
two particular inherited slots are the same. Unknown nearer native children
must continue to refuse; do not replace the bounded check with a broad
all-Inventory-descendants allowance.

## Actionable next step

Authenticate the exact actual Engine.Weapon native class/ancestry and its
qualified selected Tick/state/native physics services, then add the narrowly
proven inherited implementation to normal dynamic actor processing. Preserve
the real owner/parity/stasis/animation/state/timer/lifespan/physics path and
actual selected weapon script. Do not skip the knife slot, stub its Tick,
force Physics0, empty its script or bypass any later missing dependency.

Required evidence includes generated cross-package native Weapon controls,
owner-first/deferred processing, selected script Tick with actual Float
packet, whole-root late/write/work undo, unknown-nearer-native refusal, then
repeat actual first Training plus original paired-door and crowbar regressions.
The current generic error text is not proof that such future admission has
been implemented, nor that unvisited Weapon/native teardown/services are done.
