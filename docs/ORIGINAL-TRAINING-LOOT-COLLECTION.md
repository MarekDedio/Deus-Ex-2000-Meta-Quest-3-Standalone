# Original first-Training crate loot collection audit

Read-only source audit, 2026-10-23. Scope is the actual surviving `Lockpick`
birth from either destructible crate in `Maps/00_Training.dx`, not other maps.
This document identifies the next original gameplay call chain; it does **not**
claim that collection has run successfully in the portable VM or on Quest.

## Authority and current boundary

The owned original package files were read as data using our
`script_bytecode_inspect.exe`. No original executable or DLL was loaded or
executed. SHA-256 of the inspected files:

| File | SHA-256 |
| --- | --- |
| `System/DeusEx.u` | `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480` |
| `System/Engine.u` | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| `System/Extension.u` | `9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7` |

The existing startup damage diagnostic runs in the real begun Training world,
with its actual initialized `GameInfo` and the genuine `JC` returned by original
`Login`, before `SetSessionPlayer`/`Possess`. The previously captured destruction
prefix leaves six live `WoodFragment` actors and one live `Lockpick` per crate.
The lockpick has `Physics=2`, `Owner=None`, `Base=None`, `RemoteRole=1` and the
actual selected `DeusEx.DeusExPickup.Pickup` state. These are observations of
the damage prefix, not instructions to set those fields in a new fixture.

The new `3970` binding implements the original `Actor.SetPhysics` coordinator
and real support/base providers; collection must exercise that binding rather
than substitute a direct byte store. The later ordinary startup/Possess path
still reaches the known `DamageHUDDisplay.iconInfo[].DamageType[3]` nested
Name-array refusal. The complete startup root rolls back. A collection prefix
must retain that later continuation/refusal and rollback, not publish a partial
player session or force HUD/player/begun flags to manufacture success.

## Actual state-aware pickup entry

The inherited state chain is:

```text
DeusEx.DeusExPickup.Pickup (auto state, flags 0x2)
  -> Engine.Pickup.Pickup (auto state, flags 0x2)
      -> Engine.Inventory.Pickup (auto state, flags 0x2)
```

`DeusExPickup.Pickup` only adds `Landed`; it does not define `Frob`.
For an unmodified live lockpick in that state, normal state-aware `Frob`
selection therefore resolves `Engine.Pickup.Pickup.Frob`, **not**
`Engine.Inventory.Pickup.Frob` and not the no-op `Engine.Actor.Frob`.
This must be checked against the actual selected state at the time of the call.

`Engine.Pickup.Pickup.Frob` is original script, native index `0`, flags `0x2`,
384 logical bytecode bytes. Its required packet is:

| Parameter | Original property | Flags | Dimension |
| --- | --- | --- | --- |
| `Other` | Object constrained to `Engine.Actor` | `0x80` | 1 |
| `frobWith` | Object constrained to `Engine.Inventory` | `0x80` | 1 |

The intended direct-prefix packet is the genuine returned `JC` as `Other`
and actual `None` as `frobWith`, dispatched on the genuine surviving lockpick.
There is no return parameter; successful completion returns Nothing.
Do not require a `FUNC_Event` bit: this original state function has `0x2`, as
does the original base `Actor.Frob` declaration.

The function first calls inherited `Engine.Inventory.Pickup.ValidTouch(Other)`.
Its real gates are:

1. `Other.bIsPawn`.
2. `Pawn(Other).bIsPlayer`.
3. `Pawn(Other).Health > 0`.
4. `Level.Game.PickupQuery(Pawn(Other), Self)`.

Those gates must be read/allowed to execute, never changed for the test.
`ValidTouch` also dispatches tagged original `Trigger` callbacks if the item's
actual `Event` is not None. It has no distance/trace test; a direct Frob prefix
is therefore not evidence of controller targeting, reach or occlusion.

## Original transfer and ownership chain

For a first lockpick accepted by `PickupQuery`, the call chain is:

```text
Engine.Pickup.Pickup.Frob(JC, None)
  -> Engine.Inventory.Pickup.ValidTouch(JC)
      -> actual GameInfo.PickupQuery(JC, lockpick)
  -> Engine.Pickup.SpawnCopy(JC)
      -> Engine.Inventory.SpawnCopy(JC)
          -> actual GameInfo.ShouldRespawn(lockpick)
          -> Copy = Self in standalone when ShouldRespawn is false
          -> Copy.RespawnTime = 0; Copy.bHeldItem = true
          -> Engine.Inventory.GiveTo(JC)
              -> Instigator = JC
              -> Engine.Inventory.BecomeItem()
              -> DeusEx.DeusExPlayer.AddInventory(lockpick)
                  -> Engine.Pawn.AddInventory(lockpick)
              -> GotoState('Idle2')
      -> Copy.Charge = source Charge
  -> original pickup message / PlaySound / PickupFunction
```

Signatures are all original script/native index `0`:

| Function | Flags | Required packet / result |
| --- | --- | --- |
| `Inventory.Pickup.ValidTouch` | `0x2` | `Other:Actor`; Bool return |
| `GameInfo.PickupQuery` | `0x2` | `Other:Pawn, Item:Inventory`; Bool return |
| `Pickup.SpawnCopy` | `0x2` | `Other:Pawn`; Inventory return |
| `Inventory.SpawnCopy` | `0x2` | `Other:Pawn`; Inventory return |
| `GameInfo.ShouldRespawn` | `0x2` | `Other:Actor`; Bool return |
| `Inventory.GiveTo` | `0x2` | `Other:Pawn`; no return |
| `Inventory.BecomeItem` | `0x2` | no arguments/result |
| `DeusExPlayer.AddInventory` | `0x2` | `Item:Inventory`; Bool return |
| `Pawn.AddInventory` | `0x2` | `NewItem:Inventory`; Bool return |
| `Pickup.PickupFunction` | `0x2` | `Other:Pawn`; no return, original empty body |

Required parameters above have flags `0x80`, dimension 1. Return properties
have flags `0x580`, dimension 1. For actual GameInfo virtual calls, resolve the
current original override; the inspected `Engine.GameInfo` base implementations
are not permission to force a base call over a real override.

The inspected base `ShouldRespawn` returns false for `Level.NetMode=0`.
Accordingly the standalone accepted transfer keeps the actual born lockpick
identity; it is not a fabricated inventory descriptor or necessarily a new
SpawnCopy birth. `Pawn.AddInventory` avoids duplicate identical nodes, deletes
the item from a previous Pawn owner if needed, calls `SetOwner(Self)`, sets
the item's `Inventory` link to the old head, then sets `JC.Inventory` to it.

`BecomeItem` genuinely stores the original owner-visible/hidden/carried item
presentation fields and player-view mesh/scale, calls
`SetCollision(False,False,False)` (native `262`), and calls `SetPhysics(0)`
(native `3970`, omitted floor). Original `3970` can call `FindBase` even with
an omitted None floor; it is not equivalent to setting `Physics` alone.
`GotoState('Idle2')` (native `113`) runs the real outgoing Pickup `EndState`,
which stores `bCollideWorld=False` and `bSleepTouch=False`; `Idle2` has a Stop
body. Keep all these effects in the same rollback-capable VM execution.

### Existing same-class inventory and the second crate

Base `GameInfo.PickupQuery` returns true when `Other.Inventory=None`, otherwise
returns `!Other.Inventory.HandlePickupQuery(Item)`.
`DeusEx.DeusExPickup.HandlePickupQuery` (script flags `0x2`) handles an identical
class that permits multiple copies by adding `Item.NumCopies`, checking actual
`maxCopies`, producing the original message, and in standalone destroying the
new world item with native `279`. It calls `UpdateBeltText` when appropriate.

Thus collecting both crate lockpicks is not proved by expecting two linked-list
nodes: the second can be consumed by the original merge branch. Observe the
actual ownership/list, copy counts, return behavior and world deletion. Do not
force `ValidTouch=True`; a false return after a handled merge is legitimate.

## Why pre-pair collection can be a legitimate diagnostic

`DeusExPlayer.AddInventory` calls the base linked-list operation before looking
for `RootWindow`. Its belt/window additions are guarded by a genuine non-None
root. `DeusExPickup.UpdateBeltText` similarly checks root, HUD and belt.
`Extension.PlayerPawnExt.ClientMessage` falls through to
`Engine.PlayerPawn.ClientMessage` if the root does not handle it; the latter
immediately returns when the actual `Player=None`. No fake root is needed.

`Frob` still executes original `PlaySound` native `264`. The portable
`ClientHearSoundNative` matches the original legitimate None/non-Viewport
early-return branch for the unpaired JC. It explicitly refuses a genuine
bound Viewport route because commit-only original audio publication is not yet
implemented. This is a concrete remaining **paired** native gap, not a reason
to suppress sound calls or pretend the pre-pair test proves audible pickup.

The inspected portable `Member`/`ResolveEvent` paths resolve from the actual
selected state and inherited functions, without requiring the Frob function
to carry `FUNC_Event`. The relevant `SetCollision`, `SetPhysics`, `SetOwner`,
state transition and PlaySound handlers are present in current source. This
audit has not found a proven additional missing native on the first direct
pre-pair Frob path. Only actually running the original call in the existing
root can establish its next reached refusal or completion.

## Full player control, inventory slots and VR limits

The original player action entry is `DeusEx.DeusExPlayer.ParseRightClick`,
script native `0`, flags `0x2c2`, no parameters/result. It reads the player's
**member** `FrobTarget`; a function-local target argument is not a substitute.
Its opening `RestrictInput` checks actual states Interpolating, Dying and
Paralyzed using native `281`.

For an Inventory target it calls
`HandleItemPickup(FrobTarget, True)` (script flags `0x2`; required Actor target,
optional Bool `bSearchOnly` flags `0x90`, Bool return). That searches the actual
grid when needed, then calls `DoFrob(Self,inHand)`. `DoFrob` (flags `0xc2`,
required Actor `Frobber`, Inventory `frobWith`) reads the member `FrobTarget`,
detaches its genuine based actors through iterator native `306` and `SetBase`
native `298`, dispatches target `Frob`, conditionally sends original `Futz`
native `713` for an actually owned target, plays the genuine `Pickup` or
`PushButton` animation via native `259`, and bases the owned inventory on the
Frobber. After successful standalone ownership, `ParseRightClick` calls
`FindInventorySlot` again to place it and clears its member target.

`FindInventorySlot` is script flags `0xc2`, required Inventory `anItem`, optional
Bool `bSearchOnly` (`0x90`), Bool return. Its standalone path calls original
`IsEmptyItemSlot`, `PlaceItemInSlot` and `SetInvSlots`, using the existing
fixed Byte `DeusExPlayer.invSlots` grid. This is not the unrelated HUD nested
Name-array schema. Do not skip placement and claim complete inventory behavior.

Current VR `UseTargetedActor` still calls the presentation-layer
`InteractPortableRuntimeActor`. Its Inventory branch sets `active=False` and
appends the path to `persistentInventory`; it does not dispatch original Frob,
transfer ownership, merge copies, run grid placement or exercise the state
chain above. That is a concrete live integration gap. A direct same-root Frob
proof should precede replacing this path with actual selected-player input and
commit-only world/HUD/inventory publication. It does not by itself certify
controller reach/occlusion, paired audio, save persistence, render publication
or the still-unimplemented original falling/settling phase.

## Next bounded proof, without changing game rules

Extend the optional existing startup diagnostic, not the production API with
a fake grant. Select the lockpick by its actual just-born identity/class and
live state; authenticate the declaration and packet above. Before/after the
real state-aware call, capture selected function/state, original eligibility
fields, owner/base/physics, `JC.Inventory` chain and same-class `NumCopies`,
deletion state, collision/held flags and root write prefix. Keep observation
storage bounded and admitted for the entire outer startup root. Test a second
actual crate birth for merging, plus budget/refusal rollback and unchanged
source/save/actor identities. Do not assume any outcome until this runs.

The next stronger proof must supply a real selected member target and invoke
original `ParseRightClick`, observing slot placement and animation/base
effects; it must not silently certify those from bare Frob success. Continue
the unchanged ordinary pairing/Possess path after the diagnostic and verify
complete rollback at its real later refusal until startup is implemented.

Inspection can be reproduced with our built executable, passing the owned game
root and the fully qualified function/state names listed in this document.
The audit made no runtime/compiler-input changes and ran no commercial code.
