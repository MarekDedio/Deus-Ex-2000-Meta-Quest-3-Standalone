# Original SetupActor ownership bridge (2026-10-31)

This implements the SetupActor portion of the selected original moving-brush
constructor. It does not complete that constructor: pool allocation, initial
Update, recursive filtering/BSP insertion, later startup, playable crates and
headset acceptance are still required. The post-partition startup guard remains.
No original commercial DLL or executable is loaded or executed.

## Source identity before runtime initialization

`LoadPortableRuntimeBrushTextureSources` discovers authored Mover Brush hints
from the actual Level actor order, follows actual class ancestry and validates
each selected native Model and full Polys body. Nonzero polygon Texture references
resolve through actual imports, ordered Core.System.Paths and matching exports.
Actual Engine.Texture Class and Core.Package outer identities and original load
context are authenticated before the additional tables join the source baseline.
The production startup and desktop Training probe call the same admission code.

The runtime binder must still authenticate current fields and object identities;
prefetching a hint does not fabricate a runtime Brush=None result, run scripts or
claim support for arbitrary later dynamic/inherited asset changes. There are no
hardcoded Training texture/package names in the production loader. The first
Training fixture independently expects its two actual material packages and two
bindings; see TRAINING-BRUSH-SETUP-ASSETS.md for source-only discovery evidence.

The detached loader has shared finite retained/work/serialized/directory bounds.
Streaming preflight sizes table/name storage before loading, without decoding
whole mip payloads or reserving a multiple of the entire texture file as tables.
The existing shared NameString loader interns into a process-lifetime arena; its
existing resize/rehash/rollback policy is outside this detached budget. The loader
is not a total-process memory cap or an arena transaction. This limitation is
explicit in its public header rather than hidden behind a passing byte counter.

## Runtime ownership and exact stores

Native Model+54 binds to the actual indexed, source-local Engine.Polys object,
not a reflected property or UPrimitive. Its mutable native payload retains the
full source polygon value and parallel typed Actor/Texture references and actual
NameString handles. Raw source-package ordinals remain provenance only.

The actual world Model owns appended surfaces, points and vectors plus matching
typed surface Actor/Texture edges. SetupActor selects current nonnull Brush,
ABrush and nonstatic actors in ascending live native actor-array order, and
checked-casts each selected actor to Mover instead of silently skipping it.

The stores follow the recovered original contract:

- Brush root becomes -1; OldPos=(-1,-1,-1), OldRot=(123,456,789), bAssimilated=false.
- Each source polygon appends one zero surface, three zero vectors and one zero
  point, retaining original source-order correspondence.
- Surface normal/U/V/base indices select those new slots; pans are copied.
- Surface lightMap receives source polygon brushPoly verbatim, surface brushPoly
  receives the source ordinal, and source polygon link receives the new surface.
- Flags retain source mask0x3cffffff and OR0x100000 when bSpecialLit is set.
- Real native Texture and current Mover pointers are retained as typed edges;
  source map reference values in the value record are not pointer substitutes.

Setup does not prematurely transform the zero placeholder vectors/points.
Transformation and insertion belong to the mandatory Update continuation.

## Shared budget, journal and GC boundaries

Model/Polys first-write journals root their actual objects, retain prior owning
payloads and root all prior typed edges while copy-on-write replaces an owner.
Model->Polys and optional Brush-root bindings have their own before-images.
GC visits current native Model/Polys edges. Rollback swaps previous Polys owners,
restores Model bindings, and swaps the prior world Model before object retirement.
Reflected pose/assimilation stores use the outer VM reference undo machinery.

Detached prepared payloads, old-edge root vectors and identity workspace remain
in the shared scratch ledger across nested object/class resolution. Admission
is not an isolated check that lets a nested helper reuse the same allowance.
Transfer into the owning object/journal is followed by no fallible publication
step. Shared retained/work caps are unchanged.

Mutable native brush geometry is unsaved: capture and restore explicitly refuse
until a genuine geometry archive/rebind path exists. No mutation is written into
commercial source files as a substitute. This observation API always rolls back;
it is not an alternate startup success path.

## Verification scope

The focused owned-data fixture is `portable_brush_setup_test --game-root ROOT`.
It checks the first Training cohort against independently decoded source bodies,
all exact per-polygon surface/cache/link stores and zero placeholders, typed
identities, a completed root rollback and injected partial-write refusals after
polygons1/7/149/150. It compares complete checkpoint bytes, world revision, native
geometry absence, GC statistics and original source fingerprints after rollback.
Texture admission also has explicit bounded-policy/missing-source refusals.
Actual gate outcomes are recorded in STATUS.md once run; this test description
alone is not passing evidence.

The observation invokes real GC visiting while current edges and before-images
are installed. Original assets also have immutable baseline roots: stable GC
counts and correct edge identity do not isolate edge liveness after baseline
retirement or prove travel teardown. Already-published tracker copy-on-write,
dynamic pool ownership and geometry checkpoint restoration remain separate gates.

Nothing here proves recursive FilterPoly/AddPolyBsp, completed GNewBrushTracker,
current-revision collision/render views, later input/render/detail startup,
crate destruction/loot/gravity or a new playable APK. Other-map integration is
deliberately skipped while first Training remains the immediate gameplay target.
