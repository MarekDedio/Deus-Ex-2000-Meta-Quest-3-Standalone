# Original Physics9 MovingBrush prerequisites

Read-only owned GOTY audit,2026-10-25. This audits the Physics9 service after
the generic dispatch gap exposed by exact Engine.Mover admission; the live
mode/actor/interpolation flag still require capture as noted below. It adds
no physics provider, fallback, standalone transaction or playable frame claim.
Only first Training gameplay integration is prioritized.

## Static source and exact dispatch

Owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Only external objdump/static bytes and the port's read-only package inspector
were used. No original DLL/EXE was loaded or executed.

AActor.performPhysics thunk10303242 jumps to103C9770. Its table103C99A4 is
indexed by Physics-2. Index7, Physics9, contains103C9856. That whole dispatch
block is103C9856..103C98D1; it calls thunk10303238 at103C987C. Export-name/EAT
index2060 identifies that thunk as AActor.physMovingBrush; its JMP reaches
103DE690..103DEA40. This is not an AMover-specific virtual Tick body.

The outer block copies Location into OldLocation BEFORE the inner call and
derives Velocity AFTER it. Even an actual inner early return is therefore not
a successful empty whole-dispatch provider. Original rotation/PendingTouch
work follows at103C98ED onward; it must still run after the dispatch returns.

## Bounded inner entry and no-interpolation paths

1.103DE6B8..103DE6DB follows actual source Class/Super pointers for the exact
 native AMover UClass1058BB58. Independent export index1373 is
 `AMover.PrivateStaticClass`, RVA28BB58. A failed cast exits at103DEA2E, before
 reading Mover-only fields. It does not skip the caller's outer stores.
2. For a successful cast,103DE6E1..103DE703 zero-extends actual Mover.KeyNum
 Byte(+377) and captures a local clamped to0..8. The lower-than-zero branch is
 unreachable for an ordinary unsigned Byte. The stores target EBP-20, NOT the
 native KeyNum field. Do not normalize/write the original field or replace
 the source's local upper bound8 with7. Active index8 needs separate exact
 treatment: the declared KeyPos/KeyRot arrays each contain only8 elements.
3.103DE707 tests Actor+1EC bit80, actual Actor.bInterpolating. Clear exits to
 103DEA2E. There is no PhysAlpha/PhysRate, glide, key-array, target-pose, Move
 or InterpolateEnd operation on this path.
4. Only when interpolating,103DE714..103DE722 compares supplied Float delta to
 Double0 at10429838. Nonpositive or unordered exits. This constant is exactly
 positive zero, not a guessed small time threshold.

Thus a faithful idle/no-interpolation block can be established from actual
class/KeyNum/flag authority, but it must still execute the entire surrounding
OldLocation/Velocity contract below. A blanket no-op for every Physics9 actor
would not implement this source.

## Outer raw copies and Velocity spills

At103C9856..103C9873 the original reads/copies Location.X, Y, Z raw DWORDs into
OldLocation.X, Y, Z in that order. It then calls the inner method with the
original Float delta. Post-call subtraction reads FRESH Location and
OldLocation fields; an active callback/movement path must not substitute a
stale local vector because the outer prefix once copied it.

The velocity sequence103C9881..103C98CE is asymmetric:

- Load/subtract Z and retain its difference in x87 PC64.
- Load/subtract Y, FSTP Float to local EBP-1C.
- Load/subtract X, FSTP Float to local EBP-18.
- Compute Double1 / Float(delta), FST Float to EBP-14 WITHOUT popping/reloading
  that retained PC64 reciprocal. Multiply retained Z difference by that PC64
  reciprocal. Reload the spilled Float reciprocal separately for Y and X and
  multiply their spilled Float differences.
- Spill the resulting X, Y, Z to Float. Native destination stores are
  Velocity.X, Y, Z. The first X destination store occurs before the final Z
  Float spill; no game callback occurs between these arithmetic/native stores.

For ordinary positive finite actor delta with finite reciprocal spills and an
unchanged finite pose, the idle result is zero Velocity. That observation is
not permission to hard-code zero for every packet. At delta0 the inner exits,
but the outer reciprocal/divide still executes; zero displacement multiplied
by infinity is invalid/NaN under the original masked x87 arithmetic. Extremely
small positive delta can overflow the Float reciprocal used for X/Y even when
the retained PC64 reciprocal used for Z remains finite. A bounded adapter must
faithfully implement or explicitly refuse unsupported exceptional arithmetic
in the same rollback root, not silently turn such cases into a benign no-op.

## Confirmed authored declarations

The existing script_bytecode_inspect executable reads these actual Engine.u
declarations, not runtime instance offsets guessed as portable memory layout:

| Exact field | Kind/dimension | Flags | Referenced type |
| --- | --- | --- | --- |
| Engine.Actor.Location | Vector/1 |23|Core.Object.Vector|
| Engine.Actor.OldLocation | Vector/1 |2|Core.Object.Vector|
| Engine.Actor.Velocity | Vector/1 |21|Core.Object.Vector|
| Engine.Actor.bInterpolating | Bool/1 |0|None|
| Engine.Actor.PhysAlpha | Float/1 |0|None|
| Engine.Actor.PhysRate | Float/1 |0|None|
| Engine.Mover.KeyNum | Byte/1 |1|None|
| Engine.Mover.MoverGlideType | Byte/1 |1|Mover.EMoverGlideType|
| Engine.Mover.KeyPos / KeyRot | Vector / Rotator,8 |0|Core.Object.Vector / Rotator|
| Engine.Mover.BasePos / OldPos | Vector/1 |0|Core.Object.Vector|
| Engine.Mover.BaseRot / OldRot | Rotator/1 |0|Core.Object.Rotator|

Flags in this table are hexadecimal. Actor.bInterpolating's native mask is
corroborated by the original Actor copy/Bool packing audit, not inferred from
its serialized property offset. Engine.Actor.InterpolateEnd is native0,
flags800, with one required scalar Actor Other parameter, flags80.

## Active movement remains separate required work

After the gates,103DE728 onward consumes PhysAlpha/PhysRate, retained PC64
alpha comparisons, Float residual-delta and alpha spills, MoverGlideType1
smooth interpolation, key/base/old pose arithmetic and float-to-int rotation
conversion. It invokes actual Level.MoveActor virtual+88 at103DE999 with the
real by-value translation/rotation, hit output and four zero options.

Success stores captured PhysAlpha; alpha1 clears bInterpolating before selected
InterpolateEnd(None). The code then loops through103DE705/707 with residual
delta and fresh flag/alpha/pose reads. The local clamped KeyNum was captured
before that loop, not automatically reselected after the callback. Failed Move
also returns through the loop/gates without an invented successful alpha store.
This audit does not authenticate all active interpolation arithmetic or its
callbacks sufficiently to implement that branch by analogy to the port's
separate presentation door motion. Active movement must remain explicit
unsupported until those real services and rollback controls are implemented.

## Existing same-root integration point

`portable_actor_perform_physics.inl::RetailPerformActorPhysics` builds the
actual production queries and currently leaves q.otherPhysicsBlock absent.
`quest_actor_perform_physics.h` selects that provider for Physics6/7/8/9/11.
The Physics9 implementation belongs inside that current host/Execution:
typed native-field reads/stores, existing Touch/reference/work/state budgets,
then the existing rotation/PendingTouch tail. It must not open another public
transaction, call the presentation mover clock, erase actors or skip Physics9.
Other reached unimplemented modes remain refused, not successful empty cases.

No implementation/build/test was performed by this audit. The actual Training
observation's generic missing-provider error identifies a reached dispatch
gap, but does not alone record its exact actor, Physics byte or interpolation
flag. Capture those live inputs when joining this service; do not infer that
every Training mover is idle from class defaults alone.
