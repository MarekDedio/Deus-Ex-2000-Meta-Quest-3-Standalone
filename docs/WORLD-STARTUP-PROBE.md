# Transactional original-world startup probe

The public `ProbePortableWorldStartup` diagnostic runs recovered LoadMap
operations over the loaded map's actual native ULevel slots. It is intentionally
**non-publishing**: every outcome rolls back. It is not a started campaign,
player login, possessed body, or automatic Quest simulation.

## Coordination and original operations

`Vm::ExecuteTransaction` owns one Machine and one host transaction for the
whole synchronous coordinator. Actual host-resolved function identities and
events share instructions, parsed nodes, writes, retained storage and direct
entry-operation budgets. A failed entry poisons the transaction even if a
callback catches its exception. The first actual function/PC/opcode/stack is
retained. A coordinator-only error or unresolved top-level event failure clears
stale bytecode location rather than blaming the last successful script. Once
an event resolves, preparation failures name that actual declaration; nested
event resolution failures retain their active caller. External OUT closures must own backing
storage through rollback; the InitGame bridge uses shared-owned String storage.

The standalone probe validates native slot0 LevelInfo and NM_Standalone. It
selects a GameInfo using an already-decoded URL GAME class, then actual map
DefaultGameType, then the explicitly configured local-client DefaultGame. It
does not guess a server configuration or a Botpack fallback. Native GameInfo
birth shares transactional allocation but uses LoadMap's null Owner/Instigator,
zero transforms and native Class.Name Tag; it does not use execSpawn's script
post-return tag assignment or mutable reflected XLevel for native ownership.
Before begun play, direct GameInfo birth still performs native zone(1,1) and
LastRenderTime initialization; only script ProcessEvent is suppressed. Its
native Spawned is the audited empty base and its ancestry excludes physical
FindBase. A colliding custom GameInfo explicitly refuses the unaudited native
CheckEncroachment path. General script Spawn's broader before-begun native
completion remains a separate existing gap. Script Spawn now preserves native
Class.Name during callbacks, then writes its captured optional SpawnTag to the
returned identity after native completion, including replacement actors/None.
The original encroachment fast path is specifically null Brush and clear
bCollideActors/bBlockActors/bBlockPlayers (native mask0x0d), not bCollideWorld;
the strict probe checks the recovered three flags after native Brush clearing.
The original conditional native birth queue runs only during ULevel::Tick;
this diagnostic does not enter Tick. Nonnull SpawnNotify explicitly refuses
instead of bypassing its native chain. Strict nested begun births use native
zone(1,1), LastRenderTime and pre-PPP base ordering, not the general adapter's
older water/overlap shortcuts.

For a fresh world the recovered order is touching reset, all-live-actor native
InitExecution, begun/startup publication, real InitGame with OUT Error, separate
live PreBeginPlay and BeginPlay passes, SetActorZone(1,1), native LastRenderTime
reset, PostBeginPlay, SetInitialState, world base initialization, startup clear,
then PostPostBeginPlay. Each pass reacquires the slot/count after callbacks;
nested births can receive both their Spawn callbacks and later outer-pass
callbacks. Nonempty InitGame Error alone does not invent a fatal original branch.

Already-begun worlds restore the staged clock from TimeSeconds, skip fresh
initialization and converge on PostPostBeginPlay. This does not establish
player/save/travel restoration. The staged native clock is never published.

World AttachTag uses the first matching live candidate's Tag, with the outer
actor as SetBase receiver and callbacks enabled as in SetBase(candidate,false).
No match does not fall through to physical FindBase. A real unsupported
FindBase requirement aborts instead of using the existing overlap shortcut.
Nested physical SpawnBase and native Mover::Spawned likewise refuse in this
strict probe until their original contracts have implementations.

## Region correction and rollback boundary

Root BSP traversal now selects one side at exact plane equality, terminates
when that side's child is absent, and uses the same side for leaf and zone.
Empty BSP uses leaf -1; zero serialized zone count forces ZoneNumber0. Fallback
uses actual native slot0, not a scan or a mutable reflected Level. The world
LevelInfo zone special case is {slot0,-1,0}. World zone refresh suppresses all
zone callbacks, skips deleted actors, updates Pawn foot/head/PRI regions and
does not import SpawnZone's water-to-Physics/SetBase behavior.

Writes, execution frames/HasStack, local revisions, actor-owned timer retention,
native LastRenderTime, births/names/slots, relationship topology, collision,
AI state and RNG are covered by the existing host journal. The probe does not
reorder slots or publish a native world clock. After the known prefix it aborts
at physical FindBase if required, otherwise at native slot compaction: the
current save/birth format assumes authored-prefix/appended-birth ordering and
must be redesigned before original static/dynamic ordering can be published.

Phase, slot, receiver, selected GameInfo, copied InitGame Error, phase visit
counts and the first VM failure are returned for diagnostics. Counts are work
observations, not acceptance of those phases. Host native lifecycle work and
touched-object counts are separately bounded; current bounds do not prove all
campaign maps can start.

Primary original DLL evidence, including native frame initialization and the
precise FindBase/SupportActor gap, is in
[WORLD-NATIVE-STARTUP.md](WORLD-NATIVE-STARTUP.md) and
[WORLD-BOOTSTRAP-CONTRACT.md](WORLD-BOOTSTRAP-CONTRACT.md).

## Reproduction

Run `portable_actor_spawn_test` for generated original-layout phase/region
controls and `quest_portable_vm_test` for shared-transaction failure contracts.
Run the optional owned-data test without modifying the installation:

```powershell
artifacts/prerequisites-20261009/build/portable_actor_script_test.exe `
  'D:/Steam/steamapps/common/Deus Ex' --world-startup-only
```

The real-map control does not seed begun play or manually append a GameInfo.
It requires repeatable first failure and byte-identical full saves, original
actor count, revision and GC allocation totals after rollback, plus a short
shared instruction-budget failure. Its output is a diagnostic, never proof
that original Training or a player session completed startup.

The unmodified 1,308-actor Training control currently reaches PreBeginPlay
slot756, `00_Training.JaimeReyes0`, after actual `DeusExGameInfo0` native birth,
all-world frame initialization, InitGame and base Mutator creation. Its first
observed portable failure is actual `Engine.Decal.AttachDecal` at native
declaration offset0, unsupported runtime native0. This is an NPC shadow/decal
dependency, not a completed pass or evidence that later FindBase, compaction,
Login, player UI and scheduling will work. The control pins this evidence so
an implementation change must deliberately update the original contract.

The 2026-10-12 [decal geometry plan](DECAL-GEOMETRY-PLAN.md) adds a separate
read-only Jaime shadow diagnostic to this focused suite, using actual inherited
Shadow defaults, texture USize, BSP surface selection, full multilevel grid and
quad membership. It does not publish a native graph or suppress the original
startup refusal. Graph/GC/dynamic SurfList/save/render integration remains
necessary before this startup probe can legitimately move past AttachDecal.
