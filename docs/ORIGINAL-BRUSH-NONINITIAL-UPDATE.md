# Original noninitial moving-brush Update (2026-11-01)

This batch implements dependency/removal/rebuild against the actual current
Level/Model/tracker. It does NOT bind the renderer consumer, complete startup,
publish gameplay, prove earlier-root committed-tracker COW, finish destructor/
travel/archive, or establish crate/loot/gravity/headset acceptance. No new APK
is packaged. Gate outcomes belong in STATUS.md, not the presence of this file.

## Source-selected consumer, not a guessed movement callback

The complete MoveActor coordinator has no tracker Update hook. Bounded static
inspection of owned Render.dll identifies URender::SetupDynamics, exported
thunk10B0118B -> body10B22D90. After prior frame/actor gates, the live ascending
Actor walk selects Brush/nonstatic ABrush at10B22EFE..10B22F2D. The branch at
10B230D5..10B230E5 reads actual Level+FC, conditionally calls tracker virtual
+04(Update), and supplies the current actor. This is a renderer dynamics
service, not permission to append an invented call after MoveActor or physics.
Engine.dll Update references at103AFFC8/103B01DF/103B03E1 are raytrace operations.

Render.dll was only read as file data by external objdump; no original module
was loaded/executed. Its SHA256 is
607F1077C5CD3E25F8A4CDE1FFACF4CF9260AEBC92A72BE40721CD6955BD75D1.
The future Quest dynamics adapter must establish its own genuine selected
consumer; it is not implemented by exposing this host service.

Read-only follow-up recovers the ordered gates before that call. The whole
function skips empty BSP and requires viewport Pawn.ShowFlags bit8. Its live
ascending Actor walk applies actual render-device HighDetailActors against
bHighDetail, the Frame+1C/behind-view/ViewTarget branch (Frame+1C's field NAME
is not independently authenticated), the explicit second-Actor exclusion,
game/editor hidden flags, and repeated live Owner-chain/behind-view tests for
bOnlyOwnerSee/bOwnerNoSee. It then selects nonstatic ABrush with nonnull Brush
and conditionally calls the actual Level tracker per actor. Some exclusions
bypass Update but continue later actor work. NO distance/frustum/BSP-visibility
test precedes this brush call. The set is therefore NEITHER all brushes NOR
only visible GPU chunks. Do not replace it by bulk Update(nullptr), snapshot
owner sets or the Quest presentation grouping/streaming selector. Genuine
renderer/frame/native-setting authority and those selected branches still
need implementation; this static follow-up is not a bound render capability.

## Same caller and actual ownership

Update authenticates the actual completed current Level/Model/tracker and map
generation, excludes active live-query leases, and holds tracker.mutating over
all three passes. No-op supplied unchanged/already-assimilated movers return
before Model/tracker COW. A read-only full-selection preflight similarly avoids
journals/world-revision changes for empty/all-assimilated full selections; the
actual enqueue walk still reads current Actor slots ascending.

Rooted pending entries use append-only stable indices and immutable next links.
Enqueue writes assimilation, allocates/prepends the rooted entry, then writes
actual OldPos/OldRot. Pass1 captures the old head ONCE; dependency may prepend
without extending that outer traversal. Passes2 and3 reload the final head and
follow saved entry.next. Source ordering is in ORIGINAL-BRUSH-DEPENDENCY-FLUSH.md.

Dependency marks node0x20 before Front/Back/Plane recursion and performs owner
enqueue postorder. Shared work and simultaneous recursive-frame scratch stay
inside the caller; pending roots survive all passes. Flush unlinks ONE matching
Front-else-Back-else-Plane parent edge, frees points ascending into the original
LIFO chain, clears vertex owners ascending, then clears the node owner/parent
pair. It reads node.renderBound (original+30) AFTER release, then finally writes
the actual Brush.Root=-1. It does not zero geometry, shrink arrays, reset cursors
or repair masks/children. A selected parent may itself already be released;
unlinking that parent's field is still an original mutation, not global cleanup.

A private per-pass released bitmap authorizes only those partial/released
mutation reads. Strict active query access never admits arbitrary free records.
The insertion phase retains original transforms/sphere/prefilter/surface/filter/
allocation behavior, clears visited flags on the NEW root chains only and
increments independent Model/tracker revisions with overflow refusal.

## Journals and explicit COW coverage limit

Actual Brush root writes reuse the earliest BeforeBrushModel, without copying
unchanged Polys merely to journal its owner root. Mutable Model and tracker
owners retain their earliest beforeimages. Tracker COW preparation accounts
detached arrays, capacity/root-node overlap and typed prior Model/node/vertex/
point roots before publication. Rollback restores tracker, Brush/Polys and
Model owners before object retirement. A failure is not internally repaired by
the mutation helper; the complete outer root rolls back its original prefix.

Real constructor plus noninitial updates in ONE root genuinely exercises old
dynamic geometry, dependency/removal/rebuild, allocation reuse, query revision
selection and same-root journal reuse. It does NOT exercise first-write COW of
a tracker committed by an earlier root. Startup is still guarded and preview
roots still roll back: do not clear a journal or silently commit construction
to manufacture that missing acceptance. Isolated beforeimage-edge survival
after real prior-owner retirement also remains unproven.

## Verification scope

Generated mutation-helper controls pass101 checks/18 refusals. They cover
F/B/Plane postorder, live field reacquisition, visited/assimilation distinctions,
depth/work/frame/enqueue faults, single-link parent priority, same-pass released
parents, ascending free/LIFO reuse, retained geometry/point/vertex bytes and
cursors, actual postrelease next reads and partial failures. Their owners and
callbacks are explicitly test fixtures, not runtime/GC authority.

The extended owned first-Training gate constructs the real tracker, then tests
single/full updates, unchanged/assimilated/empty-full controls, repeated same-
mover and another-mover/full updates, current dynamic FastTrace reads and old
free-node refusal. Four selected partial-root faults target dependency/cache,
ordered flush, actual reattachment and partial point free. Full checkpoint/
revision/native-absence/GC/source-cache/source-file restoration is required.
The owned gate now passes2643 checks/9 source-policy or missing-source refusals,
with43 source files unchanged and full restoration after both successes and
all four selected failures. Single Update observes1 pending/1 pass1/1 flush,
6 released nodes/24 points and5 attachments; full Update observes25 pending/
25 pass1/25 flushes,150 released nodes/600 points and149 attachments. Both
observe149 active nodes after the changed pose and complete two further real
updates with current dynamic query reads and retired-root refusal. These are
actual observed component counts, not visual/render or solid-hit acceptance.
Owned coordinate regression also passes154643 checks/325 identity refusals.
Strict ARM64, Android-native, focused and full desktop builds pass. Full CTest
passes253 tests with55 explicit skips/zero failures out of308 registered in
135.92s. Real startup reaches the unchanged later guard in311.069s with actual
initial tracker completion, native work14169034/16777216, full rollback and45
unchanged sources. This does not execute renderer-selected noninitial Update
or publish startup. Final audit verifies1895 frozen compiler inputs,45 protected
originals, three selected texture/music assets and Render.dll unchanged, with
zero live build/test jobs. STATUS records the complete gate/remaining scope.
