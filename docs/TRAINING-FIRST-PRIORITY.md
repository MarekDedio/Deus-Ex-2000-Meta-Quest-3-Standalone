# Immediate milestone: complete the first Training map

User priority,2026-10-18: make the first original Training map fully work
before spending time on other campaign-map integration. The long-term full
campaign objective remains, but it is not the current acceptance milestone.
Reconfirmed2026-10-19: skip other-map chest/physics integration and testing to
save time; shared prerequisites needed by this first map remain in scope.

## In scope now

- Only the actual `Maps/00_Training.dx` level as the playable content target.
  Complete its authored tutorial route, not just the opening corridor or an
  isolated recording scene. Enumerate required interactions from its original
  map/scripts rather than assuming that a few successful rooms finish it.
- Next: live held-weapon crate damage, actual destruction/contents callbacks,
  collectible usable loot, and gravity/support/falling/landing of the crate
  stack and debris. Preserve the accepted held models, NPC poses, music,
  speech, comfortable HUD, quickbar and controller behavior.
- Original world/player startup, shared native services and support packages
  only as needed for Training's real behavior. A required shared dependency
  is not another campaign-map integration. Do not replace missing startup with
  forced begun-play/Active flags, invented loot or cosmetic falling.
- Verify the entire level from a fresh start: its required items/equipment,
  doors/locks, NPCs/speech, movement/collision and scripted progression. Check
  save/reload, repeatable interactions and failure/rollback behavior without
  breaking the accepted UI and body/hands.
- Build a meaningful playable APK when ready; automatically replace-install
  it if the authorized Quest is connected, preserving data. Verify rendered
  frames and frame timing on the actual Training route before calling it ready
  for the user's demo recording.

## Deferred

Other campaign levels, per-map integration/polish, a campaign-wide readiness
matrix, broad map-content sweeps and inter-campaign-map travel acceptance are
deferred until this Training milestone is working. Keep inexpensive shared-code
regressions where they protect Training, saves or already accepted features.
Synthetic map lifecycle fixtures are regression tests, not extra campaign work.

Production startup now retains its first `00_Training` load instead of loading
`00_TrainingCombat`, `00_TrainingFinal` and `00_Training` again as a map-replacement
self-test. First-map initialization and subsequent mesh, interaction, save,
audio and UI preparation remain. The Android native module/link passes; shared
map-transition, mover and stair regressions pass. No replacement APK or measured
headset startup-time improvement is claimed for this source-only checkpoint.

## Current evidence is not a playable-physics claim

The actual Engine-owned Cylinder prerequisite now passes both the focused
fixture and the Quest bootstrap's exact shared-package set, including a real
Engine-before-map/Client/Viewport sequence. All227 null Brush/Mesh actors in
first Training select that same constructed Primitive;31 are collidable. This
is shared work required by this map, not integration of other map content.
See TRAINING-ENGINE-COLLISION.md for the memory fixes and verification boundary.

Original crate damage/fragment scripts and actual UObject lookup now pass
isolated Training diagnostics, including six fragment identities, save
replacement, GC and rollback. The world is deliberately dormant in those tests.
They do not prove live VR attacks, surviving contents, gravity or completion of
Training. Those remain the immediate development target.

The actual native collision-registration prefix has rollback-only tests on the
same first Training map:1308 actual actors,120 collidable/nondeleted actors and
478 links, including real crate primitive packets and save/GC/RNG rollback.
Original SetCollision/SetCollisionSize now update this same native hash; real
lookup-storage and native Touching-reset reductions pass the earlier InitGame
memory refusal. Separately bounding fixed table initialization avoids charging
its49152 draws as actor traversals; the65536 actor-work limit is unchanged.
Strict startup now passes PreBeginPlay and next reaches missing native Move266
in `00_Training.DeusExMover16` BeginPlay. The actual current-Level world/actor
multi-line query prerequisite is connected through rollback-only diagnostics.
This is not publishing physics:
original Move, support/falling, destruction and surviving usable contents remain
unfinished. See TRAINING-RETAIL-COLLISION.md. No other-map content integration
or replacement APK is included in this prerequisite checkpoint.

The next overlap prerequisite now has the original cylinder PointCheck kernel
and actual Primitive/Mesh/LodMesh owner-field adapter. Its independent numeric
and integer tests pass; it is not a line/zero-length sweep substitute. Actual
Model.PointCheck now has all zero/box/owner/null hull branches and staged packet
behavior, independently checked against a separate hardware-x87 occupancy
oracle. Its actual selected-Brush/ABrush.ToWorld adapter passes clear/overlap
controls on three nonempty source Models in this first map, including the
DeusExMover16 at the startup boundary. The subsequent hash encroachment service
now supplies temporarily authoritative source pose, candidate cylinders,
tag-before-gate ordering and tail-linked hits, followed by exact scoped pose
restoration without collision Remove/Add, native hash Location or ColLocation
updates. Its kernel and scoped raw-word/refusal tests pass. This is not
completed higher-level encroachment, native Move or playable physics. Full Level
encroachment, Move, support/falling and surviving usable contents remain the
same immediate gameplay target. See ORIGINAL-ACTOR-POINT-CHECK.md,
ORIGINAL-MODEL-POINT-CHECK.md and ORIGINAL-ACTOR-ENCROACHMENT-CHECK.md.

The next shared prerequisite supplies the complete original lazy IsBlockedBy,
receiver-local GetPlayerPawn and directional IsBasedOn helpers plus actual
typed first-map runtime providers. It does not use native720's world scan or
add a generic collide/self/Owner gate. The higher Level encroachment audit is
now complete, including push/recheck/veto side effects and original touch-event
ordering; those higher movement/touch dependencies are still unfinished.
Predicate tests and static audits are not crate gameplay acceptance. Other-map
integration and test sweeps remain deferred. See
ORIGINAL-ACTOR-BLOCKING-PREDICATES.md and ORIGINAL-LEVEL-ENCROACHMENT.md.
The separate moveSmooth audit pins its complete movement/event/store order
without implementing an alternative slide or claiming its remaining numeric
dependencies are solved; see ORIGINAL-ACTOR-MOVE-SMOOTH.md. The predicate
checkpoint's actual test loads only first Training:1308 local-player controls,
81 directional pairs and90 Base/self/None cases, with no invented possession
or Base links. The whole owned first-map suite passes883599/55; original
startup still reaches missing Move266. No replacement APK is produced for
these backend-only prerequisites.

The subsequent shared dependency now has the complete detached TwoWallAdjust
numeric adapter and moveSmooth call protocol, including exact aliases,
retained-vs-stored slide math, same-hit reuse, current native pose/Level reads
and event ordering. These are not direct native Move replacements or proof
of physical falling. Their original numeric/protocol tests run without any
additional real map; the actual current-map regression stays on first Training.
The remaining genuine movement/startup/gravity/loot acceptance is unchanged.
Verification passes both new detached helper suites, strict ARM64 runtime/
test syntax, Android native link and coherent CTest141 executed/165 registered
(24 explicit owned-data skips, zero failures). Separate actual first-map
collision883599/55, Engine42647/31 and isolated crate347/two regressions pass.
The unchanged startup native266 boundary still prevents a playable-physics
claim; no replacement APK is produced or installed for this helper checkpoint.

The next dependency implements the original four-slot BeginTouch/EndTouch
protocol and strict signed-cylinder IsOverlapping, with actual current-Level
identity/qualified-property/event providers. They remain rollback-only entries;
no original startup flag, role or possession is forced. Independent touch
interpreter and hardware-x87 overlap tests pass; the Android native link,
strict ARM64 syntax and coherent CTest143 executed/167 registered also pass
(24 explicit owned-data skips, no failures). This work still targets only
first Training's movement/destruction prerequisites. It does not integrate or
test chest physics on the other campaign maps. Original native Move, support/
falling and surviving usable loot remain unfinished. See
ORIGINAL-ACTOR-TOUCH.md and ORIGINAL-ACTOR-OVERLAP.md.
The final owned first-map collision regression passes951463/68, including a
real Doctor1.PlayWaiting animation-clock rollback control. Its cold-world
success/refusal tests are not an in-root clock or begun-callback witness.
Startup still reaches unsupported Move266, and no new APK is produced.

The next shared checkpoint supplies the COMPLETE detached higher Level
encroachment protocol and source-backed MoveActor numeric prequery/backoff.
Their independent protocol/hardware-x87 tests pass; strict ARM64 runtime/tests,
host compilation, Android native link and coherent CTest145 executed/169
registered pass (24 explicit owned-data skips, no failures). These are the
remaining real movement dependency's parts, not a fake publishing provider:
native266/299, movement-time zone/carry/callback integration and genuine
startup remain required before live crate/loot/support/falling acceptance.
Other-map content/integration/tests remain deferred. No new APK for this
backend-only checkpoint. See ORIGINAL-LEVEL-ENCROACHMENT-PROTOCOL.md,
ORIGINAL-ACTOR-MOVE-GEOMETRY.md and ORIGINAL-LEVEL-MOVE-ACTOR.md.
The rebuilt exact38-source first-map regression passes951463 checks/68 explicit
refusals with complete rollback/source immutability. The startup boundary stays
at DeusExMover16/Engine.Mover.BeginPlay PC166, unsupported native266; the new
detached components are not falsely reported as executed through that call.

The next shared correction replaces Region's approximate Float BSP lookup with
the original retained-PC64 PointRegion query and the current loader-owned Model
cache. Its independent numeric/path suite passes201293 checks/35 refusals,
including52106 hardware-x87 comparisons,12000 multilevel paths and explicit
Float/Double cancellation witnesses. Host/strict ARM64/Android native builds
and coherent CTest146 executed/170 registered pass (24 owned-data skips,
zero failures). Actual map coverage remains ONLY first Training; no other-map
integration or APK is added. Complete movement-time SetActorZone, native Move,
startup, support/falling and surviving usable contents still remain. See
ORIGINAL-MODEL-POINT-REGION.md.
The final rebuilt38-package first-map suite passes1043599 checks/68 refusals;
all1308 actors' regions agree on explicit cold and warm/GC passes with43442
independent source/hardware-x87 node visits. Full rollback/source immutability
pass and the startup native266 boundary is unchanged. Final coherent CTest
completes146 executed/170 registered with24 owned-data skips and zero failures.
The full movement-time zone event/store audit is in ORIGINAL-MOVEMENT-ZONE.md;
it is not an implemented provider or playable-physics claim.

The subsequent ordinary actor-zone service now has its complete kernel and
real typed runtime providers, tested only through rollback-only first-map
diagnostics. All22 Training Pawns are included among26 representatives with
78 native operations and210 independently predicted main/foot/head tuples.
Temporary posed-input controls distinguish the old authored region from the
calculated facade and restore the scope before the next command in the SAME
root. No pose is committed, no actor is relocated in its hash, and the fixture
is forbidden in a begun world. This is not native Move, original startup or
live crate/gravity/usable-loot acceptance. Other maps remain deferred and no
APK is packaged for these prerequisites. See TRAINING-ACTOR-ZONES.md.
The final actual38-package first-map regression passes1090740 checks/77
refusals. Doctor1.PlayWaiting's real native clock is byte-identical INSIDE the
successful zone root, with late refusal/full state/save/GC rollback separately
verified. Coherent CTest147 executed/171 registered passes with24 owned-data
skips and no failures; strict ARM64 and Android native link pass. Original
startup still reaches the same unsupported Move266 boundary; these results
do not complete live crate destruction/loot/gravity or produce a new APK.

The next shared dependency implements the complete detached based-child carry
segment, including live list changes, lazy per-child source pose reads,
quantize-before-subtract yaw, exact coordinate/Float stores, fresh child hit
packets, ignored ordinary child results and retained postcall Pawn.ViewRotation.
Its independent suite passes140257 checks/115 refusals; recursive Move is
explicitly MOCK, not a fake transform setter or native266 publishing shortcut.
Actual recursive movement/root/hash integration, genuine startup, support/
falling and usable loot remain the immediate unfinished target. No campaign
map beyond first Training is integrated, and no APK is made for this helper.
See TRAINING-CHILD-CARRY.md. Coherent host, strict ARM64 and Android native
builds pass; CTest148 executed/172 registered passes with24 owned-data skips
and no failures. Those skips are not claimed as actual-map or headset evidence.
The separate rebuilt actual38-package first-map regression passes1090740
checks/77 explicit refusals with complete rollback/source immutability.
Startup still reaches the same DeusExMover16 Move266 boundary. It does not
exercise the detached child-carry helper through a real recursive move provider.

The complete ordinary detached MoveActor coordinator now supplies the original
outer protocol, verified by independent provider/hardware tests and an actual
child-helper composition suite. The native pose-read seam now resolves
temporary Location separately from Rotation, with mutable Location-only
authority and generation-safe delayed undo. Full query scopes remain readonly;
trusted durable writes and real recursive movement are not wired yet. These
are first-map prerequisites, not live crate physics. See
TRAINING-LEVEL-MOVE-ACTOR.md. Coherent host/strict ARM64/Android native builds
pass; CTest150 executed/174 registered passes with24 owned-data skips and no
failures. The actual38-package first-map regression passes1090740 checks/77
refusals and still reports the unsupported266 startup boundary. No other maps
or new APK are included.

The subsequent bounded hit-list store now keeps separate query arenas alive
through nested marks, with global Next rebasing, expired/reused-handle refusal,
cumulative limits and allocation-free cleanup. Its real coordinator composition
still has mock lower services; actual native266 binding remains unfinished.
The already connected first-map queries now retain simultaneous scratch
reservations through nested Model admissions. No memory limit is raised.
Host/strict ARM64/Android native builds pass; CTest151 executed/175 registered
passes with24 owned-data skips, and the separate actual38-package first-map
suite passes1090740 checks/77 refusals with full rollback/source immutability.
The startup boundary and unfinished crate/loot/gravity milestone are unchanged.
See TRAINING-MOVEMENT-HIT-STORAGE.md. No other-map integration or APK is added.

The subsequent actual movement bridge now connects the complete coordinator
to the first map's real native Level/hash, retained lists, recursive Move/Smooth,
typed scalar writes, higher encroachment, child/event/touch/zone services and
native266/299. Focused owned-data tests pass122541/seven refusals with genuine
nonzero upper/lower crate collision and real NPC-clock/commit controls. The
coherent host/strict ARM64/Android native builds and shared CTest pass. Actual
startup advances past Move266 and next stops at Core rotator addition316,
before SetRotation's argument finishes. The final full actual first-map
regression passes1164319 checks/83 refusals, with complete rollback/source
immutability and the repeatable new startup boundary. This is still not automatic falling/support, live crate
destruction, surviving loot or a finished tutorial. Those remain the same
immediate gameplay target; no other map and no APK is added. See
TRAINING-NATIVE-MOVEMENT.md and ORIGINAL-ROTATOR-ADD.md.
