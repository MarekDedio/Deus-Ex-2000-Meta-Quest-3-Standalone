# First Training: original actor-zone service (2026-10-19)

This checkpoint implements the original ordinary SetActorZone protocol and
real typed runtime providers, exercised through rollback-only diagnostics on
`00_Training.dx`. It is not wired as a fake native266 success path. Native
MoveActor, authentic world/player startup, support/falling and surviving usable
crate contents remain required. Other-map content/integration is deferred.

The complete static control-flow/source audit is ORIGINAL-MOVEMENT-ZONE.md.
The lower original BSP query is ORIGINAL-MODEL-POINT-REGION.md. Original
commercial binaries are inspected statically, never loaded/executed.

## Detached complete ordinary protocol

`native/quest_actor_zone.h` preserves:

- initial deletion-only gate before Level/Pawn/BSP access;
- distinct nonnull/class/final slot0 reads and the LevelInfo-self special case;
- cached Pawn membership and any-nonzero signed test/force flags;
- force reset of main tuple and interleaved Head/Foot component stores;
- count-dependent main fallback, raw by-value main Location and postquery
  sampling of the old main Zone;
- ActorLeaving, ZoneChange, full main publication, live Zone reread and
  ActorEntered, without extra postcallback deletion checks;
- foot height/Z arithmetic and raw Y/X samples after main callbacks;
- FootZoneChange before the new foot tuple, EyeHeight read before its stores,
  then fresh head Location with Float `+0+X/Y` and `EyeHeight+Z` construction;
- HeadZoneChange before full head publication, then fresh LevelInfo.NetMode,
  conditional PRI and current main Region.Zone for PlayerZone;
- full leaf/number stores even for unchanged Zone identity or suppressed events.

Only active providers are required. All provider calls join the supplied work
ledger before the call; recursive entry does not reset it. Undefined null event
receivers, missing active providers and nonfinite/overflowing active Float
arithmetic receive explicit portable refusals. The helper does not manufacture
a separate transaction or claim it can undo provider-owned events/stores:
refusal must unwind the same caller VM root.

## Actual runtime providers

`native/portable_actor_zone.inl` binds current loader-generation Level/Actor
UObject identities without hiding extra slot0 classification in every Actor
identity check. Level.Model lookup is factored out of the Actor convenience
wrapper so that the original supplied Level, not reflected Actor.Level, selects
the immutable Model cache. Existing FastTrace/Region callers retain their
previous Actor-to-Level validation wrapper.

The exact authored PointRegion Struct and its Zone/iLeaf/ZoneNumber members,
main Actor.Region, Pawn.FootRegion/HeadRegion and actual float/object/enum fields
are authenticated from original declaring metadata. Native stored-region reads
explicitly bypass the read-only Region facade while retaining qualified live
overlays, authored tags and class defaults. This prevents a calculated old Zone
from silently suppressing the event required for a genuinely uninitialized tuple.
No reflected Level overlay, render geometry or guessed local player is used.

Each component store uses the enclosing execution's journal/write budget.
Raw-region parent reads preserve the true old tuple for that journal. These
qualified region/PlayerZone writes no longer reconstruct an unrelated native
animation clock. The generic read-only Region facade remains available to its
existing callers; this is not a global change to their view of a dormant world.

Each selected callback validates the actual original base event and real
selected class/state override, then attempts synchronous CallEvent on the SAME
execution. Original ZoneInfo/Pawn events have local variables and flags0x802;
they are not incorrectly treated as Touch's one-variable/flags0x800 schema.
Only their single original Object parameter packet is required for overrides.
Authentic dormant-world eligibility suppresses bytecode, not native entry
attempts. No begun-play, role, pose, possession or water/physics flag is forced.

## Diagnostics and evidence boundary

SetActorZone is an appended operation in the existing bounded retail diagnostic.
Its signed test/force inputs are distinct from unrelated touch/hash flags.
Detached output includes the **stored** tuples and optional canonical full-clock
codec snapshots immediately before/after the operation inside its root. The
clock snapshot's temporary copy/emitter is scratch-admitted; retained bytes
join the same8MiB diagnostic ledger before output allocation. Empty snapshots
mean no actual clock, not an invented idle pose. Such diagnostic binding/output
reads are not attributed to the original native body's own provider sequence.

The owned tests compare tuples against a separate source-BSP/hardware-x87
oracle for actual Training Pawns, crates, slot0 and an assigned-zone actor.
They cover suppressed callbacks, same-zone refresh, repeated force reset,
active original hash preservation, authored old-zone retention, authentic event
entry budget refusal, partial/late write rollback and invalid identities.
A real Doctor1.PlayWaiting command supplies the animation-clock witness; no
clock is injected. Full codec snapshots inside successful zone execution must
equal that real clock, followed by exact outside-root rollback/save/GC checks.

The independent detached test currently passes75635 checks/296 refusals,
867 provider-interpreter cases,128 callback-mutation cases,281 provider/work
failure prefixes, two reentrant controls and60000 independent numeric
comparisons. Its explicit outer-root restoration control is MOCK, not runtime
or begun-world evidence. Actual callback mutations are proven at this detached
protocol layer; the owned world's event bodies remain authentically dormant.

All1308 actual source actors have authored Region tags; none has a null Zone.
The initial owned test exposed and corrected its own contrary assumption.
The actual crates' authored tuple is `(LevelInfo0,3505,1)`. A separate explicitly
labelled dormant posed-input fixture uses the existing raw native pose scope
to present the crate at another real source actor's differently zoned position.
It does not commit Location/Rotation or relocate hash links. A restrictive
entry budget admits ActorLeaving against the **old authored** Zone and refuses
the following ZoneChange before unforced main publication (zero writes).
Using the calculated Region facade as the old Zone would instead perform one
member write before budget refusal, so refusal alone is not counted as proof.
The force-refresh entry control separately admits exactly its3 or9 component
writes and requires refusal at the subsequent actual event entry. VM writes
consume operation entries too; the budget is not misattributed to callbacks.
The fixture forbids a begun world; it is not evidence of live script callbacks
mutating a pose scope. Success/nonfinite refusal both restore the raw pose
before detached output/hash snapshots and the complete enclosing root rollback.

Read-only original schema audit:
`artifacts/training-movement-zone-20261019/zone-schema.cpp` and `.log`.
It uses the port's package reader without initializing a runtime or modifying
source. It confirms main Region flags2, Foot/Head flags0, EyeHeight flags20h,
PRI/PlayerZone flags20h, and original event packets/locals/flags. Source hashes
remain those pinned in ORIGINAL-MOVEMENT-ZONE.md.

## Final verification

- Detached protocol/numeric test:75635 checks/296 refusals,867 independent
  provider cases,128 callback-mutation cases,281 provider/work failure prefixes,
  two reentrant controls and60000 numeric comparisons.
- Actual first Training:26 representatives including all22 Pawns,78 cohort
  operations in seven rollback-only batches (three with an active original
  hash),210 independent source/hardware-x87 tuples and66 Pawn operations,
  plus the separately labelled posed-input and refusal controls.
- Actual Doctor1.PlayWaiting full native clock is byte-identical **inside**
  successful zone execution before outer rollback. A late refusal and final
  preanimation save restoration preserve the complete world/hash/lookup/GC state.
- Final exact38-source-package first-map regression:1090740 checks/77 explicit
  refusals,1308 native-slot actors,120 collidable/nondeleted actors,478 hash
  links and94 actual primitive-provider calls. All source size/mtime/byte
  fingerprints, save/revision/lookup/client/hash/GC/RNG rollback controls pass.
- Startup remains at BeginPlay slot126, `00_Training.DeusExMover16`,
  Engine.Mover.BeginPlay PC166/opcode97, unsupported native266. No advancement
  past that real boundary or begun callback execution is claimed.
- Coherent host build, strict ARM64 runtime/tests and Android native
  compilation/link pass. Final CTest:147 executed/171 registered,24 explicit
  owned-data skips, zero failures in45.82s.

Evidence logs (ignored artifacts, not bundled source/game assets):

- `artifacts/training-zone-unit-final-20261019.log`
- `artifacts/training-zone-owned-verified-20261019.log`
- `artifacts/training-zone-ctest-verified-20261019.log`
- `artifacts/training-zone-host-coherent-20261019.log`
- `artifacts/training-zone-owned-target-recheck-20261019.log`
- `artifacts/training-zone-arm-posed-20261019.log`
- `artifacts/training-zone-arm-owned-final-20261019.log`
- `artifacts/training-zone-android-native-final-20261019.log`

The earlier owned attempts caught two TEST assumptions, not a reason to alter
the game data or omit callbacks: actual source regions are authored/nonempty,
and native journal writes consume VM operation entries too. Both controls were
corrected from original data/VM evidence and rerun with the distinguishing
partial-state/write-count requirements retained.

No APK is packaged for this backend prerequisite. Neither the old world-zone
initialization helper nor generic birth helpers are relabeled as this complete
movement-time operation. Their migration and genuine Move/startup remain
separate required work before live crate/loot/support/falling acceptance.
