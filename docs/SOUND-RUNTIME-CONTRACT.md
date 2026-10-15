# Sound prerequisites for original crate destruction

This is a source-level checkpoint, not a claim of working crate destruction
or a new Quest build. The accepted NPC APK remains the installed test build.

## Why sound is part of crate damage

The two actual Training crates use `DeusExDecoration.Active.TakeDamage`, not
the port's Pawn-only health helper. An isolated original-script hit of6 takes
HitPoints from10 to4 and sets ScaleGlow to0.46. A second hit currently rolls
back at PlaySound264, PC464. That probe passes `EventInstigator=None`.
Player-instigated fatal damage reaches MakeNoise512 at PC428 first; sound264
is therefore not the only live gameplay prerequisite.

The remaining chain includes AISendEvent713, original fragment Spawn/lifecycle,
GetMeshTexture1013, Destroy/Containers.Destroyed/FastTrace548, dropped Contents,
and falling/landing/Base support. The renderer also needs to publish the whole
committed change, not merely hide the struck crate. Nothing in this checkpoint
bypasses those dependencies or substitutes Pawn health for decoration HitPoints.

## Read-only retail evidence

Original `Engine.dll` SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
The DLL/executable are never executed. Function metadata comes from the owned
original packages; native behavior comes from static disassembly.

- `Engine.Actor.PlaySound`: native264, flags0x401, six inputs and an Int return.
  Export thunk RVA0x2612 targets VA0x103e1f60.
- Arguments evaluate in the caller's `FFrame.Object`, not the source Actor.
  The source is the opcode receiver. Defaults initialize immediately before
  each input: Sound=None, Slot=1, current TransientSoundVolume, bNoOverride=False,
  current TransientSoundRadius, Pitch=1. Earlier input side effects can change
  later defaults; later input side effects cannot change an already consumed
  default or input.
- Sound=None evaluates all inputs and branches from VA0x103e2117 directly to
  epilogue0x103e2618. The result destination is not written. A declaration's
  Int return does not authorize manufacturing zero.
- Nonnull Sound normalizes Radius<=0 to800. Wrapped DWORD channel arithmetic
  is `(source.UObjectIndex<<4)+(uint8(Slot)*2)+bNoOverride`. The normal result
  store at VA0x103e2610..0x103e2616 occurs after recipient routing, including
  genuine zero-recipient cases. An address or package-local export number
  is not an authoritative UObject index.
- Packed Parameters is `{Volume*100, Radius, Pitch*100}`. The squared-radius
  fallback tests Radius==0 and uses1600, not Volume==0 or6400. For finite
  inputs that branch is unreachable after the preceding nonpositive rule.

Local viewport routing applies when NetMode==3 or the actual calling node is
a UFunction with FUNC_Simulated0x100. `FFrame.Node` at VA0x103e237b is not the
called PlaySound declaration, nor a Pawn/bNetOwner/bSimulatedPawn predicate.
Otherwise routing scans the live LevelInfo.PawnList/nextPawn for bIsPlayer.
Viewport routing uses ordered registered viewports, nonnull Viewport.Actor,
and matching source/listener native Level. Recorder/network state is a separate
native dependency and must not silently be assumed absent.

`CheckHearSound` (RVA0x2856, VA0x103e1b00) uses a PlayerPawn's nonnull ViewTarget
location, otherwise listener Location. Initial distance squared is an unrounded
x87 geometric sum, also stored to Float for the later check. The initial
threshold is stored Float `radiusSquared * bit_cast<float>(0x3f44ec4f)` and the
comparison is strict. Blocked BSP FastLineCheck multiplies the secondary
threshold by Float0.6 unless source.Owner==listener; this product remains
unrounded until comparison against the stored Float distance. The secondary
comparison permits equality. A blocked path multiplies packed Radius by0.35
even for the owner; it does not attenuate packed Volume instead.

Accepted listeners receive the original `ENGINE_ClientHearSound` event
synchronously inside the same VM transaction. Only external audio delivery
publishes after successful commit. `Engine.Pawn.ClientHearSound` is native0,
flags0xd40, with Actor, Id, S, SoundLocation, Parameters. Its audio route requires
an actual PlayerPawn/Viewport binding and available engine audio, not bIsPlayer
alone. Pending-kill Actor becomes None; zero packed Radius becomes1600; packed
Volume/Pitch decode with the audited Double0.01. Package access/PCM decoding
must not happen inside a real-time native callback or XR frame.

## Implemented foundation and explicit limits

- The VM has a narrow per-slot opt-in to materialize initialized native omissions
  at their own evaluation boundary. Native264 opts in; other natives retain
  their previous Nothing markers. Captured input values are detached immediately.
- Native result disposition survives raw/reflected calls. NoWrite/ZeroFill do
  not read an unwritten return alias; a scripted initialized return retains
  its separate zero-value behavior.
- A backward-compatible call-site hook exposes the real script/state/native
  caller before input evaluation. Direct top-level entries report no caller.
  It does not infer caller flags from the callee.
- Runtime264 validates the actual original declaration and sequential defaults.
  Sound=None completes without writing a result. Nonnull playback still refuses
  with `Unsupported runtime native 264` and rolls back the entire script.
- `quest_sound_protocol.h` implements detached scalar packet preparation and
  hearing comparisons only. Its split-distance overload accepts the unrounded
  initial distance and stored Float separately; the Float convenience overload
  proves no original geometric/x87 equivalence. Portable nonfinite/overflow
  refusals are explicit safety policy, not invented retail clamps.

Still required: owned stable UObject indices, engine recorder/network authority,
actual recipient selection/event delivery, a bounded commit-only external sound
queue and asynchronous/prepared audio consumer. These are not implemented by a
pure scalar helper, and audible native effects/crates remain unfinished.

## Player-instigated MakeNoise512 follow-on audit

Fatal decoration damage assigns Instigator=EventInstigator at PC406, guards
None at PC417, and calls MakeNoise(1.0) at PC428. The wrapper (VA0x103bcd20)
evaluates its required Float Loudness into an initialized0.0 destination before
the actual native Level/slot0 NetMode==3 early return. Native MakeNoise
(VA0x103bfad0) legitimately returns for Instigator=None; a real player hit does
not take that branch.

The two Pawn noise caches use native ULevel Double TimeSeconds, not reflected
LevelInfo Float TimeSeconds. A recent cache (time>now-Double0.2), nearby spot
(unrounded squared distance<2500), and old loudness>=Double0.9*L suppress noise.
Otherwise stale cache1, then stale cache2 (time<now-Double0.18), has precedence.
The close-cache1/loudness test follows. The final cache2-loudness fallback
actually writes **cache1**, unlike the pin. Cache writes store Float time,
source location and Loudness. No eligible cache write does not itself suppress
recipient delivery.

Routing uses the captured instigator's bIsPlayer or Enemy.bIsPlayer branch,
actual live PawnList/nextPawn, the HearNoise probe before CanHear, and synchronous
HearNoise callbacks in the current transaction. It rereads nextPawn after each
callback. Player-aware routing excludes actual PlayerPawn class instances;
nonplayer routing requires compatible source/listener UClass ancestry. Current
source.Instigator is excluded separately.

CanHear has a general squared-distance limit4,000,000*L^2, not the pin's16m
for both branches. The same-team player branch uses16,000,000*L^2, driven by
actual Game.bTeamGame and both PlayerReplicationInfo.Team values. General
coincident distance returns early. Otherwise Stimulus is written before the
HearingThreshold/BSP refusal; same-team routing skips that Stimulus write.
Actual Model.FastLineCheck is required. All caches, Stimulus and synchronous
callback effects must join the enclosing crate rollback, including a later
sound/fragment/Destroy failure. Native Double clock ownership/ticking/saving
and this handler remain unimplemented; no arbitrary NPC or reflected clock
is substituted for a real player/native level.

## Verified checkpoint

- Coherent full host build succeeds. Final CTest has94 passing tests out of108
  registered,14 explicit original-data skips and no failures (23.32s).
- `quest_sound_protocol_test`:1,859,556 checks,43 portable safety refusals.
- `quest_portable_vm_test`:695,208 checks,784 refusal controls, including454
  native result-destination checks,217 omission checks and43 caller checks.
- Separately invoked `portable_lamp_interaction_test GAME_ROOT` passes actual
  None calls with1..6 inputs, illegal Actor-as-Sound refusal, complete saved
  state/revision invariance and the unchanged nonnull Frob264 rollback.
- Separately invoked `portable_based_actors_test GAME_ROOT` passes152 checks.
  Both crates retain the damage6 -> HP4 result; fatal None-instigator input
  rolls back at264 PC464. A legal actual Pawn instigator (Jaime, solely an
  isolated test argument) proves the earlier512 PC428 branch and identical
  complete saved state/revision rollback. This does not create a VR player.
- Strict NDK ARM64 `-Wall -Wextra -Werror -fsyntax-only` passes for the changed
  runtime, VM and scalar sound tests. No new APK or audible native playback
  is claimed.

Ignored local evidence logs are `artifacts/crate-sound-foundation-full-build-resumed-20261015.log`,
`crate-sound-foundation-ctest-final-20261015.log`, `crate-sound-owned-lamp-final-20261015.log`,
`crate-sound-owned-crates-final-20261015.log`, `crate-sound-vm-20261015.log`,
`crate-sound-protocol-20261015.log` and `crate-sound-ndk-syntax-20261015.log`.

## Next BSP dependency audit

Original FastTrace548 has TraceEnd:Vector, optional TraceStart:Vector and a
Bool return, flags0x401. Its thunk RVA0x2103 targets VA0x103e35e0. TraceStart
initializes from the receiver's current Location **after** evaluating TraceEnd
(VA0x103e3607..0x103e3627); expressions still run in FFrame.Object. It calls the
actual native XLevel.Model.FastLineCheck, not actor/mover ray tracing. The pin's
TraceAnyHit wrapper is not authority for this retail BSP path.

Model.FastLineCheck RVA0x1a37 targets VA0x103f3280; recursion0x103f33a0 reads
only native BSP planes/children/NodeFlags bit0 and RootOutside. Plane distances
are stored Float, classified by the **signed bits** (`test`/`setge`), not an
epsilon or ordinary nonnegative comparison. Splits visit the end-side segment
first; middle coordinates are Float stores from the retained extended fraction.
This is distinct from the existing flags6 Model.LineCheck helper (mask0x27,
NumVertices and epsilon tests). It must not be replaced by that helper when
implementing Containers.Destroyed, sound occlusion or CanHear. This paragraph
records a read-only audit; FastTrace548/FastLineCheck are not implemented here.
