# First Training original crate-loot collection checkpoint

2026-10-23. Actual original-script pickup/ownership/merge diagnostic inside the
real initialized first Training startup root. This is NOT published gameplay,
controller targeting, inventory-grid placement or headset acceptance. Only
Maps/00_Training.dx is loaded; other actual-map integration remains deferred.

## Original caller and implementation

The optional collectDamageLoot probe follows genuine damage callbacks with
Frob(JC,None) on their actual surviving Inventory births. It does not spawn
replacement loot, grant a descriptor, force begun/Active flags, pair a partial
player session or suppress normal later Possess failure. The real Login-returned
JC, GameInfo and begun/nonstartup world are used before SetPlayer/Possess.
ORIGINAL-TRAINING-LOOT-COLLECTION.md records original declaration/call authority.

portable_startup_loot.inl authenticates actual Core.Function and two original
Core.ObjectProperty Actor/Inventory inputs, including base Actor.Frob and the
selected inherited state implementation. Overrides need not retain FUNC_Event:
actual Engine.Pickup.Pickup.Frob has flags2. Snapshots read actual eligibility,
state, Owner/Base/Instigator, Inventory links, optional copy/capacity fields,
collision and carried/held flags. Other Inventory types are not assumed to have
every Pickup field. Real support/SetPhysics services run in the same VM root.

Commands require actual damage, one Login viewport, at most eight live Inventory
birth attempts and128 linked nodes per snapshot. Repeated/cyclic identities
refuse. Detached output retains a fixed1MiB allowance for the whole root,
lexical scratch64KiB, plus existing shared VM/write/work limits. Only complete
observations have captured=true; a thrown callback records its actual reached
write prefix and poisons the entire caller root.

## Observed transfer and merge

Actual JC initially owns NanoKeyRing1 only. Both original crate deaths leave
six WoodFragments and one live Lockpick each. Before pickup these Lockpicks
have NumCopies1/maxCopies20, Physics2/RemoteRole1, selected
DeusEx.DeusExPickup.Pickup, no Owner/Base, and world collision. The diagnostic
reads these values; it does not assign them.

| Attempt | Actual source | Original result | Shared writes |
| --- | --- | --- | --- |
| 0 | Crate1 birth Lockpick0 | Same actor becomes JC's Inventory head, linked to unchanged NanoKeyRing1 | 15068 ->15100 |
| 1 | Crate2 birth Lockpick1 | Lockpick0 NumCopies1 ->2; Lockpick1 genuinely deleted, no duplicate Inventory node | 15100 ->15128 |

Transferred Lockpick0 has Owner/Instigator=actual JC, Physics0/RemoteRole2,
Engine.Inventory.Idle2, bHeldItem/bCarriedItem/bHidden/bOnlyOwnerSee true;
collision/block/sleep-touch fields false. Other Inventory fields/order remain
unchanged. This is original GiveTo/BecomeItem/AddInventory/state behavior,
not merely hiding a presentation object.

Both normal collection repeats return/capture two Frob rows, one transfer and
one merge; exact detached damage/loot/write-prefix observations match. Unchanged
normal SetPlayer/Possess then reaches the genuine DamageHUDDisplay.InitWindow
PC39 opcode1 refusal: Unsupported authored struct array/string member
DeusEx.DamageHUDDisplay.sIconInfo.DamageType. Both have222885 instructions/
24560 writes; native work507410cold/506783warm of1048576, not exhausted.
Exit2 means observed unsupported startup, NOT accepted startup. Full save/
script/state/revision/hash/RNG/client/lookup/Actor/GC rollback and all41 unchanged
owned-source fingerprints pass.

## Interrupted merge

With shared write allowance15110, the first pickup completes at15100; second
Frob fails at Engine.Inventory.Destroyed PC65 opcode11. It reports returned=false,
captured=false; only the first pickup is complete. Both repeats stop at207333
instructions/15110 writes; native work478467cold/477843warm of1048576. Entire-root
rollback restores both crate deaths, births, first transfer and partial second
merge/deletion; all41 sources remain unchanged. The incomplete callback is not
counted as a successful merge.

## Reproduction and evidence

No original commercial EXE/DLL is executed or loaded. Owned packages are data
for our VM/inspectors. The production38-script-package list plus DeusEx.ini,
User.ini and first Training map are fingerprinted read-only.

```powershell
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:/Steam/steamapps/common/Deus Ex' --collect-training-loot --repeat 2 --native-work 1048576
& artifacts/prerequisites-20261009/build/portable_training_startup_probe.exe --owned-source-root 'D:/Steam/steamapps/common/Deus Ex' --collect-training-loot --damage-write-limit 15110 --repeat 2 --native-work 1048576
```

Evidence in artifacts/: training-loot-discovery-20261023.log,
training-loot-repeat-20261023.log, training-loot-late-20261023.log,
training-loot-build-20261023.log and training-loot-android-20261023.log.
Strict ARM64 syntax and Android native compile/link pass. These are library
checks, not a newly packaged/installed APK or headset pickup test.

The coherent full desktop rebuild passes, including a final no-change build
confirmation. Shared CTest passes174 executed/206 registered with32 explicit
owned-data skips, zero failures in95.91s. Separate isolated original crate
destruction regression passes347 checks/two genuine contents refusals. Normal
startup without damage/collection retains no diagnostic observations, reaches
the same genuine HUD boundary (218253 instructions/24192 writes/native491158),
and fully rolls back/all41 sources unchanged. Final logs:
training-loot-build-final-20261023.log, training-loot-full-tests-20261023.log,
training-loot-crate-regression-20261023.log and
training-loot-normal-startup-20261023.log.

## Still required for actual play

Normal Quest input still uses presentation-layer pickup bookkeeping rather
than original player targeting/ParseRightClick/grid placement. The pre-pair
sound None gate is legitimate, but real paired Viewport ClientHearSound
publication remains unimplemented. Falling/settling, support loss after crate
destruction and loot falling remain unfinished. Save continuation/render
publication of these diagnostic changes is NOT claimed: the entire startup
root is undone at its real later gap.

The next stronger input proof needs genuine selected FrobTarget and original
ParseRightClick, including slots/animations/Base; bare Frob success does not
prove that contract. Swept falling/selected Landed must use real native motion/
support, not hardcoded coordinates or floor snaps. Unrelated HUD completion
and other actual maps are not prerequisites for these Training diagnostics.
