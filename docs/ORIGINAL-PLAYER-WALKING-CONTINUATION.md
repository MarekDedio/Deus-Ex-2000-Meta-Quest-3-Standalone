# Original player Walking continuation (static audit, 2026-11-03)

Scope is the next genuine player-movement integration for the first
`Maps/00_Training.dx`, not other-map integration. This document implements
nothing and reports no build, test, committed player frame, crate gameplay,
headset validation or new APK. Only external GNU objdump read the owned PE as
data; no commercial EXE/DLL was loaded, executed, linked, patched or rewritten.
The existing compiler inputs remained frozen; this documentation is the only
file added by this audit.

## Source identity

Both hashes were reread after inspection beneath the owned installation at
`D:/Steam/steamapps/common/Deus Ex`:

| File | SHA256 |
| --- | --- |
| `System/Engine.dll` | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| `System/Engine.u` | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Engine.u identifies the existing metadata context; this audit did not repeat
its property linker/schema proof. Addresses and offsets below describe only
the original preferred-image PE32, never portable C++ layouts. Existing Pawn
metadata/outer-dispatch authority is in ORIGINAL-PAWN-PERFORM-PHYSICS.md and
ORIGINAL-PAWN-FALLING-SETUP.md.

## Selected native boundaries

`APawn.performPhysics` selects Walking for Physics1 and passes whole delta
and literal iterations0. Its actual `physWalking` export is ordinal2068,
thunk `10303FD0 ->103CA540`; the normal body ends at `103CBDD3`.

1. At `103CA574`, Walking first calls `103CC450(source,delta,iterations)`.
   A nonzero AL returns before ordinary Walking. Falling also calls this
   helper, so a new Walking dispatcher alone cannot complete local JC physics.
2. That helper walks actual PlayerPawn Class/Super at `103CC4A6..103CC4C9`.
   A genuine nonplayer Pawn returns false at `103CC9E6..103CC9F2`. The current
   falling adapter implements that negative arm, not the positive player arm.
3. The player arm calls `103CCC40`, examines its returned object's Outer/FName
   against UTF-16 `Ladder` at `10481A74`, and invokes a selected ProcessEvent
   through `ENGINE_WalkTexture` at `105835F4`. The ladder arm calls actual
   physFlying at `103CC64E` and returns handled at `103CC6BE`. Its ordinary
   continuation also makes real queries, a WalkTexture callback and conditional
   movement. These are not permission to return false or use a fake floor.
   Exact texture-query and callback-packet protocols still need implementation
   audit; this is not a claim that the complete helper is specified here.
4. After preprocessing/ZoneNumber handling, Walking stores Velocity.Z=+0 at
   `103CA5F0` and Acceleration.Z=+0 at `103CA5F6`. It constructs the acceleration
   direction and calls actual `APawn.calcVelocity` at `103CA674`, using source
   delta/GroundSpeed/zone friction and original literal integer arguments.
   Export `10302180` jumps to `103CD7A0`; no runtime provider exists yet.
5. Walking uses actual Level SingleLine and MoveActor services and calls
   `APawn.stepUp` at `103CB23C`. That export is `103027BB ->103CE4A0`; it is a
   separate missing movement service, not generic moveSmooth.
6. The substep guard at `103CA867..103CA86D` requires iterations below8.
   Fresh Physics selects real continuations: Falling call `103CB32B`, Flying
   call `103CB3E5`, and startSwimming calls including `103CB44E`. Grounded work
   includes real traces, MoveActor and SetSupportBase at `103CB6AE`. The normal
   tail derives velocity from displacement unless the teleport flag is set,
   then stores Velocity.Z=+0 at `103CBDBC`.

This bounded audit establishes those dependencies, not every arithmetic spill,
comparison mask, callback packet or branch in the complete Walking body.
Pinned Surreal `UActor::TickWalking` is not an equivalent retail provider: its
five-iteration generic TryMove algorithm differs from this owned protocol.

## Existing seams and missing implementation

Reuse existing SAME-root services only at the authentic call sites:

- `MoveLevelActor` in `portable_actor_move.inl`.
- `RetailSingleLineLevel` in `portable_retail_single_line.inl`.
- `RetailSetSupportBase` in `portable_actor_find_base.inl`, and `SetBase` in
  `portable_unreal_runtime.cpp`.
- Qualified field validation and native vector/scalar journaling from
  `portable_actor_falling.inl`, with existing caller work/byte/undo ownership.

`portable_pawn_perform_physics.inl` still refuses active Walking through
`otherPhysicsBlock`; `portable_actor_falling.inl` still refuses positive
PlayerPawn preprocessing. Missing genuine work is shared player preprocessing,
calcVelocity, stepUp and the Walking coordinator/adapters. Reached Flying or
Swimming must retain explicit refusals until their own providers exist.

Pawn landing is independently required. `ProcessOriginalDecorationLanded` in
`quest_decoration_landing.h` explicitly refuses Pawn/Carcass branches; the
actual processLanded Pawn path must not be replaced by Decoration handling or
by simply invoking Walking after a synthetic Landed event. Whole-frame startup,
pose/aim, scheduler and publication integration remain separate requirements.

## Reproduce the bounded PE reads

Run only the external inspector, never the inspected DLL's entry points:

```powershell
$walkingDump = 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe'
$walkingEngine = 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
Get-FileHash -Algorithm SHA256 -LiteralPath $walkingEngine
& $walkingDump -d -Mintel --start-address=0x10303fd0 --stop-address=0x10303fd5 $walkingEngine
& $walkingDump -d -Mintel --start-address=0x103ca540 --stop-address=0x103cbe10 $walkingEngine
& $walkingDump -d -Mintel --start-address=0x103cc450 --stop-address=0x103cc9f3 $walkingEngine
& $walkingDump -s --start-address=0x10481a70 --stop-address=0x10481a84 $walkingEngine
& $walkingDump -d -Mintel --start-address=0x10302180 --stop-address=0x10302185 $walkingEngine
& $walkingDump -d -Mintel --start-address=0x103027bb --stop-address=0x103027c0 $walkingEngine
Get-FileHash -Algorithm SHA256 -LiteralPath $walkingEngine
```
