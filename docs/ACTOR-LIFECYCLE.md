# Original actor ownership, attachment and deletion

Native SetOwner272, SetBase298 and Destroy279 now execute through the same
interpreter, callback budgets and transaction as their original caller. This
removes real campaign-startup dependencies; it does not create actors, run
automatic world startup, or make the campaign fully playable yet.

## Pinned behavior

SetOwner sends LostChild to the current owner before removing the first matching
native child-list entry. It assigns Owner, sends GainedChild, then appends the
child. Same-owner calls still perform both halves. Owner is reread after each
callback: a reentrant GainedChild can produce ordered duplicate entries, exactly
as in the pinned engine. Unsafe callback-null/reentrant loops fail with complete
rollback and bounded work instead of hanging or inventing a normalized tree.

SetBase skips equal bases and refuses a prospective base chain reaching Self.
The old non-Level base removes the first matching entry, updates byte
StandingCount, and receives Detach before Base changes. The new non-Level base
appends/counts, then receives Attach; Self receives BaseChange last. A Level
base has neither a native based list nor Attach/Detach callbacks. StandingCount
uses min(list size,255), not a guessed number from reflected Base fields.

Map loading establishes native base lists in the exact serialized ULevel actor
order, including its null holes. Metadata-only generated fixtures explicitly
lacking ULevel use export order. Owner lists and touch-event flags are initially
empty; authored Owner/Touching tags alone do not establish native callback state.

Destroy checks bStatic/bNoDelete before its already-deleted return. It marks
bDeleteMe, detaches its base, dispatches enum Destroyed, removes live peers'
touch links, clears its owner, and detaches its native children and based actors.
Its Level slot is removed last. UnTouch clears a live side's link before its
callback and sends the event only if the native TouchEventSent flag was set.
The deleted side retains its own Touching slots. Enabled probes, begun-play
and the enum-only Destroyed deletion exception follow original event dispatch.

Deletion is not UObject invalidation: in-flight and later direct calls,
properties, state and typed object references remain usable until map teardown.
Both world-snapshot modes exclude removed actors. A script clearing bDeleteMe
does not recreate the removed slot. This is required by original weapon code
which continues with a concrete-class default write after Destroy returns.

## Transactions, saves and publication

Reflected writes, native ordered lists, touch flags, deletion membership,
animation/state/default mutations and callback effects share one rollback
journal. Nested failures publish neither a partial relationship nor a world
revision. Cumulative actor-record/link/value/retained-byte caps apply before
commit, not only when a later save is attempted.

Runtime checkpoint v7 / script codec v4 store native topology separately from
properties. Link identities are schema-validated as actual same-map actors;
ordered duplicates remain intact. Complete allocation/schema preparation
precedes nonallocating replacement. Legacy v1-v6 loads reset native topology
and removal to the authored map baseline; v7 restores exact saved lists and
flags, resetting omitted actors. Restore never replays lifecycle callbacks.
Existing codec-v1/v2/v3 formats remain byte-exact when no native records exist.

The Quest owner polls committed world revisions only while no map worker or
staged transition owns the runtime. It cancels stale pose jobs, refreshes actual
actor snapshots/targeting and actor-lighting inputs, then rebuilds actor GPU
chunks incrementally. Deleted ambient emitters stop; same-map checkpoint
revival reuses map-prepared clips, with no XR-thread audio decoding. Changed or
new uncached sound/pitch combinations are diagnosed as unavailable.
Snapshot or incremental GPU publication failure invalidates the queued revision,
discards partial actor geometry/targets and silences active ambient emitters.
Runtime input is suspended until map recovery, rather than accepting a revision
whose geometry never finished. The map clip catalogue remains available.

## Verification and remaining work

Generated production-metadata tests cover callback order, reentrant duplicates,
failure/budget rollback, base cycles and Level exceptions, protected/repeated
Destroy, UnTouch flags, direct deleted-object access, cleared-delete-flag
persistence, read-only validation, all legacy v1-v6 reset cycles and GC. The
structural codec independently checks malformed booleans/counts/identities,
aggregate budgets, truncation and unchanged legacy bytes. Original-package
callback tests use the read-only game installation, not replacement bytecode.

The portable world still lacks the pin's actor collision hash/BSP-node
membership, full physics and general sound-channel natives. Static BSP lightmap
atlases are not rebaked by actor revisions. Geometry/audio publication is
Android-compiled but is not proof of on-device lifecycle execution or frame
performance. Automatic campaign startup, actual Spawn278 births/frozen instance
defaults and birth-save manifests, AI/timers/latent actions, an authored player
pawn/inventory and campaign travel archives remain unfinished.

Final evidence (2026-10-09): 45 ordinary host passes / three optional skips;
1,243 generated lifecycle controls / 14 rejections; 4,209 structural codec
controls / 3,948 rejections. Three separately invoked original-data integrations
pass. The expanded original defaults test passes 996 controls; a matched
NetMode0 dormant/begun comparison with actual Enable117(Destroyed) increases
67 instructions to 88, exactly the original cleanup callback's 21 instructions.
Natural unmodified map deletion and post-delete CDO continuation also pass.
The enabled/begun fixture is labelled input, not automatic startup or owned
inventory cleanup. Ignored logs are under `artifacts/prerequisites-20261009/`:
`actor-lifecycle-generated-final.log`, `actor-lifecycle-codec-final.log`,
`actor-lifecycle-ctest-final.log`, `actor-lifecycle-original-defaults-final.log`,
`actor-lifecycle-original-actors-final.log`, `actor-lifecycle-original-runtime-final.log`
and `actor-lifecycle-android-final-build.log`.

```powershell
.\artifacts\prerequisites-20261009\build\portable_actor_lifecycle_test.exe
.\artifacts\prerequisites-20261009\build\portable_original_defaults_test.exe 'D:\Steam\steamapps\common\Deus Ex'
```
