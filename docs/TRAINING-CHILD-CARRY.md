# First Training: original based-child carrying (2026-10-19)

This implements the complete ordinary **based-child segment** of original
MoveActor, not the whole movement operation. It is needed for first Training's
real movement/support behavior. It does not simulate falling, publish a crate
move, provide usable loot, start a world or implement a native266 shortcut.
Other campaign-map integration remains deferred.

The complete static source audit is ORIGINAL-CHILD-MOVE-CARRY.md, based on
owned Engine `103998A0..10399BB5` inside MoveActor. Original source hashes and
the complete disassembly/provenance are recorded there. No commercial code is
loaded or executed by the port or its tests.

## Implementation

`native/quest_actor_child_carry.h` supplies the detached live-provider protocol:

- Read StandingCount once; zero skips all child/list/pose/math work. Nonzero
  bTest bypasses this scan and reports the original parent-flow branch. The
  result distinguishes that bypass from no dependants and a completed scan.
- Resample the original native Level's signed Actor count on every iteration.
  Only an in-range slot is read. Null slots skip Base. Retain each nonnull child
  whose current Base exactly equals the source; no extra self/deletion/Owner/
  class/mobility/collision filters are invented.
- Sample source Yaw then Pitch per child. Preserve a raw wrapping32 yaw delta
  for child/ViewRotation additions. Roll is read only when Pitch and Yaw both
  compare equal. Raw equality is not substituted by sine-table phase equality.
- For active rotational math, quantize **each** yaw operand with0000FFFC
  before subtraction, not the raw difference. Read source/child Location in
  interleaved Z/Y/X order, retain differences, load the math-root pointer, then
  Float-store X/Y/Z offsets before copying UnitCoords.
- Apply complete FCoords*=FRotator to the actual supplied UnitCoords, including
  zero Pitch/Roll stages. The new public RotateOriginalModelCoords wrapper
  exposes the existing verified value service without prepending a Location
  translation or replacing UnitCoords with identity.
- Vector transformation retains each row's `(X+Y)+Z` arithmetic until final
  Float stores; it does not use PointBy or consume coordinate Origin there.
  Offset-minus-transformed differences retain their separate Float boundaries.
- Initialize a fresh complete child hit (Next/Actor/Primitive0, Location/Normal
  positive zero, Time1, Item-1). Read current child Roll/Yaw/Pitch after the
  offset work. Add the saved raw yaw difference with exact wrapping32.
- Build translation with individual Z/Y/X Float additions even on the
  no-rotation branch. A raw copy would incorrectly retain negative-zero words.
- Call a **complete recursive MoveActor** on the retained original Level with
  options0,0,1,0. Both the raw integer result and the entire returned hit are
  ignored by this segment. The caller's actual hit reference is preserved by
  forwarding, not silently copied into the provider.
- After the recursive call, classify the **retained child** as Pawn. Do not
  reload its possibly replaced slot or Base. For a Pawn, read live ViewRotation
  Yaw/Pitch/Roll and write Pitch/Yaw/Roll, adding the saved raw yaw delta rather
  than recomputing it from a callback-mutated parent pose.

The caller supplies the explicit original sine numeric profile; no libm
rotation, cardinal-zero repair, host graphics matrix or runtime table guess is
used. ORIGINAL-SINE-PROFILE.md documents that generated profile's measured
CPU/control-word limits; it is not a captured live original GMath object.

The detached operation charges the caller's shared work hook before every
ordinary provider invocation. It bounds cumulative live slot visits without
snapshotting the initial count, truncating successful scans or deduplicating
appended actors. Self-Base cycles or endless appends refuse the shared root.
Active nonfinite/overflowing arithmetic, unsafe/null math identity or missing
active provider also refuse explicitly. Inactive table/pose/Level branches are
not eagerly validated. These safety bounds are portable policies, not claims
about original unchecked-memory or floating-error handling.

## Verification and honest boundary

The independent test has its own provider interpreter, wrapping/modulo-angle
arithmetic and hardware-x87 operations. It does not call production carry or
coordinate helpers for expected values. It regenerates the whole16384-Float
table independently with FILD/FMUL/Double-store/FSIN/Float-store, compares the
explicit profile and preserves the measured nonzero half-turn residual.
Local hardware identity/profile limitations remain as documented above.

Test controls include:

- all selected raw full-range/low-bit/high-word angle combinations;
- inactive Roll/Location/math/table branches and any-nonzero bTest;
- source rotation/position changes between recursive child calls;
- replaced current slot/Base and later live appends/removals/class changes;
- fresh full hit seeds for every child and poisoned ignored result packets;
- raw native options/Level selection and the actual live hit reference;
- nonidentity UnitCoords, active nonfinite origin in rotation construction,
  inactive poisoned Origin in the inline vector transform, and all offset
  overflow boundaries around math-root load versus UnitCoords copy;
- endless appends bounded by cumulative slot visits and self-Base recursion
  bounded by the same shared work allowance;
- every provider/work failure prefix on the selected complete trace;
- independent row arithmetic and random complete child packets;
- an explicitly **MOCK** enclosing-root restoration control after real fixture
  list/view/source mutations. It is not an actual VM/hash transaction proof.

Final expanded result:140257 checks/115 explicit refusals,3884 independent
interpreter cases,104 provider/work prefixes,12000 row-arithmetic comparisons
and67 callback-mutation executions across production/oracle/mock controls.
Log: `artifacts/training-child-carry-unit-verified-20261019.log`.

The production runtime currently includes the header for build/type admission
only. There is **no real child MoveActor provider wired through native266** in
this checkpoint. All recursive movement in the standalone test is explicitly
MOCK. A provider that only writes Location, assumes an unobstructed move, or
silently accepts zero translation would not satisfy the original operation.
Complete original parent movement, actual native field/list authority, same-root
events/hash updates, startup/support/falling and playable loot remain required.

Coherent host build, strict ARM64 runtime/test syntax and Android native
compilation/link pass. Final CTest:148 executed/172 registered,24 explicit
owned-data skips, no failures in63.17s.

The separately supplied owned first-map regression passes1090740 checks/77
explicit refusals with38 production script packages,1308 actual native-slot
actors,120 collidable/nondeleted actors,478 registration links and94 actual
primitive-provider calls. It proves the existing first-map runtime/zone/hash/
touching/clock/save/revision/lookup/client/GC/RNG rollback and source-immutability
regressions still pass after rebuilding the shared coordinate consumers. It
does **not** invoke the new carrying segment through a real MoveActor provider.
Actual startup remains at BeginPlay slot126, `00_Training.DeusExMover16`,
Engine.Mover.BeginPlay PC166/opcode97, unsupported native266. Those results
must not be conflated with default owned-data skips, completion of movement,
playable first-map crates or headset validation.

Final ignored evidence logs:

- `artifacts/training-child-carry-unit-verified-20261019.log`
- `artifacts/training-child-carry-host-final-20261019.log`
- `artifacts/training-child-carry-arm-runtime-final-20261019.log`
- `artifacts/training-child-carry-arm-test-final-20261019.log`
- `artifacts/training-child-carry-android-20261019.log`
- `artifacts/training-child-carry-ctest-20261019.log`
- `artifacts/training-child-carry-owned-20261019.log`

No new APK is packaged for this detached prerequisite.
