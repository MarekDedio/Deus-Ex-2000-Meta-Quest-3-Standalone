# First Training: original collision registration prefix

The current content target is **only `Maps/00_Training.dx`**. This checkpoint
connects the previously detached original collision-hash kernel to actual
Training UObject/ULevel identities and original primitive providers. It is not
live crate damage, usable loot, gravity, complete original startup or a new APK.
Other campaign-map integration remains deferred.

## What is connected

- The actual current native Level owns a numeric `Hash` lease; no native pointer
  is stored in a script property or checkpoint. Actors are authenticated through
  the actual UObject lookup and current native Level/generation before use.
- The original LoadMap collision-enable prefix runs before GameInfo selection
  in a new, strict rollback-only startup diagnostic. Ascending live Level slots
  supply actors, with the original nonnull/`bCollideActors` registration test.
  Primitive bounds and raw line packets come from the existing actual
  Brush/Mesh/Engine.Cylinder providers, not renderer boxes or a second scene.
- The kernel retains its genuine process-wide permutation tables and query tag
  across enable/disable cycles within one diagnostic. First initialization
  consumes49152 draws from the existing shared CRT stream; it is neither a
  per-query seed nor a per-map redraw.
- Qualified live `Engine.Actor.CollisionTag` and `ColLocation` reads resolve to
  the same native actor records used by registration/query. Default/immutable
  reads remain separate source-default reads. Generic writes cannot shadow
  these native mirrors.
- Spawn registers collision after its initial transform and before Region,
  SetOwner, initialization and callbacks. Original native SetCollision262 and
  SetCollisionSize283 now authenticate their declarations/fields and perform
  old-bounds removal, typed field writes and new-bounds insertion in the same
  root. Unsupported generic shape mutation and Destroy still explicitly refuse
  in an active retail hash rather than using the older approximate registry.

## First Training setter and memory follow-on (2026-10-18)

The actual UObject lookup no longer stores a redundant previous-chain pointer:
its stable numeric tree records, exact-name precedence and identity lookup are
unchanged. This removes8 actual bytes per node on64-bit hosts; it does not lower
the conservative tree-node allowance or omit loaded objects.

Native startup ClearTouching now authenticates the original declared four-slot
ObjectProperty and only materializes reset overlays for non-None slots. An
already-None assignment has no callback or property setter to preserve. Authored,
mutable and frozen-birth non-None values still receive journaled None writes.
Generated controls check empty/nonempty source slots, typed frozen-birth slots,
callback observations and exact rollback. This is a narrow native reset change,
not general script-write elision or forced begun-play state.

SetCollision's optional defaults are sampled separately immediately before each
argument expression; ordinary input values are copied before later expressions
can mutate their references. SetCollision returns no value; SetCollisionSize
requires both scalar inputs and returns Bool true after success, not a guessed
fit result. Identical setters still remove/reinsert and change hash encounter
order. Unsafe nonfinite/negative sizes explicitly refuse as portable supported-
domain policy. See [the static original audit](ORIGINAL-COLLISION-SETTERS.md).

The first-map integration test independently checks real shared-cell crate
encounter order before/after unchanged setters, collision toggles, preserved
world flags, resized primitive hits, height bounds, typed results and late-root
rollback. No other campaign map is loaded for these tests.

These physical ownership reductions pass the earlier InitGame and SecurityCamera3
state-budget boundaries. The next observed strict startup refusal is now
PreBeginPlay slot759, `00_Training.PatrolPoint1`, in
`Engine.PatrolPoint.PreBeginPlay` offset39/op47:
`Native actor lifecycle work budget exceeded`. The shared65536 work limit is
unchanged and includes49152 first-initialization CRT draws; no counter is reset
mid-root. This remains rollback-only startup, not successful world publication
or live crate physics. Move, support/falling and usable surviving loot are still
unfinished. No replacement APK is produced for this checkpoint.

The existing state budget is not yet a full physical-memory/undo measurement:
property accounting remains save-shaped and does not count every Before journal
allocation. These narrow real-storage reductions must not be described as fixing
all state/undo memory accounting; any future accounting change must retain the
whole rollback domain and independent checkpoint limits.

Final coherent host build passes. The owned exact38-package first-map suite
passes390461 checks/30 explicit refusals with unchanged source fingerprints;
the stricter startup refusal above is asserted, not counted as successful
startup. The same-runtime Engine regression passes42647/31 and the isolated
first-map crate diagnostic347/two (its contents still fail dormant Game=None
relevance; it is not surviving usable loot). Generated Spawn/reset/rollback
controls pass9211/144, VM695763/846 and lookup203655/59. CTest executes133 of157
registered tests with24 explicit owned-data skips, zero failures,54.42s. Owned
data runs listed here are separate; other skipped tests are not claimed.
Strict ARM64 syntax and Android native compilation/link pass. Evidence uses
`artifacts/training-setters-memory-final-*20261018.log` and
`artifacts/training-sparse-touching-*20261018.log`; no APK is generated.

## One transaction, not independently committed native subcalls

`OriginalCollisionHashWorld::RootLease` owns a complete undo snapshot and one
work ledger. Nested hash operations join that root, including nested failures
that a provider catches. It stays alive through VM reference undo. Host rollback
restores links, native tags/ColLocation and Level.Hash before retiring spawned
lookup identities, then restores shared RNG and other actor effects.

The last throwing native publication step is `PrepareCommit`. The final
`CommitPrepared` boundary is nonthrowing, after all host preparation. The VM
also materializes its success identity before host publication; the publication
tail cannot fail on a later success-string allocation. Kernel tests exercise
allocation failure, poisoned nesting, lease misuse and provider abandonment.

The current public runtime entries in `portable_retail_collision.h` are
deliberately **rollback-only**. The older `ProbePortableWorldStartup` remains an
isolated script/callback diagnostic, including generated fixtures; it does not
silently acquire this earlier native LoadMap prefix. Use
`ProbePortableRuntimeRetailCollisionStartup` to test the stricter boundary.

## Explicit remaining boundaries

The old Destroy helper marks pending-delete before collision removal. Original
Destroy removes first and sets that bit much later; swapping only its remove
call would strand links. The active-hash path refuses before that helper mutates
the actor. This is a guard, not an implemented original destruction sequence.

Native Move266, SetRotation299, encroachment/touch/bump, base finding and true
support/falling, publishing original world/player startup, native hash retirement
and atomic checkpoint reconstruction remain unfinished. Save/validate/load and
map unload fail closed while the new native domain is active. No active native
hash can be silently checkpointed as stale generic properties or retired through
the old registry. Persistent CollisionTag overlays explicitly refuse; the
original archive instead skips a well-typed transient tag. ColLocation is not
transient, but admitting its type alone does not implement a hash restore.

See [the original hash contract](ORIGINAL-COLLISION-HASH.md),
[the field/archive and lifecycle audit](ORIGINAL-COLLISION-HASH-SAVE.md), and
[the actual Engine prerequisite](TRAINING-ENGINE-COLLISION.md). No original
commercial EXE/DLL is executed or modified by this work.

## Acceptance boundary

These CPU tests must establish correct source registration, primitive packets,
declared native mirrors and complete rollback before this prefix can support
the next startup/movement step. They do not establish headset-visible falling,
surviving usable contents, live weapon attacks or completion of Training.
Keep the last playable installed APK unchanged until there is a meaningful
playable replacement; automatically install a new APK when one is produced and
the authorized Quest is connected.

The owned exact38-package first-map run currently registers1308 nonnull native
Level-slot actors,120 collidable/nondeleted actors and478 links. Its separate
vector-bucket/CRT oracle compares actual Time0 primitive packets, encounter
order, result links and native query tags. Repeated enable is a no-op; disabling
destroys only its hash lease, and re-enabling does not redraw the tables.

At the earlier registration-only checkpoint, strict startup refused during InitGame, slot1337
`00_Training.DeusExGameInfo0`, at `Engine.Actor.SetInitialState` offset25,
opcode113: the retained-state budget is exceeded. The captured desktop values
are16776735 bytes already retained before the rejected increment, including
487424 native hash bytes,164997 detached diagnostic bytes and13479480 actual
lookup bytes. This is the observed boundary, not successful original startup
or evidence of reaching Move266. The16MiB state limit is unchanged; diagnose
and reduce genuine retained ownership before advancing this path. That observed
boundary is superseded by the follow-on above. The older
isolated startup probe does not include these added native tables/mirrors.

The earlier owned integration suite passed122490 checks/12 explicit refusals, including
source-file immutability, malformed/late-failing commands, a short VM budget,
and rejection of actor/default/frozen-birth persistent CollisionTag records with
accepted positive controls. The kernel passes10778569 checks/51 refusals and the
VM695583/838. Strict ARM64 syntax, Android native link and coherent desktop host
rebuild pass. No APK is produced for this rollback-only prerequisite.

Final coherent-build CTest executes133/157 registered tests, with24 explicit
owned-data skips and zero failures in59.64s. Owned data is supplied separately
to this122490/12 test, the exact38-source Engine42647/31 regression, and the
isolated first Training crate347/two regression; the rest of the skipped set
is not claimed as executed. Ignored evidence logs use the
`artifacts/training-retail-*20261018.log` prefix.
