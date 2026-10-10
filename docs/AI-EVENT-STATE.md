# Native AI event state (2026-10-10)

The runtime now implements manager initialization and event-state mutations,
not automatic NPC AI. The contracts are characterized from the user's original
Engine.dll; the pinned engine's AI stubs are not behavioral implementations.
See [AI-NATIVE-AUDIT.md](AI-NATIVE-AUDIT.md) for primary identities and boundaries.

## Implemented path

- LevelInfo.InitEventManager650 creates one native manager per owner and is
  idempotent, including when the manager has no events. It does not spawn an
  Actor, append an export, or create a Level slot.
- Actor wrappers resolve the actual linked native ULevel, then its Actors[0]
  LevelInfo's manager. They do not route through mutable reflected Level/XLevel
  overlays. A genuinely absent manager returns after argument evaluation.
- Native710 registers or updates callbacks, score names and four perception
  flags while retaining detection/history. Native711 marks receivers deleted;
  re-registration appends a distinct live node. Per-name lists and the circular
  receiver ring retain their original ordering and stable one-based IDs.
- Native713 pulses and native714 persistent emissions preserve sixteen sensory
  history samples. Native715 ends one current channel but keeps its maxima;
  native716 zeros all current channels without removing the sender. Sensory
  type conversion, finite negative values and pending-kill suppression follow
  the characterized native path. Unknown sensory types do not write channels.
- AI-only mutations use a separate first-touch VM journal. A failed enclosing
  callback restores bindings, graphs and native GC edges without publishing a
  geometry revision. Provisional manager allocations retire at API boundaries.

## Persistence and ownership

Native managers are GC objects reached through their LevelInfo owner. They
retain their native Level and all referenced actors, including tombstones and
previous/current best actors. They are not born actors or script properties.

Codec6 / runtime checkpoint9 stores manager presence, identities, counters,
ordered event/node lists, ring links, all flags/detection parameters, current
emissions and all sixteen history slots. An empty manager selects this format.
Removing manager records selects the byte-exact earlier codec/envelope formats.
This is a complete portable graph snapshot, not the original save archive's
cleanup normalization or a compatible original Deus Ex savegame.

Structural decoding uses cumulative node/type/link/string/memory/work budgets
and validates both read passes. Runtime preflight separately checks actual
LevelInfo ownership, the unique serialized ULevel, and actor membership in its
serialized slots or complete saved birth manifest. Nonzero processing depth is
not restorable. Read-only validation allocates no runtime GC objects. Applying
a valid save stages all bindings and edges before publishing; legacy or absent
manager sections clear existing managers instead of merging or replaying Init.

Cold birth restore now recovers the native constructor Level independently of
a reflected XLevel override. A saved reflected None remains None, while native
event routing continues to use the actual Level, just as it did before save.

## Explicit limits

AIProcess, perception/traces, score ordering, callback delivery, history cursor
advancement, manager Tick and native deferred cleanup are not implemented.
Destroy279 does not prematurely invoke the original manager cleanup hook; that
hook belongs to ULevel::CleanupDestroyed immediately before destruction.
Portable snapshots retain tombstones while original manager Serialize cleans
them on save. These differences remain visible limitations, not no-op success.

Automatic authored world/player startup, player possession, general movement
physics, latent/timer scheduling, campaign map archives and headset validation
remain unfinished. The physically accepted seated-player APK stays installed.

The unchanged original UpdateReactionCallbacks and SetDistress functions now
reach their actual Returns (PC484 and PC57). Explicit begun-play BeginState
executes its original movement-physics selection, SetDistress, BlockReactions
and ResetDestLoc effects; the sole Futz registration becomes one tombstone.
This is not automatic world initialization. This increment originally stopped
explicit StartUp at native1010 in Engine.Pawn.PlayTurnHead PC211/opcode0x63.
The subsequent [blend-animation commands](BLEND-ANIMATION.md) implement that
dependency; StartUp now refuses Switch opcode5 in DeusEx.ScriptedPawn.PlayTurnHead
PC52. Provisional registrations, inventory births and world changes still roll
back. Automatic startup and live animation remain unfinished.

## Verification

- Final ordinary host suite: 48 passes, with three optional original-data
  entries skipped until supplied an explicit game root (51 entries total).
  The final separate original actor, runtime-state and class-default integrations
  all pass against the user's read-only GOTY installation.
- AI mutation contract: 188 checks / 51 explicit refusals. Codec controls:
  6,128 / 5,574, including literal codec6 wire fixtures, malformed graphs,
  cumulative budgets and unchanged legacy codec1-5 bytes.
- Generated actual-runtime integration: 3,485 / 77. It covers actual native
  Level routing, inactive Context wrappers with retained direct-call guards,
  two LevelInfo owners, presence/GC, sensory histories, journal rollback,
  malformed symbolic saves and cold births with reflected XLevel=None.
- Final Android ARM64 build and APK signature-v2 verification pass. The APK
  contains 16 ZIP entries, no original commercial game packages and no
  non-ARM64 library directories. It is archived locally and not installed.

Reproduction uses `artifacts/prerequisites-20261009/build` and the CMake targets
`quest_ai_event_state_test`, `quest_script_state_test`,
`portable_actor_spawn_test`, `portable_actor_script_test`,
`portable_runtime_state_test` and `portable_original_defaults_test`. Supply the
original game root to the actor/defaults executables, and `--game-root` plus
that root to the runtime-state executable. Tests without original data remain
commercial-data-free.

Ignored local evidence is under `artifacts/ai-event-state-20261010/`:
`ctest-final-v3.log`, `core-final.log`, `codec-final.log`, `generated-final.log`,
`original-actor-final-v3.log`, `original-runtime-final.log`,
`original-defaults-final.log`, `final-android-build.log` and
`apk-signature-final.log`. Earlier actor logs retain a stale-test-binary failure;
the final-v3 run uses the rebuilt positive real-manager BeginState test and
completes the full suite with exit zero. No runtime behavior was bypassed.
