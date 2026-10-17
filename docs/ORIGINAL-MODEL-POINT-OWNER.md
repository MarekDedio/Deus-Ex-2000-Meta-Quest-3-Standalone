# Original selected-Model point bridge (2026-10-17)

This adds the missing point-query branch to the real selected Brush Model
provider. It is a weapon/ray prerequisite, not Move, FindBase, falling, world/
player startup or playable crate destruction. It does not replace the missing
Engine.Cylinder fallback. The existing extent provider remains separate.

## Original evidence

Read-only static inspection of owned Engine.dll, SHA256 independently rechecked:
0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.
No commercial executable or DLL was loaded or executed. Preferred PE VAs are
source evidence, not portable object offsets.

- Wrapper103F3C20..103F3F6D selects point traversal after three x87 extent
  comparisons. C3 also accepts unordered values: all-zero-or-NaN components
  select the point branch and are inactive. A nonzero component selects the
  extent branch with all original values retained.
- With a nonnull owner it calls actual Owner.ToWorld slot88 at103F3CA7 before
  recursion. Recursion103F3560..103F3AB9 transforms each plane's normal with
  Z/Y/X dot scheduling and Float stores. It computes Origin.TransformVectorBy
  and keeps W-dot unrounded through the endpoint-distance subtraction. The
  null-owner inline distance instead sums (Z+X)+Y, not FastLine's (Z+Y)+X.
- Front/back same-side comparisons use Double-0.001/+0.001 with front
  precedence in the overlap. Their CSG mask clears flags10 before OR21
  (103F372C..103F373C and103F3791..103F379F). Crossing calls actual
  FBspNode.IsCsg103F68B0 with the full flags byte OR21. NumVertices must be
  nonzero. No render polygon/surface collision substitute is used.
- Wrapper resets1058F34C before traversal. An outside leaf sets that query-wide
  value. flags10 suppresses an initial solid leaf only while no outside leaf
  has been encountered (103F3A16..103F3A42). A later solid reentry still hits.
  LastNode stays unchanged through same-side/near descent, then becomes the
  crossed parent for far descent. Hit.Item is that node index, not its surface.
- A clear return leaves every caller hit field untouched. A hit writes the
  Model identity, Item, raw entry location/normal, then wrapper Time/Actor and
  backed-off location. Next is preserved. Projected Time is Float-stored before
  subtracting0.5/sqrt(stored segment-square); clamped Time is stored but kept
  unrounded for offset multiplication, with offset/location Float stores.
- With no owner, normal faces against travel. With an owner, the wrapper calls
  ToWorld again ONLY after a hit (103F3E41), transforms the raw local normal by
  those second captured axes with (X+Y)+Z scheduling, and does not normalize or
  face it. The second capture's Origin is inactive.

## Implemented provider and safety boundary

`quest_model_point_line_hit.h` implements these rules using the existing exact
PC64 dyadic arithmetic and square-root helpers. This does not reuse the older
flags6/null-owner Double-arithmetic decal kernel, FastLine or an extent sweep.
Traversal is iterative; front/back structure and all plane data are validated
on every query, including warm geometry. Shared branches are legal, cycles are
not. No externally supplied already-validated capability bypasses that work.

Fixed bounds remain1 million nodes,8 million validation steps and16 million
query steps; callers may only tighten them. Pending stack growth is node-bounded.
Active nonfinite/overflow data, malformed topology, exhausted work and blocked
zero-length arithmetic refuse. Packet/statistics publish only after all work
and both required owner captures succeed. Empty Models return RootOutside
without owner capture, endpoint/extent reads or caller mutation, under the
existing bounded structural admission policy.

The native bridge retains actual qualified Brush-first selection, current
ULevel/generation, exact native Model/export/source authority and the bounded
selected-Model cache. Point and extent queries charge the same outer8M/16M
validation/query ledger; no extra trace or warm-validation bypass is used.
The actual native ToWorld service supplies both captures. There is no decoded
mesh, render matrix, root Model, guessed cylinder or fabricated Engine fallback.

## Verification scope

The commercial-free point bridge test passes139308 checks/10 explicit refusals,
including4099 independent recursive hardware-x87 comparisons. Its oracle uses
test-only scalar arithmetic, not production dyadic/trace/coordinate helpers.
Controls cover all256x256 query/node flag masks, real initial-solid/reentry
ordering, arbitrary caller seeds, literal half-unit backoff, owner/null normal
differences, changed second-capture axes, inactive second Origin, second-capture
failure rollback, adjacent thresholds, unrounded W cancellation, distinct null
dot ordering, an8192-node chain and invalid graph/work/numeric admission.

Full coherent host rebuild, Android native module/link and strict ARM64 syntax
pass. CTest129 passed/147 registered/18 explicit owned-data skips/zero failures
in54.44s; the owned fixtures below were run separately with actual data.
Generated provider715 checks/six explicit refusals exercise literal point packets versus extent Time2,
discarded/active NaN extents and exact cache/state/lifetime preservation. The
owned Training fixture passes74660 checks/246 explicit refusals, comparing four
flag variants for all1017 actual selected Models among1308 actors. It also
exercises64 Mesh actors and256 cylinder line probes, fingerprints all source
bytes/mtime and preserves the complete checkpoint/revision/GC/clock/world state.
The1017 Models retain the same700781 accounted host bytes. Reusing the kernel there is adapter
evidence, not the independent arithmetic oracle.

The qualified original null-Brush/null-Mesh census finds227 actors, of which31
have bCollideActors and4 have bCollideWorld. Collidable classes are DamageTrigger1,
DataLinkTrigger17, FlagTrigger2, OrdersTrigger4, Camera4, Teleporter2 and Trigger1.
These are dormant authored/current flags before startup, not permission to filter
editor actors or omit candidates from the original collision phase. They prove
that an all-Mesh/Brush-only load cannot stand in for the whole Training actor
hash; actual Engine.Cylinder fallback remains necessary.

The replacement APK was packaged and replace-installed on the verified Quest3
on2026-10-17; see MODEL-POINT-QUEST-DEPLOYMENT.md for exact artifact provenance.
Quest GPU behavior, weapon Fire/equip, startup, gravity, contents or live crate
acceptance is not implied. Actual configured native defaults,
Engine/session lifetime, hash/collector/Move/support, startup/player and original
damage/committed graph publication remain required.
