# Real world/player startup: current gaps

This is the next integration boundary for a playable original campaign, not an
implemented bootstrap or an acceptance claim. The landing/probe increment
provides explicit actor-state calls; the production Quest loop still does not
invoke them as a world simulation.

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
  `DeusEx.JCDentonMale`. Map-specific game-class precedence still needs the
  original engine contract.
- Inherited `Engine.GameInfo.InitGame` starts with Log231 at PC0. The portable
  host has no logging handler. Later option/string handling, Mutator creation
  and ObjectToString opcode0x56 also require implementation.
- `Engine.Pawn.PreBeginPlay` starts with AddPawn529 at PC0. Native pawn-list
  ownership/cleanup and required Tan189/FClamp246 are absent.
- `Extension.PlayerPawnExt.Possess` calls its base, then InitRootWindow1052
  at PC6. Omitting the most-derived possession call would omit authored UI
  initialization. Root-window ownership and VR presentation integration remain
  unimplemented.
- ConBindEvents2102 is absent. The existing conversation subset does not
  supply original player/conversation bindings.

## Required shared actor-control correction

Original Core/Engine disassembly distinguishes selected StateNode, running
Node, nullable Code and a live positive ProbeMask. Dormant package masks and
frameless probing are now corrected, but transitioned portable frames still
conflate these identities and retain per-state negative records.

Before connecting automatic simulation, implement independent identities and
live-mask persistence, original callback/preemption order, selected-node label
ancestry, null-Code Stop/miss semantics, selected-state simulated gating and
the original post-statement transition throttle. Missing behavior must report
an actual actor/function/PC failure, not become a successful no-op. Detailed
read-only evidence is in the ignored
`artifacts/landing-state-20261010/original-state-identity-audit.md` and its
bounded Core/Engine disassembly logs.

## Real bootstrap and proof

Characterize the original level initialization order, then implement one staged
world bootstrap transaction. It must create/bind the actual GameInfo, execute
InitGame, run separate loaded-actor lifecycle passes in actual captured Level
slot order, handle nested births/deletion correctly, establish native bases and
perform real player login plus most-derived possession. Publish a started world
only when required phases succeed; preserve precise phase/receiver/function/PC
diagnostics and complete rollback on failure.

Generated ordering/reentrancy tests are necessary but not sufficient. The owned
unmodified Training map must initialize without checkpoint-seeding begun-play
or selectively skipping callbacks. Evidence must show a real JCDenton identity,
native player ownership, original inventory and GetPlayerPawn, then actual
Quest interaction/rendering. Restored worlds must not rerun fresh login.

Full original Tick/animation/state/timer/physics ordering and authority gates
follow that bootstrap, not a partial loop over map snapshots. Current-map
committed script state still blocks travel even after saving: per-map archives,
real transition/restore lifecycle and campaign progression also remain required.

## Visual work remains separate

The earlier landing build's downward capture confirms tracked textured hands
and grounded lower-body surfaces, not a complete VR character. The original
Glock weapon grasp has genuinely open, one-sided surfaces exposed by VR angles;
the partial JC body omits torso surfaces and exposes its open waist from above.
Use bounded, explicitly marked VR geometry adaptation or a more complete
audited original source; preserve original assets and texture appearance. Do
not delete legitimate finger triangles or claim an arbitrary cap is an original
torso. Front/back/top/underside captures and real controller poses are required.

The Persona capture also has edge fragments requiring comparison with original
authored page decoration before treating them as corruption. UI composition,
original player/root-window lifecycle and actual usable inventory remain
distinct requirements. A styled empty inventory screen is not original player
inventory initialization.
