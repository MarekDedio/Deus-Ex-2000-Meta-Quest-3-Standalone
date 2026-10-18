# Immediate milestone: complete the first Training map

User priority,2026-10-18: make the first original Training map fully work
before spending time on other campaign-map integration. The long-term full
campaign objective remains, but it is not the current acceptance milestone.

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
