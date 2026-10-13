# Original lamp interaction and remaining prerequisites

This records the actual GOTY bytecode, not a proposed approximation or a claim
that desk lamps already work in VR. Read from the installed `DeusEx.u` and
`Engine.u` with the port's static `script_bytecode_inspect` tool. Neither the
original executable nor DLLs were launched.

## Authored interaction

`DeusEx.Lamp.Frob` is a 113-byte, nonnative scripted function with actual
`Frobber: Engine.Actor` and `frobWith: Engine.Inventory` parameters. It:

1. Calls the original `DeusExDecoration.Frob` superclass at PC0.
2. Branches on actual `Lamp.bOn` at PC16.
3. When switching on: sets `bOn=True` at PC27, `LightType=LT_Steady` at PC35,
   plays `DeusExSounds.Special.Switch4ClickOn` at PC43, sets `bUnlit=True` at
   PC51 and `ScaleGlow=2.0` at PC59.
4. When switching off: sets `bOn=False` at PC73, `LightType=LT_None` at PC81,
   plays `DeusExSounds.Special.Switch4ClickOff` at PC89, sets `bUnlit=False`
   at PC97 and calls original `ResetScaleGlow` at PC105.

The script does **not** turn down `LightBrightness` or swap lamp textures.
`DeusExDecoration.ResetScaleGlow` restores the original health-dependent
`0.1 + 0.9 * HitPoints / default.HitPoints`, unless the object is invincible.

The superclass calls the original empty `Engine.Actor.Frob`, then, for an
actual DeusExPlayer Frobber, invokes `Player.StartConversation(Self,1)` before
its event/trigger branch. The absence of a real possessed VR DeusExPlayer is
not permission to silently substitute None and skip that branch in gameplay.
An explicit None parameter remains valid for an isolated script unit probe.

## Exact current gaps

- `InteractPortableRuntimeActor` currently treats Engine.Decoration use as
  toggling the port's `activated` flag. That does not run Lamp.Frob or alter
  any of the authored lamp properties; it explains the reported no-op.
- Original `Engine.Actor.PlaySound` is native264, flags0x401, with parameters
  Sound, optional Slot/Volume/bNoOverride/Radius/Pitch and an Int return.
  It currently throws unsupported. Lamp writes before that call must roll
  back, not be partly published and described as successful.
- `GetPortableRuntimeMapActors` projects committed LightType and bUnlit;
  `BuildMapLights` excludes LT_None regardless of an actor's light-class flag.
  ScaleGlow is not currently projected/applied to actor mesh lighting.
- `RebuildRuntimeActorPublication` replaces direct actor-light inputs but
  explicitly leaves the static BSP lightmap unchanged. A lamp toggle must
  also revise the world illumination without rebaking the whole map during
  the headset frame. Updating just the actor light list cannot fix the desk
  and surrounding wall illumination.
- Real player/equipment arguments and original superclass/Conversation
  behavior are still required for the actual VR use route.

## Retail native264 audit, not just the pinned implementation

The installed `Engine.dll` SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Its exported `AActor::execPlaySound` thunk at RVA0x2612 jumps to VA0x103e1f60.
Read-only disassembly establishes these differences from the pin's
`NActor::PlaySound_Deus`:

- Sound evaluates first into a zero-initialized object pointer. Slot defaults
  to byte1 at VA0x103e1fb1. Volume defaults from receiver DWORD+0x1cc at
  VA0x103e1ff0, bNoOverride to zero at VA0x103e203a, Radius from receiver
  DWORD+0x1d0 at VA0x103e207c, Pitch to float1 at VA0x103e20c6. Optional
  expressions then evaluate into those destinations, preserving defaults for
  original Nothing/no-write expressions.
- After consuming the end-parameters token, Sound=None branches directly from
  VA0x103e2117 to the epilogue VA0x103e2618. It **does not write the return
  destination**. The pin's explicit `ReturnValue=0` differs from retail. A
  future native must participate in actual no-write result-sink semantics,
  not fabricate a zero return value.
- For a nonnull Sound, Radius<=0 is replaced by float800 at
  VA0x103e2122..0x103e2132. The constant compared there is float0 at
  VA0x10426d54. The pin leaves this radius rule out.
- The channel/sound ID at VA0x103e22d9..0x103e2302 is
  `(receiver DWORD+0x04 << 4) + (uint8 Slot * 2) + bNoOverride`.
  It is not the pin's low24bits of a native pointer. A portable stable actor
  identity/channel allocator must preserve actual replacement/no-override
  semantics and cannot use an address-derived channel across reloads.
- Before returning, retail performs eligible pawn/viewport routing and calls
  `AActor::CheckHearSound` at VA0x103e2596 and VA0x103e2609. Its packed argument
  vector includes Volume*100, Radius, Pitch*100 (float100 constant at
  VA0x104299a8). This is not only delivery of a PCM clip: skipping the original
  client-sound event path can change script behavior. This is not the separate
  NPC `HearNoise`/MakeNoise contract.
- The normal return ID is written only at VA0x103e2610..0x103e2616, after those
  routing operations. The complete recipient gates still need their own
  implementation and tests before an exact native implementation is claimed.

`CheckHearSound` export RVA0x2856 jumps to VA0x103e1b00. The body selects a
PlayerPawn's actual ViewTarget location when available (actual PlayerPawn
class pointer at VA0x1058c160, receiver field+0x60c), otherwise listener
Location+0x114. It compares squared distance, consults the actual level-model
`FastLineCheck`, then dispatches `ENGINE_ClientHearSound` via ProcessEvent
at VA0x103e1c7a..0x103e1c83. The export table confirms that the FName used at
VA0x1058366c is `ENGINE_ClientHearSound`, not an inferred generic AI event.
Full radius/occlusion constants and recipient/lifecycle behavior must be
retained in a future production implementation.

Original `Engine.Pawn.ClientHearSound` is a **named native with nativeIndex0**,
flags0xd40, with these exact parameters: Actor:Object(Engine.Actor), Id:Int,
S:Object(Engine.Sound), SoundLocation:Vector, Parameters:Vector. Its bytecode
is only five NativeParm records. A correct PlaySound route therefore also
needs this actual named native and renderer/audio-consumer integration;
queuing PCM directly from Lamp.Frob cannot substitute for it.

The pinned `LightSystem::GetVertexLight` also demonstrates why ScaleGlow is
not optional: unlit mesh light is `(ambientColor + ScaleGlow*0.5)*2`, while
lit mesh light is `(ambientColor + accumulatedColor*(ScaleGlow*1.5))*2`.
The Quest direct vertex approximation currently uses white1 for bUnlit and
omits ScaleGlow. Copying only bUnlit would not reproduce the lamp's ON=2.0
glow. This source is a comparison reference, not a claim that the port already
reproduces the full original mesh light model or dynamic BSP lighting.

## Added regression

`portable_lamp_interaction_test GAME_ROOT` reads actual Training Lamp-derived
actors and both original function declarations. It calls the unmodified
original Frob with explicit legal None parameters solely as a scoped probe.
The current expected prerequisite is unsupported PlaySound264 at PC43/PC89;
the test verifies every lamp property, the complete serialized runtime and
world revision are unchanged. If the native later genuinely becomes
available, the same probe verifies the committed bOn/LightType/bUnlit toggle.
It independently checks that detached original light inputs stop emission
with LT_None and confirms original installed package/map bytes are unchanged.

The readonly original Training run passed against the finished host library:
one authored actor `00_Training.Lamp2`, unsupported native264 at
`DeusEx.Lamp.Frob:43`, exact complete rollback, LightType emission projection,
unchanged installed source/map bytes and removal of the two owned temporary
checkpoint files. The isolated probe's explicit None Frobber does not prove
the player-specific superclass route.

This test does not establish a working VR interaction, delivered switch audio,
ScaleGlow rendering, or dynamic BSP lighting. Those remain explicit work.
