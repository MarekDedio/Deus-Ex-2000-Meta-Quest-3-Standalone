# Crate fragment scene eligibility (2026-10-17)

The user accepted the Training NPC poses and asked for destructible crates next.
This checkpoint fixes an independently reachable renderer omission. It does not
claim that headset crowbar attacks, surviving contents or falling stacks work.

## Defect and implementation

The Quest scene builder admitted only Pawn, Inventory, Decoration, Mover,
Triggers and travel actors. Actual WoodFragment births from both Training crates
have none of those roles, despite having a located, visible original mesh.
They were dropped before geometry preparation. The offline actor preview had
the same class-category restriction.

`quest_actor_scene_selection.h` now separates interaction roles from supported
visible assets. Existing gameplay-role admission is preserved, including hidden
collision movers and invisible triggers. Located, nonhidden DT_Mesh actors with
an actual mesh path and the existing Quest sprite types1/4/5/7 with a texture
can additionally enter the scene. Engine.Camera remains excluded. These are
render candidates, not permission to invent geometry, pickups or physics.

The Quest builder traverses movers first, other gameplay roles second and
presentation-only actors last, preserving source order within each group and
the existing streaming/removal/512-slot limits. New render-only births therefore
cannot displace retained doors or interactables. All qualifying movers still
receive the existing complete-coverage overflow check. Presentation-only entries
own visibility/geometry ordinals but remain unusable, unhighlighted and excluded
from the temporary point-targeted weapon attack path. No fake class category or
crate-specific name rule is used. Normal pose/material preparation and complete
bank publication are unchanged.

The desktop actor preview uses the same eligibility predicate. Its existing
sprite-omission diagnostic remains explicit; this change does not add offline
sprite rendering. The Quest material collector already considers all actor
snapshots, so the new fragment selection retains actual authored materials.

## Original-data verification

The existing isolated original TakeDamage test now verifies each of the six
WoodFragment births from each actual Training crate:

- No gameplay-role flags; selected into the presentation scene group instead.
- Actual mesh geometry is already available through the non-lazy mesh-view API
  immediately after commit, before any test-side decode or replacement.
- Its authored pose is drawable and every shared CPU triangle has finite
  transformed positions/UVs.
- Those render queries preserve the entire damage/fragment/RNG checkpoint and
  committed world revision.

Both crates pass1116 checks/four expected refusal controls. Existing exact
warm/cold save replacement, repeated destruction, real Pawn-instigator noise
cache checks and final-return rollback/GC controls continue to pass. The test
still explicitly selects Active only in an isolated dormant-world fixture;
it does not bootstrap the game or authorize doing that in the Quest app.
All original source bytes and modification times are verified unchanged.

Commercial-free selector tests pass18706 checks across all256 draw-type bytes,
asset/hidden/Location/Camera combinations, all role combinations and generated
fragment-shaped data. Targeted host, coherent full rebuild and actual Android
ARM64 native module/link builds pass. Fresh CTest:128 passed/146 registered/
18 explicit owned-data skips/zero failures in33.73s. Independent read-only
integration review found no blocking issue in priority/cap/interaction consumers.

An offline close-up also executes genuine inherited Spawn with the actual
DeusEx.WoodFragment class, then isolates its returned Actor. The angled640x640
capture renders12 original triangles/19188 covered pixels (4.68%), no missing
materials, and visually shows the authored wood shard. It is an isolated birth
fixture, not a fatal-hit or physics video. The initial near-edge-on view rendered
3034 pixels (0.74%) and correctly failed the chosen2% viewpoint gate; the
broader-face camera passes the same gate without changing geometry or thresholds.
Local captures/reports use `artifacts/crate-scene-woodfragment*-20261017.*`.

Ignored local evidence: `artifacts/crate-scene-*-20261017.log`.
These are CPU/source/native-build checks, not Quest GPU or live crate acceptance.

## Remaining gameplay integration

The original damage chain can create and remove real actors transactionally in
isolated tests, but FireTargetedActor still reaches the old Pawn-only damage
helper. It must eventually invoke real weapon/player-owned script damage and
publish the entire committed world graph, not simply mask one killed actor.
Actual native defaults/session lifetime, collision-hash/Move/support, world/
player startup, contents relevance, falling/landing and fragment ticking remain
required. No generic float HitPoints replacement, forced begun/Active flags,
inventory grant or visual position-settling workaround was added. See
TRAINING-WEAPON-CRATE-GAPS.md and ORIGINAL-NATIVE-CONFIG-IMPORT.md.

No replacement APK or installation is claimed for this prerequisite checkpoint.
