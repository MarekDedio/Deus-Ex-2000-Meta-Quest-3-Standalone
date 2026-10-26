# Original Actor InStasis binding

First-Training integration,2026-10-25. This implements the original native
InStasis query, not Actor.Tick, a world scheduler, renderer publication or
automatic gravity. It adds no account, login screen or character-creation UI.
The immediate target remains Training's crates, surviving loot and debris.

## Script-call adapter reached by Shadow, 2026-10-26

After exact native Decal admission, the real first-Training Shadow.Tick reaches
its Owner.InStasis call at bytecode PC40. The query below already serves native
world updates, but its script-native721 route was not bound. That is an adapter
gap, not permission to make the shadow static, skip it or force stasis false.

Original Engine.u declares Engine.Actor.InStasis native721/flags401 with no
inputs and one scalar Bool ReturnValue flags580. Independent Engine.dll export
name/EAT1941 (ordinal1942) execInStasis thunk1030372E bytesE9ED9B0600 jumps to
1036D320. The body increments FFrame.Code, calls AActor.InStasis thunk10302013
(EAT1131/ordinal1132, bytesE9C89F0600 ->1036BFE0), writes the returned INT to
the result slot and returns8. There is no argument evaluation or new stasis
algorithm in that exec wrapper.

The script adapter authenticates the actual canonical Function/Class/Bool
metadata, native index/flags and single return packet for numeric and named
call forms, rejects extra inputs or changed result declarations, then returns
the existing native query's Bool. It borrows the same transaction, actor/Level/
Model authority and work ledger; it does not alter the timestamp, distance,
flags, Physics, NetMode or native clock to force an answer. The established
lazy PC64 query below remains the only source of the result.

Final adapter verification: generated production tests cover numeric, named
final-import and direct call forms across eleven true/false boundary cases,
including exact age5/nextafter5, force-stasis, Physics, zone age/distance,
NetMode and NaN LastRenderTime. Results preserve the native Double clock and
LastRenderTime, round-trip checkpoints and use the shared work allowance.
Extra inputs, changed Bool return schema and missing native timestamps refuse;
late native999 and write/work cutoffs undo preceding stores and native state.
These controls add1,487 checks/10 refusals. The final complete driver exits0;
desktop/Android-native/strict GNU+ARM64 and CTest217 executed/39 skips/zero
failures pass. See ORIGINAL-TRAINING-DECAL-TICK-GAP.md for the combined final
audit and exact logs. Actual cold/warm first-Training updates now pass721 and
reach the next native GameReplicationInfo guard, not completed gameplay.

## Original authority and arithmetic

Engine.dll1036BFE0..1036C06C and the exact declaring fields are recorded in
[the prerequisite audit](ORIGINAL-ACTOR-STASIS-PREREQUISITES.md). The helper
retains the original ordered lazy flow:

1. Actor native LastRenderTime Float, then actual native ULevel Double time;
   compare their PC64 difference strictly greater than Double5.
2. Capture bStasis/bForceStasis together. Require bStasis. ForceStasis bypasses
   only the Physics0/5 gate, not either time test or final NetMode test.
3. Use the stored PointRegion.ZoneNumber and native Model zone timestamp;
   reread native Level time for this second age test. If the zone is not older
   than5 seconds, require stored DistanceFromPlayer strictly greater than1200.
4. Follow actual named Actor.Level to its LevelInfo.NetMode byte; only0 is true.
   This is not a substitution of native ULevel, slot0 or viewport camera.

Both age differences stay PC64 until comparison. No eager Float/binary64
subtraction spill is introduced. NaN comparisons are ordered-false, same-sign
infinite subtraction is unordered-false, and other infinities retain the
native comparison result. These explicit classifications do not enable
arbitrary nonfinite persistent state in the VM's separate save policy.
Stored zone numbers64..255 explicitly refuse the unsafe original out-of-range
access; they do not wrap or choose zone0. Only reached providers are required,
and every provider call borrows the enclosing root's native-work allowance.

## Native lifetime and runtime binding

`quest_actor_stasis.h` contains the host-independent original query.
`portable_actor_stasis.inl` binds actual scalar declaring properties/types/
flags/enum constraints, exact stored PointRegion and current loader-owned
ULevel/Model generation/lookup identities in the same existing transaction.
It adds no pending-delete/active/bTicked filter that the original query lacks.

Actor.LastRenderTime reads the genuine existing native optional slot initialized
by original startup/Spawn. A dormant actor without that initialization refuses
instead of pretending its reflected default is the native current value.
Native startup/Spawn's three original -10 assignments now use the authenticated
exact declaring Float reference, retaining their direct-native write counting.
Generic script Let updates the same native optional and its canonical saved
property mirror, without refreshing an unrelated animation clock. Generic
instance reads join that native value; class defaults, immutable birth defaults
and a subclass's distinct same-name field retain their own storage.

The existing property wire format carries the value, including signed zero.
Capture bit-validates the native/mirror agreement. Preflight validates the exact
declared scalar Float and stages the native optional; atomic application resets
the old value and publishes the staged mirror and optional together. A legacy
checkpoint with no LastRenderTime record clears current-session native history;
it does not merge or manufacture a newly rendered timestamp. Existing Touch/
Before rollback already owns both storage views. No save codec field is added.
Actor.Level remains an actual Object field for the final NetMode gate; native
XLevel remains the distinct retained owner of Double time and Model timing.

The actual freshly constructed exact native Model now owns64 transient Float
timestamp slots. All begin at positive zero through independently proven
native class-default zero-extension and fresh template0 CreateExport copying,
not inferred archive contents. See
[the complete initialization evidence](ORIGINAL-MODEL-ZONE-TIMESTAMP-INITIALIZATION.md).
Allocation precedes loader publication; only successful genuine native Model
publication permits query authority. Storage belongs to that Model's GC
lifetime and its256 retained bytes are charged to the root state budget.

There is intentionally no timestamp mutation/render setter in this increment.
The initialized baseline remains immutable, including across portable state
checkpoints on the retained Model. Original renderer publication will require
same-root journal/checkpoint ownership before these timestamps can change;
do not add a reset-to-zero on frame, EmptyModel or save reload.

`ReadPortableActorStasis` exposes the read-only query under one shared root.
The optional startup `observeStasisActors` diagnostic runs at most16 named
actual actors after real startup/partition and before client/player setup.
It detaches only bounded observations and retains the startup probe's whole
rollback. No begun flags, time, distance, physics or render values are injected
to make the gate return a desired answer.

## Verification

The strict standalone GCC helper test passes29390 checks/211 explicit refusals:
all Physics/NetMode/zone bytes, packed flag capture, native-time rereads, exact
lazy order, PC64 threshold witnesses, NaN/infinities, absent reached providers,
nonnull identity and every shared-work cutoff.

The owned lower crate's serialized DistanceFromPlayer is approximately
6376.4893, not zero. The first native Level distance prepass has not run in a
dormant map. The dedicated saved-property fixture therefore preserves that
actual far-distance input first, then explicitly controls zero only for its
separate near-distance case. It never changes production defaults or calls
that fixture a landed/gameplay crate. An actual original Level-clock phase
advances the isolated fixture to six seconds to reach the old-zone NetMode
branch; that explicit clock call is not automatic world scheduling.

Final verification, after every compiler process finished:

- Full desktop build and final affected-test rebuild pass. Strict ARM64 syntax
  passes for the helper and every changed test/probe; the Android native link
  passes. No APK is packaged.
- Shared CTest:240 registered,201 executed,39 explicit owned-data skips,
  zero failures in34.04 seconds. The generated Spawn fixture now declares the
  actual scalar Actor.LastRenderTime required by original native initialization.
- Actual dormant Training animation/uninitialized-stasis/native-work controls
  pass588 checks/24 refusal prefixes. Existing actual crate415/5 and dormant
  Engine collision/config15156/31 regressions pass separately.
- The focused owned `--last-render-only` fixture passes exact timestamp/native
  read agreement, overwrite/reload, signed-zero preservation, readonly
  preflight, atomic wrong kind/index/name refusals, work-limit undo and old
  v4/v3 absence reset. Its far/near/aged-zone paths reach actual stored Region,
  Model timestamps, distance and named Actor.Level.NetMode with exact lazy
  paths; clock advance/reset and full checkpoint/revision invariants pass.
- Genuine startup with four requested stasis observations repeats twice with
  the original220285 instructions/25439 writes. Both crates and Lamp2 remain
  Physics2 and bypass zone timing; Doctor1 bypasses through bStasis. The aimed
  two-Fire regression repeats228837/26047, preserving actual lower-crate
  HP10->4->-2/removal, six WoodFragments and surviving Lockpick birth. Both
  startup modes fully undo at the named ConsoleCommand FExec refusal; neither
  publishes or proves playable startup.
- All45 checked original script/config/map sources retain exact length,
  modification time and SHA256. Commercial DLL/EXE code is never executed.

## Remaining first-Training work

The helper and native binding do not skip these missing integration services:
real renderer actor/
zone timestamps, owner/parity/pending ordering, complete native Actor/Level
Tick, newborn scheduling and deferred cleanup, player weapon/inventory input
and render/save/audio publication. Original complete startup still refuses at
the unimplemented bound-player ConsoleCommand FExec route. This increment
does not establish playable crate destruction, usable surviving loot,
automatic gravity, a replacement APK or headset acceptance. No other map is
integrated or used as a substitute for first-Training gameplay verification.

Following implementation2026-10-25: the original captured-player distance
prepass is now implemented as its own shared-root service in
ORIGINAL-LEVEL-PLAYER-DISTANCE.md. It still needs its real automatic frame
schedule with these remaining services; the diagnostic insertion before
Possess is not that schedule.
