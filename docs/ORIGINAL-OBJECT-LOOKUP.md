# Actual UObject lookup (2026-10-18)

This implements the bounded CPU-owned lookup service established by
ORIGINAL-PACKAGE-LOOKUP-AUTHORITY.md and ORIGINAL-OBJECT-HASH-ORDER.md. It does
not load or execute original commercial DLLs/EXEs, start the original world,
construct an Engine/Cylinder, enable falling, or complete live crate gameplay.

## Actual identities, not display-path aliases

`PreparePortableRuntimeObjectLookup` explicitly requires the actual Package and
Core metadata phases. It creates no UObject, default block or GC root. A weak
index holds actual live object identities, allocation-index leases, exact
loaded/constructed FName identities and actual Outer identities. Diagnostic
paths in detached results are not lookup or ancestry authority.

Serialized export names are cached from their exact source ObjName. Native
wrapper names come from the audited registration's explicit package/name pair.
Constructed actors, windows and native player objects retain their explicit
constructor names; restored births use their validated saved native Name field.
A literal dot inside an export FName does not create a nested Outer tree.
Unknown/None query names return no object without adding a name or spelling.

A weak intrusive publication journal records successful publication order,
including real Package-phase Outer rebinding, independently of numeric indices.
It lets late preparation reconstruct hash precedence even for objects genuinely
retained after their canonical alias was replaced. It does not enumerate path
maps, keep map objects alive or admit detached probes/default blocks. Cleanup
unlinks actual identities before freeing index leases. Shutdown drains the
journal and disables the old control before replacing engine-owned domains.

The kernel uses 4096 newest-publication-first buckets and a separate ascending
numeric-index fallback. Copy/swap preserves bucket order, including explicit
same-name rehash and lower-index reuse. Registration is strict: occupied live
slots cannot be silently overwritten. Destruction/removal uses cached keys and
identity checks, never a possibly dead Outer's fields.

## Query and class semantics

`FindPortableRuntimeObject` resolves actual dotted prefixes under the current
Outer, preferring Package-derived objects and then Object-derived objects.
A missing prefix ends the query before global fallback. The exact resolved
Outer/FName hash phase precedes ANY_PACKAGE's ascending-index fallback; only
the latter ignores the resolved Outer. Qualified fallback can therefore return
another package's matching leaf after the actual prefix was resolved.

Requested Class and explicit Outer handles must identify current indexed
objects. A class handle must be an actual source ObjClass0 UClass or a closed
native wrapper, with the actual Core.Class metaclass. Filtering follows actual
Class/Super pointers, not reflection leaf names, static cast profiles or path
aliases. Imported Class references also authenticate ClassPackage Core from the
source import record. Missing relevant native UClasses (for example unregistered
Core.Function), unresolved Super links and cycles refuse explicitly.

Exact null-requested-Class lookup deliberately skips IsA. ANY_PACKAGE still
requires its separate Core.Object ancestry admission even with no requested
Class. Direct Class equality succeeds before reading an irrelevant Super link;
it does not make that link usable for a different ancestry query. Querying a
different name does not require metadata for unrelated candidates. This bounded
pure name-first optimization does not claim the original iterator's callback
trace. Unsupported ini:/usr: config resolution refuses explicitly, as do
malformed/NUL-containing, overlong and excessively segmented query names.

Allocation-index handles are process/session-local and may be reused after
actual object destruction. They are not persistent save identifiers. Results
include the monotonically increasing lookup engine generation; callers must not
reuse numeric handles across engine generations or retain raw pointers.

## Publication, rollback and lifetime

The service observes later native config/client/viewport registration, map
LinkerRoot/export publication, transactional window/actor births and restored
births. Detached, preallocated additions and retained-state admission precede
nonthrowing node-transfer publication; the full live index is not copied for a
small addition or an unchanged save restore. Script transactions journal only
their new publications. Identity-checked unlink on failure preserves the order
of every preexisting bucket member, alongside the existing UObject/world/RNG/
state journals. Provisional objects are removed from the weak publication
journal on rollback, not left discoverable until some later GC. Rename/rehash
is not currently exposed by this runtime transaction service; a future mutable
key operation needs its own inverse/order journal, not this insertion-only undo.

Save replacement publishes fresh actual birth identities. Old objects survive
only while genuine graph edges keep them alive; weak records are not forcibly
removed just because their string alias changes. Map unload revokes loader and
export ownership; GC removes unreachable weak records. Native-class-owned
Packages and native Client/Viewport UClasses retain their existing real
lifetimes independently of map/client instance retirement.

## Reproducible FNAME_Find dependency patch

`patches/surreal-engine/NameStringFindCompareIndex.patch` adds the read-only
NameString API to the pinned dependency. `tools/Initialize-ThirdParty.ps1`
checks the exact pinned HEAD, applies a forward-checked patch or recognizes a
reverse-checked already-applied patch, and refuses conflicting edits without
resetting, stashing or discarding them. The dependency checkout itself remains
ignored; the tracked patch is required for a restored source checkout to build.

`tools/tests/Test-NameStringFindPatch.ps1` exercises pristine pinned header
application, idempotence, dirty already-applied/fresh cases and conflict
byte-preservation in isolated fixture repositories. It does not claim a fresh
network download of the complete dependency. Existing application-specific
Meta OpenXR patches remain under their existing separate workflow.

## Verification and remaining integration

The independent kernel and name tests, owned lookup integration and optional
original crate lookup diagnostic are registered/available in the desktop build.
The crate diagnostic remains an isolated dormant-world test: its explicitly
selected Active state is not permission to bypass real startup in the app.
Late preparation after genuinely retained, alias-replaced births is supported
by the weak publication journal but is not directly exercised by that diagnostic.

Verified targeted results after removing the full-index copy:

- Independent Release kernel:195413 checks/43 explicit refusals, including
  allocation-denied staged node transfer, stable addresses, preserved hash
  order and unchanged live/staged state on conflict/capacity refusal.
- Standalone non-interning FName test:135240 checks.
- Owned Core/Engine integration:440 checks/32 refusals, including actual
  nested/non-Package prefixes, global fallback, wrong ClassPackage, absent
  Core.Function, map/GC cleanup, late config/client registration and shutdown.
- Optional `portable_based_actors_test --lookup OWNED_GAME_ROOT`:1560 checks/
  four refusal controls. Both original Training crates execute their existing
  isolated damage/fragment scripts with six actual WoodFragment births; every
  birth is found by real name/Class/map-root handles. Warm/cold save replacement
  uses fresh identities, collects retired records, and preserves original
  checkpoint/RNG/skins/velocities/noise and late-failure rollback invariants.
- Strict ARM64 runtime/test syntax and actual Android native module/link pass.
  The five isolated dependency-patch workflow cases pass without modifying
  the real dependency checkout or using headset hardware.
- Original unchanged crate regression:1116/four; Package11377/13, Core
  metadata10994/94 and config9971/48 regressions pass separately against the
  same final runtime. Final coherent host build passes. Fresh CTest132 passed/
  154 registered/22 explicit owned-data skips/zero failures in40.91s. These
  separately executed owned cases do not imply that every skipped case ran.

The original full-data test initially refused an unchanged clock save restore
because the entire live index was duplicated inside the32MiB retained-state
budget. Empty updates now stage nothing, new records transfer preallocated
nodes, and insertion-only transactions undo only their new nodes. The same
full-data test now passes without increasing or bypassing the budget.

This service is not yet wired to the original configured-driver class-import
path or Quest startup. Original native Engine/default/Transient/Cylinder and
level Engine ownership, movement/support/falling, complete world/player
startup, surviving contents callbacks and live weapon Fire remain separate
unfinished integration. No new APK is produced merely for this CPU-only API.

Immediate content work is now restricted to completing the actual first
Training level; see TRAINING-FIRST-PRIORITY.md. Other campaign-map integration
is deferred rather than included in these shared-engine regression claims.
