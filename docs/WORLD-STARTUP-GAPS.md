# Real world/player startup: current gaps

This is the next integration boundary for a playable original campaign, not
an accepted bootstrap. A [non-publishing world probe](WORLD-STARTUP-PROBE.md)
now coordinates original lifecycle operations in one rollback-safe transaction.
The production Quest loop still does not invoke them as a world simulation.

## Current production path

`native/quest_main.cpp` initializes the package/runtime graph, loads Training,
builds render/audio/UI caches and runs small validation helpers. Its frame loop
moves a camera offset with custom collision and operates separate gameplay
inventory, damage and conversation containers. Revision-based geometry
publication is wired, but real GameInfo startup, player login/possession,
level-wide script phases, animation advancement and actor Tick are not.

The body/hands are render-only meshes, not a possessed JCDenton actor.
`GetPlayerPawn720` currently provides an actual-Level fallback or None; it does
not own a viewport/player session. `ResumePortableActorState`,
`AdvancePortableActorState` and `ExecutePortableActorEvent` have no production
Quest call sites. Passing isolated original actor slices cannot prove a running
campaign.

## Observed original-script dependencies

Read-only inspection of the owned GOTY scripts establishes these fresh-start
dependencies; original executables were not run for this audit:

- The installation's DeusEx.ini selects `DeusEx.DeusExGameInfo` and
  `DeusEx.JCDentonMale`. The original DLL game-class precedence and fresh/restored
  phase contracts are now audited in [WORLD-BOOTSTRAP-CONTRACT.md](WORLD-BOOTSTRAP-CONTRACT.md);
  a diagnostic coordinator now exercises the known prefix without publishing
  a partial world. Physical FindBase and original slot/save ordering remain
  explicit refusal boundaries; automatic accepted startup is not implemented.
- Inherited `Engine.GameInfo.InitGame` starts with Log231 at PC0, after evaluating
  string native168. Logging, original UTF16 option primitives, Min249 and
  validated ObjectToString0x56 are implemented alongside actual Spawn278.
  Optional DynamicLoadObject and SetGameSpeed paths remain unsupported;
  a scoped InitGame control is not automatic world initialization.
- `Engine.Pawn.PreBeginPlay` starts with AddPawn529 at PC0. Add/Remove529/530 now
  operate on native XLevel's actual slot-zero LevelInfo, preserving original
  prepend/first-splice/retained-next behavior. Actual Pawn.Destroyed invokes
  removal; generic Destroy does not force it. Tan189 and FClamp246 are added.
  Complete most-derived PreBeginPlay and live world phase execution remain required.
- Positive CPU AttachDecal and native280 SetTimer now move the unmodified Training
  diagnostic past those earlier boundaries. It reaches PreBeginPlay slot1331,
  `00_Training.Camera4`, then fails in actual `Engine.Pawn.InitPlayerReplicationInfo`
  at PC36/opcode18: `ClassContext requires class-default object identity`, reading
  `Engine.GameInfo.default.DefaultPlayerName`. All prefix state is rolled back.
  Class-default identity/storage must be implemented rather than substituting a
  guessed player name or skipping the original expression.
- [Original actor timers](ACTOR-TIMER-CONTRACT.md) now support native280 and an
  explicit isolated transactional phase, with original once-per-tick coalescing,
  current-state callback dispatch and saved reflected fields. This does not wire
  Timer into the Quest frame loop or provide complete world scheduler ordering.
- `Extension.PlayerPawnExt.Possess` calls its base, then InitRootWindow1052
  at PC6. Omitting the most-derived possession call would omit authored UI
  initialization. Root-window ownership and VR presentation integration remain
  unimplemented.
- ConBindEvents2102 is absent. The existing conversation subset does not
  supply original player/conversation bindings.

## Required shared actor-control correction

Original Core/Engine disassembly distinguishes selected StateNode, running
Node, nullable Code and a live positive ProbeMask. Dormant package masks and
frameless probing are corrected. The [modern representation](STATE-CONTROL-MODEL.md)
now separates these identities, persists the live mask and fixes modern Stop,
queries, eligibility and slice throttling. Production transitions now author
modern frames with original callback flags, pre-argument wrapper capture and
selected-node label search. Spawn initializes its owned class-backed execution
frame after SetOwner, including before begun play. These shared controls do not
themselves start the world.

The shared [original dispatch graph](ORIGINAL-STATE-DISPATCH.md) now preserves
actual Children/Next Auto order and explicit state SuperField callback ancestry.
Inherited Auto paths are not re-resolved to derived same-name overrides. This
corrects lookup metadata, not the remaining frame/control integration below.

Before connecting automatic simulation, complete original world InitExecution
and live-slot actor startup phases, plus the dependencies above. Missing behavior must report
an actual actor/function/PC failure, not become a successful no-op. Detailed
read-only evidence is in the ignored
`artifacts/landing-state-20261010/original-state-identity-audit.md` and its
bounded Core/Engine disassembly logs.

## Real bootstrap and proof

Characterize the original level initialization order, then implement one staged
world bootstrap transaction. It must create/bind the actual GameInfo, execute
InitGame, run separate loaded-actor lifecycle passes over actual live Level
slot order, handle nested births/deletion correctly, establish native bases and
perform real player login plus most-derived possession. Publish a started world
only when required phases succeed; preserve precise phase/receiver/function/PC
diagnostics and complete rollback on failure.

Generated ordering/reentrancy tests are necessary but not sufficient. The owned
unmodified Training map must initialize without checkpoint-seeding begun-play
or selectively skipping callbacks. Evidence must show a real JCDenton identity,
native player ownership, original inventory and GetPlayerPawn, then actual
Quest interaction/rendering. Restored-world and player restoration behavior must
follow their separately established contracts, not silently rerun fresh startup.

Full original Tick/animation/state/timer/physics ordering and authority gates
follow that bootstrap, not a partial loop over map snapshots. Current-map
committed script state still blocks travel even after saving: per-map archives,
real transition/restore lifecycle and campaign progression also remain required.

## Visual work remains separate

The accepted hand-direction build repairs the open cuff/waist geometry and
tracks physically accepted hand direction without changing accepted seated
body placement. It remains render-only geometry, not a possessed player or IK.
See [PLAYER-VR.md](PLAYER-VR.md). The original
Glock weapon grasp has genuinely open, one-sided surfaces exposed by VR angles;
the accepted derived self-body now restores torso surfaces but remains static
and omits original arms/head.
Use bounded, explicitly marked VR geometry adaptation or a more complete
audited original source; preserve original assets and texture appearance. Do
not delete legitimate finger triangles or claim an arbitrary cap is an original
torso. Front/back/top/underside captures and real controller poses are required.

The Persona capture's thin outer rails were compared with the original
InventoryBorder artwork and are authored decoration, not padding corruption;
they must not be cropped away to imitate a simplified menu. UI composition,
original player/root-window lifecycle and actual usable inventory remain
distinct requirements. A styled empty inventory screen is not original player
inventory initialization.
