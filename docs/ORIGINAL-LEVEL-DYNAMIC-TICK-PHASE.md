# Original normal Level dynamic-actor phase

First-Training integration,2026-10-25. This joins the original native actor
entry, animation and post-animation services into one production committing
operation. It is not the surrounding complete Level.Tick, original startup
publication, live controller integration or on-headset crate/gravity acceptance.

## Native authority and entry scope

Source evidence is the owned GOTY Engine.dll SHA256
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Static external objdump reads only; no commercial EXE/DLL is loaded/executed.
The entry/body and live-loop bytes were independently rechecked during this
integration, not inferred from a detached helper's behavior. Read alongside:

- ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md: native virtual identities, entry,
  actor tail, live Level loop and pending batches.
- ORIGINAL-LEVEL-TICK-CONTEXT.md: actual Level constructor/default-copy
  baseline and native Spawn's opposite-Level-counter marker.
- ORIGINAL-ACTOR-STASIS.md, ORIGINAL-ACTOR-BLEND-PHASE.md and
  ORIGINAL-NONPAWN-TICK-TAIL.md: the existing joined phases.
- ORIGINAL-LEVEL-CLEANUP-BOUNDARY.md: real suffix compaction and retained
  below128 destruction queue, with explicit missing high-count teardown.

`AdvancePortableLevelDynamicActors(elapsed,tickType)` accepts the caller's
already dilated/clamped Actor delta. It requires the genuine initialized
loader-owned current Level, Model/generation and actual FirstDynamic partition.
It does NOT advance the clock, publish render times or run the player-distance
prepass, infer pause/network eligibility or substitute for Pawn Tick. Those
surrounding Level.Tick contracts still need integration before this operation
can drive a complete gameplay frame.

## One root, original actor sequence

The public entry opens ONE ExecuteTransaction. The native Level driver invokes
internal methods on that same host/Execution; it never loops over the separate
public per-actor APIs, which would create independent roots/publications.

The selected native Tick implementation is authenticated using the nearest
actual RF_Native Class boundary and the audited native vtable/import thunks.
Only Engine.Actor/Decoration/Inventory/Projectile and DeusEx.DeusExDecoration
are currently supported. An unsupported native class/Pawn is a reached whole
root failure, not an actor skipped from the scene.

The admitted AActor entry executes:

1. Actual InStasis. On true, original transient/Role>=2 destruction condition,
   then return1 without updating bTicked.
2. Actual Owner.bTicked against the native Level counter. Unequal pushes
   source into the shared pending head and returns0; no recursive owner Tick.
3. Native source bTicked assignment, preserving other flags. This exact field
   is excluded from generic animation-clock refresh; it is not an animation
   command. There is no source-already-ticked gate in this callee.
4. Original MAIN, then all four BLEND channels with their original shared
   remaining delta/counter, then selected Tick/ProcessState/Timer/LifeSpan/
   Expired/native destruction and fresh physics dispatch.

Source addresses: AActor entry103A1AC8..103A1BAD; ordinary non-Pawn animation
entry103A1BF0; original live Level loop103A5698..103A570E; active Spawn queue
insertion103956B9..103956F3. The new code borrows existing callback, collision,
script, state, RNG, native-work and write journals rather than fabricating
equivalent-looking gameplay effects.

## Live slots, pending arena and final boundary

The ascending dynamic loop reads current count and current slot each time.
It has no bDeleteMe/bTicked/class filter and no frozen initial end bound.
Appending births can therefore be reached in the same pass. Actual active
Spawn also pushes the birth after PostPostBeginPlay and before SpawnNotify,
not a later notification replacement.

Pending nodes preserve LIFO insertion, captured-batch links, an independent
live head and the preceding pass return-sum termination test. Owner cycles
can stop without progress. A pending record whose actor was already ticked
by the ascending loop is skipped by its current marker comparison. New nodes
created while consuming a detached batch belong to the next batch, not the
already captured chain.

The arena is host-owned and each node retains its Actor with a GC root,
including detached batches. Callback entry copies actor/next before arena
growth can move storage. Capacity growth admits the full temporary new buffer
alongside the old retained buffer. Pending buffers/root nodes and the native
context before-image are charged to the same state budget. Scratch roots are
retired before provisional birth identities during rollback; rollback does
not allocate or invoke game scripts.

After scheduling, native parity is logically toggled, inTick is cleared,
scratch is retired, then the real ordinary cleanup boundary runs. Its current
under128 path compacts the dynamic suffix and retains actual deferred objects.
Reached high-count class-reference cleanup/scalar destruction is explicitly
unsupported and rolls back the entire phase. Deferred capture/rebind guards
remain unchanged; a small retained native deletion queue is not save support.

## Checkpoint continuation

Codec13 adds a canonical actual Level owner and parity0/1, plus an explicit
native-actor-order presence marker so parity-only captures are unambiguous.
Codecs1..12 remain byte-compatible. Runtime envelope18 authenticates codec13;
ordinary/mover/quickbar wrappers admit it without repurposing envelope17.

Parity1 is captured; zero may omit the record because it is exactly the proven
fresh/legacy restore baseline. Restore preflights the exact actual Level owner,
then publishes parity with other staged state. Legacy omission resets to0,
not the current session history. Active-frame capture and restoration refuse,
including legacy envelopes that have no script trailer. Scratch queue/inTick
leases are never serialized.

## Verification status

Targeted desktop and repaired Android native builds pass. The generated
production phase passes510 checks/17 refusal controls, including explicit
saved LastRenderTime and native Package/metadata/lookup prerequisites, owner
LIFO/detached batches/cycles, already-marked regular actors, parity1 restore/
legacy reset, inactive clock preservation, suffix holes and shared-root late/
write/work undo. Clock/parity codec passes8737 checks/362 refusals. Original
crate415/5, deferred ownership429/21 and animation588/24 pass separately.
Final full desktop build and shared CTest pass:242 registered,203 executed,
39 explicit owned-data skips,zero failures,124.33s. Strict ARM64 changed-test
syntax checks pass; all build/test processes are terminal.

Separate original two-crowbar-Fire repeat2 retains228837 instructions/26047
writes,48 observation rows,2 cycles/returned notifies,4 actual traces,2 crate
changes and9 births, with full enclosing rollback at the unchanged bound-player
ConsoleCommand FExec refusal. All45 checked original script/config/map files
retain exact SHA256/length/modification time. No APK was packaged.

The real first-Training dynamic observation repeat2 reaches9 slots/8 admitted
actor calls and stops at the closed native-implementation allowlist's
Engine.Mover boundary. Each attempt uses203983 instructions/15281 writes,
retains parity0 and verifies whole-root undo, including client/lookup/Actor/GC
and all41 fixture source files. It does not publish a completed frame.

Further static inspection identifies the NEXT admission step, not a missing
Mover-specific Tick body: Engine export name-table index658 is
`??_7AMover@@6B@`, matching EAT index658/RVA126E08, actual vtable10426E08.
Its Tick+64 at10426E6C is10303DAA and performPhysics+8C at10426E94 is10303242,
the same audited AActor thunks. Compiler inputs stayed frozen during the full
build, so Engine.Mover was not added mid-build. Authenticate and test
that exact boundary next; do not skip movers or call the initial refusal
proof of an actually distinct native Tick implementation. Actual reached
Mover physics/script services can still expose separate missing behavior.

Generated fixtures alone do not
establish original-map startup, active Spawn/physics/GC callbacks, full Pawn/
pause/network Tick or headset gameplay. The opt-in Training startup probe
`--observe-dynamic-actors` attempts this exact joined phase inside its original
always-rolled-back startup root, with no actors filtered. A reached missing
service remains a failure and identifies the next real integration boundary.

Only first Training gameplay is prioritized. No account UI, other-map gameplay
integration or new APK is introduced by this backend phase.
