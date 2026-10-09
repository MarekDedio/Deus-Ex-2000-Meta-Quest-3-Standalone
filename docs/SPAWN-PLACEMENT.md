# Bounded world-only spawn placement

`native/quest_spawn_placement.h` is an isolated prerequisite for real actor
creation. It is not selected by the runtime, does not implement native 278,
and does not enable inventory initialization or campaign startup.

The caller must provide the active world's original root UModel, requested
Unreal-unit location, and the prospective class's default CollisionRadius,
CollisionHeight, bCollideWorld and bCollideWhenPlacing. The helper does not
manufacture defaults, actors or a world model.

## Geometry contract

The local pinned SurrealEngine revision
`677ee14c5b83486e6634687953779aafb7973ad6` supplies the contract through
`UActor_Phys.cpp::CheckLocation`, `OverlapTest.cpp` and `OverlapAABBModel.cpp`:

- With neither collision flag, preserve the requested location without a query.
- Otherwise test 27 locations in z/y/x nesting. Each axis is ordered 0, +1, -1,
  scaled by max(radius,height), using float candidate arithmetic.
- Each probe uses world-only AABB extents {radius,radius,height}. No actor,
  mover, rendered-triangle or headset-player capsule collision is substituted.
- Query original CollisionBound/LeafHulls records, their bbox and flip-bit
  planes. Inclusive contact, literal plane coefficients, double projected
  radii, inverse side signs, 1.1 traversal padding and front-before-back order
  follow the pin. The first clear candidate wins; exhaustion reports failure
  with the original requested location.

## Explicit safety boundary

Finite coordinates, nonnegative extents and bounded valid layout are required.
All-zero extents retain the pin's no-model-query behavior. An empty model with
nonzero extents refuses rather than dereferencing an absent root node.

Model version 68/licensee 0, addressed hull terminators/bboxes/planes and every
front/back component are validated before an early collision can hide bad data.
The portable decoder's -1 index sentinel is enforced; other negative child or
CollisionBound values refuse, although the pin skips any negative index there.
Hull terminators themselves still accept any negative word, matching the pin.

IEEE bbox words use memcpy. Double bbox endpoints are checked against the
finite float range before conversion. Nonfinite inputs, invalid indexes,
cycles and arithmetic overflow refuse explicitly. An iterative traversal
handles deep graphs without recursive C++ stack growth; shared children remain
valid. Query and validation work are independently bounded and shared across
all probes, including repeated hull scans. Callers may tighten fixed bounds,
not enlarge them. This can reject an otherwise valid model that exceeds them.
The bbox preflight runs before encountering a hull, unlike the pin's check
inside CollisionBound branches. Extreme finite inputs can therefore refuse
even on a hull-free model that the pin considers clear. This is a deliberate
safety boundary, not numerical equivalence for every finite input.

## Verification and remaining work

Standalone strict C++17/O2/Wall/Wextra/Werror compilation and synthetic controls
pass 9,482 checks/99 deliberate refusals. Independent axis-aligned solid oracles
constrain all 27 choices and asymmetric dimensions; slanted/flip/nonunit planes,
contact, padding/order, 20,000-node depth, malformed records, graph/work limits
and float-range boundaries are covered. The ordinary CMake test registers the
same controls; the isolated CMake build and all 36 ordinary tests pass, with
two optional original-root integrations explicitly skipped.

These are authored fixtures, not original-map collision equivalence or Quest
performance tests. Real Spawn still needs bounded identities, deep class-default
copies, ownership and lifecycle callbacks, complete birth/deletion rollback, GC
roots, checkpoint manifests, world-change publication and original inventory
continuation through GiveTo and SetBase. No installed APK is replaced by this
helper.
