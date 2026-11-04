# Native Client transaction lifetime

Implementation2026-11-04; focused and broad verification passed. This is engine-session
safety, not playable startup, crate physics, or a packaged APK.

## Authority and rollback

A numeric Client/Viewport ID is not a lifetime. The retained graph records the
exact session, Client, Engine domain, runtime, lookup generation and binding
token. Every initial actual viewport separately roots its Actor, Canvas, Console
and Input, because Clear severs their original owning edges. All root/capacity
storage is admitted before Begin publication and metered while retained.

Commit requires live binding authority and authentic current target provenance.
Both main commit checks use that authority; config publication checks again
after file preparation and before irreversible rename. Empty actual arrays do
not imply a dead Client. Actual-array consumer migration remains separate.

Undo needs structural ownership, not permission to render. Revoke alone denies
publication but does not mean Clear: exact still-owned beforeimages are restored.
Clear/unindex/replacement instead forbids restoring old player, viewport Actor,
Canvas and Console links. An already established native Player=None remains
authoritative. Input COW undo reapplies the existing retirement policy only when
its original structural owner is gone; it cannot restore old ownership, an Init
receipt, cursor or physical baseline to a retired object. Undo predicates perform
no allocating schema/path lookup, callbacks or throwing budget checks.

External Input construction now refuses an active VM root; its publication is
not a journaled in-root birth. Existing idempotent reads remain available.

## Verification and limits

The new query test first reproduced the old failure: Clear followed by a fresh
token and the same numeric IDs allowed a query-only root to commit. The failing
run is retained. The expanded registry test distinguishes retained token identity
from live authority. A separate graph diagnostic uses genuine scalar references
and actual Core SET/Input COW; ordinary failure, Revoke, Clear and same-ID rebind
all deliberately roll back, with GC during teardown. SET requests a config flush,
so this diagnostic always fails before publication and never commits its command.

The owned graph fixture genuinely constructs Canvas, Input and Console but is
unpossessed and uninitialized: positive paired-player, Init receipt and nonzero
controller-state retirement require further actual-startup coverage. Null-state
checks must not be reported as proof of those positive cases. No invented Pawn,
force-initialized flags or synthetic renderer are used.

Focused desktop build and six owned/pure suites pass: query/alias85 checks/12
refusals, Core SET with graph controls9712/10, viewport Input10698/98,
Canvas150/14, Console5505/34 and registry119/39. Android-native and strict ARM64
runtime builds pass. The focused final audit verifies1937 frozen compiler inputs,
45 originals,3 selected assets and4 source DLLs unchanged, with no live jobs or
new APK. The later distance migration passes10788/103 after correcting its GC
baseline to account for exactly three legitimate Clear retirements. The actual
Training startup gate passes its expected later refusal in620.018s with267089
instructions/38087 writes; initial/read/rate/laterReset and complete save/state/
revision/hash/RNG/client/lookup/Actor/GC rollback remain intact,45 original sources
are unchanged, and immutable Model replacement starts cold. No startup is
published. The graph diagnostic always fails before Commit and
therefore does not exercise revocation during config backend preparation.

An additional actor/spawn precheck reproduced an old decal-fixture incompatibility:
TextureNone expected a successful client-dependent commit with raw descriptors
but no actual native Client. The fresh full build passes; CTest confirms exactly
that one failure in262 executed/321 registered/59 explicit skips,146.09s. Both
failures are retained. Final frozen3 audit finds1937 inputs/45 originals/3 assets/
4 DLLs unchanged, zero live jobs and no new APK.

After those jobs ended, the fixture migration was applied: it supplies genuine
Player.Actor metadata to the decal profile and constructs real native Client/
Viewport objects using the existing constructor-only path for each live binding.
It retains positive geometry/OUT/RNG/GC checks; no missing Engine is fabricated,
no initialized flags forced, and no guard is weakened. The old revocation error
substring is preserved. Corrected actor/spawn coverage passes13810 checks/231
rejection controls, including finite-polygon/provisional SurfList/RNG/mover/
checkpoint/GC/late rollback. Android-native and strict ARM64 pass. The corrected
full desktop build and CTest pass262 executed/321 registered/59 explicit owned-
data skips/zero failures in159.87s. Final frozen4 audit verifies1937 compiler
inputs,45 originals,3 assets and4 source DLLs unchanged, zero live jobs and no
new APK. A stale fixture comment was then clarified without changing code;
the post-comment manifest/audit preserves that distinction. Actual initialized
Engine/Client-array decal migration remains separate. The actual Training gate above ran
before this fixture/error-text-only correction, not against a new APK.

Nonnull concrete renderer payloads still require their own retained-byte/lifetime
provider; unjournaled in-root NewViewport and arbitrary map replacement are not
made safe by this change. Independent RootWindow/FlagBase retirement is unchanged.
Original initial startup, remaining actual-array sound/startup/decal consumers,
local-player movement/aim and live weapon/crate/loot/physics remain unfinished.
