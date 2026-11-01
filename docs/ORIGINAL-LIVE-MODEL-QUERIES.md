# Current owned Model query integration (2026-11-01)

This increment connects collision queries to the actual mutable world Model
produced by original brush-tracker construction. It does not complete later
moving-object updates, collision/render gameplay publication, geometry saves,
travel, later startup services, crates or a new APK. No other map is needed.

## Separate raw, immutable and live authority

Public serialized-model query overloads remain concrete
`const PortableModelGeometry&` APIs. They retain full validation of EVERY raw
node, including unreachable components. The immutable-source overload retains
its actual-const allocation and original warm hull-proof policy. Neither API
accepts a public alreadyValidated flag or a caller-created live proof token.

Shared internal kernels are templated over the requested Model shape, not
copied into an alternate collision algorithm. Raw node-access dispatch always
considers every index active. A runtime-private view exposes actual pool Num
and original index space through a node proxy: all original nodes and all
owned dynamic nodes are active, including unreachable components. ONLY free
dynamic reserve is inactive. Its numerical/child/bound bytes are never read
merely to make a serialized validator accept native zero pool storage.

Active-node children into free records explicitly refuse. So do selected
free hull-plane references, terminal parents and hit-normal reloads. Nothing
is pruned/remapped to -1 or compacted, free records are not patched, and a
new mutable topology cannot borrow the original source's validation proof.
Region deliberately remains an active-path-only service; it does not acquire
unselected whole-graph/hull validation as a side effect of the refactor.

## Actual current-world capability

`RuntimeLiveModelView` and its constructor are private to the runtime's
anonymous namespace/host. Acquisition requires the real current Level/Model,
a completed nonmutating actual Level tracker and the current owning geometry.
The lexical scope pins Level and Model with GC roots and has a private dead/
live control plus a non-reused host-root serial. The Model and tracker revision
values are captured INDEPENDENTLY: Setup increments Model revision separately.

Every access checks the control before touching captured host/payload pointers,
then authenticates current root, map generation, exact Level/Model/payload/
tracker owners, both revisions, node storage and active source linker. An
active dynamic node must retain its actual current Level actor, matching typed
surface owner, owned vertex run and matching dynamic point owners. Selecting
an inactive, removed, foreign, replaced or stale owner refuses.

The mutable query requires the existing serialized game/script-thread caller
contract. It is not a cross-thread render snapshot or a new concurrent runtime.
Mutation exclusion and lifetime checks cover the whole lexical query, not only
its first node read. Scope completion, empty results, owner captures and the
last successful packet/statistics publication check the lease again. A dead
test callback cannot retain GC pins or make a retired payload valid.
The shared lexical barrier rejects a second host's VM root and actual map
load/unload/runtime shutdown BEFORE their mutation/retirement starts. Nested
read-only queries in the same existing root retain their own local scopes.
This is reentrancy/lifetime exclusion under the existing serialized owner,
not an atomic cross-thread protocol or permission for concurrent world access.

## Production routes

- Native FastTrace548 uses the shared FastLine kernel on the selected current
  world Model.
- World point and nonzero-extent MultiLineCheck branches select the current
  live view after construction. With no mutable payload, the original source/
  immutable-proof path and its warm budget policy are retained.
- World PointCheck zero/box occupancy similarly selects current live topology.
- Native PointRegion, SetActorZone and reflected Region reads use the same
  current world and original map-relative zone-reference resolution.
- Actor/Brush-local `NativeSelectedModel` collision remains its separately
  authenticated original source plus actual owner coordinates. A world tracker
  does not silently replace that distinct primitive operation.

Validation/query budgets and native root work are still shared by the original
caller. Free-index/ownership checks consume native work even when numerical
bytes are inactive. Query scratch and short-lived callbacks/GC pins join the
same retained-byte ledger. A failed query cannot get a fresh allowance through
the MultiLine/PointCheck collector's catch path. Existing caps are unchanged.

## Evidence scope

Generated `quest_active_model_kernel_test` controls use an explicitly TEST-ONLY
proxy, not RuntimeObject authority. They compare exact packet/arithmetic/query
behavior with valid raw fixtures in the SAME index space; raw validators still
reject malformed unreachable components while a designated free reserve may
contain unconsumed NaN/cycle/hull garbage. They check original/owned active
malformed components, active->free edges, hull references, selected-read counts,
empty/read-selection behavior, owner-capture drift and final atomic publication.
Final hardened generated result:113 checks/45 refusals, not live gameplay evidence.

The owned-data fixture additionally constructs the real first-Training tracker
inside one outer root, samples FastLine/point/extent/zero+box occupancy/Region,
checks active original/dynamic reads and free reserve, injects lifetime/owner/
revision faults, and invokes an expired callback AFTER host/root/payload
retirement. Full checkpoint/revision/GC/source-cache/source-file comparisons
must pass after root rollback. Actual final gate results belong in STATUS.md;
the presence of this fixture or a compiled header is not passing proof.

Final owned gate passes1820 checks/9 source-policy or missing-source refusals,
including all12 invalid-state controls. Eight distinct mover samples observe
1459608 actual node reads/18191 dynamic reads, with9524 original nodes and9886
free slots. Five point samples block, but dynamicPointHits=0 under flags6;
this is not a claim that dynamic solid collisions, doors or crates work.
Complete rollback/GC/source-cache/native absence and43 unchanged source files
pass. Owned coordinate regression passes154643 checks/325 identity refusals.
Real Training startup reaches the unchanged later guard with full rollback
and45 unchanged sources, NOT successful startup. Full desktop/strict ARM64/
Android-native builds and CTest252 passed/55 skipped/zero failed pass. Final
1892-input/45-original/three-selected-asset audit has zero drift/zero live jobs.

Noninitial tracker mutation/flush/destructor, current renderer geometry,
geometry checkpoint/rebind, later subsystem/viewport input/render/detail
startup and committing Training frames/crate destruction/loot/gravity remain
required. The existing later startup refusal is not removed by these queries.
