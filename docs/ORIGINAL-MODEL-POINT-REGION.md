# Original Model.PointRegion for first Training (2026-10-19)

This supplies the BSP region query required by movement. It does **not**
implement movement-time SetActorZone, its event/publication order, Move266,
world startup, support/falling, or playable crate destruction/usable contents.
Only `00_Training.dx` is the real-map integration target.

## Original authority

Read-only static inspection of the user's GOTY `System/Engine.dll`:
SHA256 `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
UModel.PointRegion thunk `1030391D` reaches the complete body
`103F5DB0..103F5F38`; the saved disassembly is
`artifacts/world-native-startup-20261011/original-pointregion-body.asm`.
No commercial executable/library is loaded or executed by these tests.

- The caller supplies a nonnull fallback actor. Empty Nodes returns that
  actor, leaf `-1`, ZoneNumber0 without reading point/planes/zone table.
- Nonempty traversal starts at node0. At `103F5E23..103F5E4B` the x87 computes
  `((Point.Z*Plane.Z + Point.Y*Plane.Y) + Point.X*Plane.X) - Plane.W`, rounding
  each arithmetic instruction at PC64 precision. There is no Float or Double
  spill before the comparison with Double zero. Ordered nonnegative, including
  either signed zero, chooses side1/front; negative chooses side0/back.
  Native unordered takes the back branch. The portable finite-active-input
  contract refuses that case instead of importing unchecked numeric behavior.
- Use that same side for Child, Leaf and Zone. Only child `-1` terminates.
  Equality with an absent front child never takes a present back child.
- The outside/CSG bookkeeping reads rootOutside/vertex-count/flags but cannot
  affect the next geometric side or any returned field. The pure query omits
  this nonobservable bookkeeping, not a geometry gate.
- Return the terminal signed leaf word directly; this routine never reads or
  validates an entry in the Leaves array.
- NumZones zero forces ZoneNumber0. Otherwise retain the selected zone byte.
  Resolve that slot in the native64-entry table. A null slot falls back to the
  caller without changing the selected leaf or zone number. Serialized slots
  above the actual zone count are the corresponding null slots, not a reason
  to erase the selected ZoneNumber.

## Portable implementation and boundaries

`native/quest_model_point_region.h` supplies a staged, allocation-free success
path over immutable `PortableModelGeometry`. Its returned signed package
reference is opaque: zero means the caller's nonnull fallback, not no region.
It does not resolve actors or publish reflected fields/events.

Only the active node path and active numeric fields are inspected. Empty BSP
ignores poisoned point/zone data and a zero traversal allowance. Malformed
active child/index, cyclic/exhausted paths, nonfinite active inputs, selected
zone outside0..63 and a terminal zone table larger than64 receive explicit
portable refusals. These are safety policies, not native unchecked-access
equivalence. Inactive sibling planes/children/zones and the Leaves array do not
need admission. A supplied shared-work hook is never reset by the helper.

The existing runtime Region calculation now uses this query instead of its
Float X/Y/Z loop. Native XLevel selects the current loader-owned Model; the
same immutable Model cache as FastTrace verifies native map generation,
Level/Model/source/export/object identities. This is not a render-geometry
fallback or a separately searched root Model. The existing full decoder/cache
admission is unchanged; the pure helper's lazy domain does not relax it.

The input is the qualified `Engine.Actor.Location` with the existing
FastTrace Core.Vector layout/ownership admission, not a leaf-name Location
that a subclass could shadow. This does not add a claim of complete native
field-flag registration. Actual native slot0 supplies the fallback; slot0
itself retains the existing special tuple `(slot0,-1,0)`. A query may warm the
read-only geometry cache but must not create Region overlays or alter gameplay,
saves, actor/GC identities, native hash, revision, client binding or RNG state.

The existing convenience `verticalOffset` call sites are unchanged. In
particular this does not claim original movement-time sampling after zone
callbacks, Pawn Foot/HeadZoneChange, exact head/foot coordinate construction,
PRI updates, or changed-zone callbacks. Those still need the complete audited
SetActorZone coordinator inside the real movement root.

## Verification

The standalone test's deterministic controls cover empty/lazy data, front and
back equality/zero, absent selected child with a poisoned opposite child,
all256 flag bytes with four rootOutside words, NumZones0, populated/null/missing
slot63, raw signed leaves, active nonfinite data, malformed/cyclic paths,
exact local/shared-work boundaries and retained subnormal/large products.

Its independent hardware oracle directly executes x87 arithmetic under a
saved/restored PC64/nearest control word; it never calls production numeric or
region helpers. Random full-range Float planes/points, multilevel trees and
deliberate cancellation cohorts compare complete tuples and node counts.
The initial run passes201293 checks/35 explicit refusals,52106 independent
hardware-x87 queries including12000 multilevel paths, with96 witnesses against
the former Float X/Y/Z path and10 against a binary64 replacement.
Log: `artifacts/training-point-region-unit-20261019.log`.

The owned integration additionally compares every actual first-map actor's
qualified Region against a separate source-Model/hardware-x87 oracle, with an
explicit cold-cache assertion and a second pass after cache warming/GC. All1308
actors agree on both passes:43442 independent hardware-x87 node visits,
2566 fallback/50 assigned zone tuples and seven selected zone numbers. Complete
save/state/source-immutability checks still pass. The final38-package,
first-map-only regression passes1043599 checks/68 explicit refusals; original
startup still reaches `00_Training.DeusExMover16`, Engine.Mover.BeginPlay PC166,
opcode97, unsupported native266. No later readiness is inferred from that pin.
Log: `artifacts/training-point-region-owned-final-20261019.log`.

Strict ARM64 runtime and test syntax, the coherent host build and the Android
native module/link pass. Final CTest:146 executed/170 registered,24 explicit
owned-data skips, zero failures in34.74s (51.11s before adding the explicit
cold-cache guard). Generated actor spawn/lifecycle controls also pass9211
checks/144 rejection controls. Logs:

- `artifacts/training-point-region-arm-runtime-20261019.log`
- `artifacts/training-point-region-arm-tests-final-20261019.log`
- `artifacts/training-point-region-host-build-20261019.log`
- `artifacts/training-point-region-final-build-20261019.log`
- `artifacts/training-point-region-android-native-20261019.log`
- `artifacts/training-point-region-ctest-final-20261019.log`
- `artifacts/training-point-region-spawn-20261019.log`

No APK is packaged or installed for this backend-only correction. The complete
next movement-time zone audit, including event/store order and fresh coordinate
sampling after callbacks, is in ORIGINAL-MOVEMENT-ZONE.md; it is not reported as
an implemented runtime provider.
