# Original rotator addition: next first-Training startup dependency

2026-10-19. Complete static wrapper audit, implemented and verified below.
This is not successful world startup. Only the first Training map is in scope.
No commercial executable or DLL is loaded/executed.

## Identity

Owned Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Export1450 `UObject::execAdd_RotatorRotator`, RVA2B9E/thunk10102B9E, jumps
to the complete body `10139AD0..10139B43`. Static registration at
`10139B50..10139BAE` installs native13C/316 into GNatives+4F0.

Actual Core.u declares `Core.Object.Add_RotatorRotator`, native316, flags3401.
Its ordered scalar StructProperty packet has A and B, each canonical
`Core.Object.Rotator`, flags80, dimension1; ReturnValue is the same Rotator,
flags580, dimension1. The linked child order is A, B, ReturnValue. Original
Rotator is12 bytes, with three signed32 Pitch/Yaw/Roll channels; generic Struct,
Vector or a lookalike is not interchangeable with that authenticated layout.

Ignored static evidence:

- `artifacts/training-move-bridge-rotator-add-static-20261019.asm`
- `artifacts/training-move-bridge-rotator-add-schema-20261019.log`
- `artifacts/training-move-bridge-mover-beginplay-20261019.log`
- `artifacts/world-native-startup-20261011/core-exports-imports.log`

## Complete native body

The24-byte stack frame has two separate12-byte argument destinations. The
wrapper does not initialize either destination before evaluating its argument.
It Steps A into the first destination, then Steps B into the second destination,
advances Code over EndFunctionParms, and only then reads the two copied values.
A later B expression cannot retroactively change the already copied A through
a retained lvalue alias. No OUT parameter is present.

Each result channel is an ordinary32-bit ADD of the corresponding input words.
The result writes are Pitch, Yaw, Roll at offsets0/4/8. Overflow wraps modulo
2^32; there is no signed-overflow exception, angle normalization,16-bit mask,
floating-point conversion, saturation, trig call, state write or callback after
the argument Steps. An implementation must use unsigned-word addition and
bit-preserving conversion back to signed32, not undefined C++ signed overflow.

Unwritten input words are original undefined stack data, not initialized zero
or the current actor Rotation. Portable handling must reject dependence on an
unwritten/partially zero-filled Rotator unless all12 bytes are established by
the actual destination protocol. It must not add a builtin zero seed merely
because a reflected Rotator property's default is zero. Ordinary full-width
written Rotators include every possible signed32 channel value.

## Entry boundary (before this implementation)

The real first-map startup observation before this implementation was DeusExMover16,
Engine.Mover.BeginPlay PC197/opcode97, unsupported native316. Original bytecode
at PC195 calls SetRotation299, but first evaluates the native316 argument:
BaseRot + KeyRot[KeyNum]. Therefore this boundary does not prove299 entered.

The acceptance work below uses exact copied-input/defined-result protocol,
independent modulo-word tests (including mixed extremes), argument mutation/
alias-order controls, typed/NoWrite/partial-result refusals, ordinary VM limits
and late-root rollback. Loaded mover BaseRot/KeyRot, begun-play, possession or
success must not be forced to pass this expression.

## Runtime implementation

Native316 now performs three unsigned32-bit additions and bit-preserving signed
results in the VM. `CoreRotator12` validates canonical Rotator values without
restricting any integer channel word or normalizing angles. It is an ephemeral
VM result capability, not a new gameplay save format.

The VM intrinsically records both uninitialized12-byte input destinations even
for a generic Host that offers no capability hook. This prevents reflected
parameter zeros from accidentally seeding NoWrite. A conflicting Host layout
or initialized argument seed refuses. Exact full12-byte Context clear can
establish zero; NoWrite/Nothing, zero-size, partial and oversized clears cannot.

The real runtime independently authenticates native316's original Core UFunction
and ordered scalar A/B/ReturnValue declarations. Its canonical Rotator proof
checks actual Core.StructProperty/Core.Struct identities, owner/base/script and
the complete owned Pitch/Yaw/Roll IntProperty chain. Layout is per element, so
the original KeyRot[8] array is valid while each native parameter is dimension1.
The operator has no Actor, Level, hash, clock or begun-world admission.

Copied native arguments are detached before evaluating the next input. A
narrow VM Eval Copy/Address mode retains lvalue/element capabilities during
transparent Context/ClassContext/Skip and array/member address evaluation,
then reads the final source once. NoWrite/ZeroFill aliases are not consulted.
This avoids the older generic final-reference validation read followed by a
second read during native materialization; unrelated VM expression paths keep
their previous validation behavior. Array/member parents cannot manufacture
a native value from an unwritten address/result.
This is a transparent-wrapper/address guarantee, not a general VM operand
rewrite: casts, conversions and BoolVariable still descend through their
existing validation paths. No broader single-read/NoWrite guarantee for those
non-transparent wrappers is established by this checkpoint.

The narrow copied/address path also respects the original initialized temporary
destinations used to *select* an address. Static owned Core.dll inspection shows
ClassContext setting its selector pointer to None at1012EDD1/1012EDD8 before
Step1012EDEF; ordinary Context does the same at101300D9/101300E0 before
Step101300EE. ArrayElement initializes its index INT to0 at1012EFB2 before
Step1012EFD7. A provider's stale NoWrite/ZeroFill payload or attached lvalue must
not select a child object/class or a different array element. The copied path
materializes these initialized destinations before selection: NoWrite retains
None/0, exact4-byte clear yields None/0, and audited little-endian partial INT
clears retain the initialized0. Partial object-pointer clears and oversized
clears remain explicit portable refusals, not invented pointer reconstruction
or a claim about safe original malformed-bytecode behavior. These rules do not
initialize native316's distinct uninitialized12-byte Rotator operands.

The generated-host test passes151256 checks,17408 independent byte-carry
arithmetic cases,1661 copied-input/layout/protocol cases,1214 explicit refusals
and10 mock-host shared-root rollback cases. It covers four VM entry routes,
both native operand slots, copied-input mutation, nested address/selector/index
controls, typed/undefined results and ordinary VM budgets. The unchanged general
VM suite passes695763 checks/846 rejection controls. Generated capabilities are
not actual Core.u metadata proof.

The separate complete owned-data first-map suite passes1164487 checks/97
explicit refusals, including genuine Core.u scalar/result schema, original
KeyRot[8] element layout, modulo-word arithmetic, complete12-byte clears and14
partial/NoWrite refusals. Pure arithmetic leaves the full checkpoint, revision,
hash, GC and RNG unchanged and does not warm the Level Model. This includes
the real movement/collision/NPC-clock/save controls, not live VR physics.
Strict NDK27 ARM64 VM/runtime/unit/owned-test/observation syntax and Android
native compilation/link pass. No APK is packaged for this backend-only work.
The coherent full host build and shared CTest pass152 executed/176 registered
with24 explicit owned-data skips and zero failures in60.87s. Skips are not
actual-map evidence; the separate complete owned first-Training run above is.
The separate `portable_training_startup_probe` observation tool loads only
first Training and checks full rollback/source immutability; exit2 means an
observed refusal, not a passing startup. It does not replace the full suite.

## Repeated real first-map startup observation

The rebuilt observation tool reaches the same next boundary twice after exact
rollback: `PostBeginPlay`, native actor slot134, `00_Training.DeusExMover49`,
`Engine.Mover.FindTriggerActor` PC198/opcode49, with
`Native actor lifecycle work budget exceeded`. The two observations each execute
260346 VM instructions/8465 writes before complete rollback. All original
PreBeginPlay, BeginPlay, SetActorZone and LastRenderTime passes have completed,
with1333 visited live actors per pass; PostBeginPlay has133 visits. A visit
count is not the native slot-array length: holes are skipped before reporting
visits but still consume traversal work.

The diagnostic's hash image is explicitly an EARLY prefix:1308 source actors,
478 links, seed1885821919,49152 fixed initialization draws and2649 traversal
operations. That image does not expose the final work counter inside the
failed startup root. Full save/state/revision/hash/RNG/client/viewport/lookup/
Actor/GC rollback and all40 source files' byte/size/timestamp checks pass.
The CLI returns2, not a passing startup exit code. Ignored evidence is
`artifacts/training-rotator-add-startup-final-20261019.log`.

Read-only next-frontier inspection finds a genuine untagged AllActors scan in
the original FindTriggerActor, with a monotonic cursor and one native charge
per visited numeric slot. Mover.PostBeginPlay has separately authored scans;
there is no justified iterator reset or skipped-callback workaround. The shared
65536 native-work allowance remains unchanged here. A future explicit bounded
root allowance must also cover AI helpers that currently compare the SAME
counter against their own65536 constant, without inner resets or counter
rebasing. This is the next startup dependency, not completed gravity/destruction.

Ignored current verification logs beneath `artifacts/` include
`training-rotator-add-unit-selectors-20261019.log`,
`training-rotator-add-vm-final-20261019.log`,
`training-rotator-add-owned-final-20261019.log`,
`training-rotator-add-arm-selectors-20261019.log`,
`training-rotator-add-arm-runtime-final-20261019.log`,
`training-rotator-add-arm-unit-selectors-20261019.log`,
`training-rotator-add-arm-owned-final-20261019.log`,
`training-rotator-add-arm-observe-final-20261019.log` and
`training-rotator-add-android-selectors-20261019.log`.
Coherent build/regression logs are
`training-rotator-add-host-final-20261019.log` and
`training-rotator-add-ctest-final-20261019.log` in the same directory.
