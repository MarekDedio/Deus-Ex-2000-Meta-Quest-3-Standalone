# Original Rotator-to-Vector conversion

Normalized UE1 token `0x39` now evaluates its child once, loads the detached
Rotator value, and returns the exact pinned engine's
`Coords::Rotation(Rotator(pitch,yaw,roll)).XAxis`. No new approximation,
normalization, Quest-axis conversion or angle units are introduced. The pin's
16-bit angle wrapping and float arithmetic are retained, including negative
and full-range int32 components.

The pinned `ExpressionEvaluator::RotatorToVector` and
`ExpressionValue::ToRotator` define the behavior at SurrealEngine revision
`677ee14c5b83486e6634687953779aafb7973ad6`. Nothing is zero rotation, whose
forward vector is (1,0,0), not the zero vector. Other typed values refuse.
Unlike the pin's raw generic-Struct pointer interpretation, arbitrary generic
Struct values are not reinterpreted as Rotator storage; the port's typed
Core.Rotator values already have the distinct Rotator kind.

The result is a detached Vector, not a writable alias. Normal interpreter
value validation, trace location and shared call-tree budgets/transaction
apply. Child or prior effects roll back if conversion or a later expression
fails. There is no automatic startup, world ticking or actor movement hookup.

## Verification

Strict standalone C++20/O3/Wall/Wextra/Werror compilation of the changed VM and
complete existing VM controls against the frozen buffered host archive returns
exit 0. The combined suite passes 676,652 checks and 159 deliberate refusals.

Controls compare exact float bits against the actual pinned Coords math for
all 65,536 yaw and pitch values, boundary/full-range components and 4,096
deterministic mixed rotations. Independent cardinal directions, roll behavior,
Nothing, child evaluation once, alias detachment, invalid scalar/object/vector/
generic-Struct operands, trace, whole-call rollback and non-lvalue results are
checked. Existing cast, struct, alias, state-program and budget controls remain.
Independent read-only review found no concrete conversion/transaction issue;
the new include paths were corrected to the pinned lowercase filename for
case-sensitive hosts.

The full reconfigured host suite now passes (43 ordinary tests, two optional
original-root integrations skipped by that invocation). Both separate buffered
original-data integrations and Android ARM64 compilation also pass. The
actual StartUp attempt now continues beyond InitializeHomeBase's conversion
and reaches InitializeInventory's unsupported Spawn278 PC253, with full
transaction rollback, not a spawned inventory or successful startup. Begun-play
state entry still needs AIEndEvent715.

Local logs and the isolated executable are under ignored
`artifacts/rotator-direction-20261009/`; full-batch evidence is under
`artifacts/player-controls-20261009/`. This VM change does not modify Quest saves.
