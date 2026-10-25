# Reached original Vector-to-Rotator conversion

Static-only owned Core.dll audit,2026-10-25. No commercial EXE/DLL is executed
or loaded. SHA256:
8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4.

The first real crowbar-to-crate hit reaches missing VM conversion0x50 in
DeusExWeapon.SpawnEffects131, converting actual HitNormal(0,0,1) for the
impact spawner orientation. This is VectorToRotator, NOT RotatorToBool.
Original script inspection and the pinned UStruct.h token table agree.

## Wrapper and math

Core export[1691],RVA329C, jumps to10136F90..10136FEF. The wrapper initializes
all three local Vector words to positive zero, Steps one operand using
FFrame.Object (not a changed context receiver), calls FVector.Rotation and
unconditionally copies all three resulting INT words to its destination.
NoWrite/partial-null-context behavior belongs to that initialized12-byte local
buffer; it must not load a stale returned reference or pass a generic Struct.

FVector.Rotation export[1161],RVA2888, jumps to10144390..10144408:

1. Copy X and Y Float words into Double arguments to appAtan2(Y,X).
2. appAtan2 at10123280..1012328A is FLD Y, FLD X, FPATAN, RET. It does not
   spill the returned angle to Float or Double before its caller multiplies it.
3. Multiply the retained angle by Double at10178050, then imported _ftol
   through1016EEC0 to produce Yaw.
4. Compute unspilled Y*Y and X*X, add, then STORE THE SUM TO DOUBLE. Call
   appSqrt at10123290..10123296 (FLD Double;FSQRT;RET), then STORE ROOT TO
   DOUBLE before appAtan2(Z,root). Z is promoted from Float to Double.
5. Multiply retained pitch angle by the same Double scale, then _ftol for Pitch.
6. Store Roll INT0. No angle normalization or sixteen-bit mask is applied.

The scale bytes at10178050 are `b9 5a 99 0e 1c 5f c4 40`, Double bits
40C45F1C0E995AB9 =10430.2191955274 (the65535-per-turn scale), NOT the
pinned Rotator::FromVector's float65536/(2*floatPI) scale. The pinned helper
also uses Float products/sqrt/atan2 and cannot be substituted as original math.
Finite axis inputs alone do not authorize a guessed general-angle implementation.

Logs:
training-hit-core-exports-20261025.log;
training-hit-vector-rotator-wrapper-20261025.log;
training-hit-vector-rotation-body-20261025.log;
training-hit-spawn-effects-script-20261025.log.

## Required controls / current limit

Implement the actual conversion's initialized buffer and Frame.Object behavior
in the VM, including ordinary Vector, Nothing/NoWrite and partial12-byte null
context stores without stale alias reload. Test all axis/signed-zero/quadrant
cases, true65535 scaling, near-quantization boundaries, Double sum/root spills,
same-root callback/write-cut rollback and genuine hit/away/second-swing runs.
Independent test-owned x87 instructions may serve as an oracle, but the original
binary must remain data only. The initial static audit did not implement the
conversion or prove an attack-after-hit, fatal break or impact effect.

## Portable conversion and admission certificate,2026-10-25

`native/quest_vector_to_rotator.h` now implements bounded general finite-vector
conversion. It retains exact original PC64 products/addition, Double XY-sum
storage, PC64 square root and Double root storage using integer dyadics. It
does not substitute host libm, Float arithmetic or the pinned65536 scale.

The original imports MSVCRT._ftol. Static disassembly of the installed32-bit
CRT corroborates RC=toward-zero,FISTP QWORD,restore-CW and low32 consumption;
this installed-CRT evidence is version-specific, not a supplied original CRT.
Finite Vector angles stay within signed32, so integer overflow is inactive.
Nonfinite active components explicitly refuse rather than emulate malformed
x87 exception/raw-memory behavior.

FPATAN is not specified as a unique correctly rounded result across CPUs.
[Intel SDM Vol1 section8.3.10](https://cdrdv2-public.intel.com/843827/253665-sdm-vol-1-dec-24.pdf)
gives Pentium-and-later nearest-even error below one extended-precision ulp.
The production helper computes a Q112 atan2 enclosure using128 bounded Taylor
terms after reduction to magnitude<=1/2, plus Machin's pi identity. Its fixed
products retain all224 bits in four limbs; fraction division is bounded binary
long division. Conservative software error65536 Q112 units encloses series,
ratio, pi and quadrant errors. The actual angle's binade adds the FPATAN error
allowance. Enclosing endpoints are monotonically rounded to PC64, multiplied
by the exact original Double scale with PC64 arithmetic, and truncated.
Only equal endpoint INTs are admitted. Ambiguous intervals explicitly refuse.

This certifies observable INTs under the documented numerical profile; it
does NOT claim unique hardware FPATAN words or complete all-finite admission.
Three concrete Float pairs that ordinary Double atan2 rounds incorrectly
remain accepted with yaw973,1313,5359. Three genuinely ambiguous directions
and their signed variants exercise honest refusal. Removing the remaining
rare admission gaps needs stronger original-CPU equivalence evidence, not
silently choosing a host-libm answer.

The VM0x50 wrapper copies its operand exactly once with FFrame.Object,
materializes the initialized+0 Vector for NoWrite and null clear prefixes0..12,
rejects wider clears, and returns a complete typed Rotator. The partial-prefix
rule is local to this initialized temporary, not a widening of generic stores.

Focused compiled results:

- Math153684 checks,56 portable cases,30723 independent test-owned x87 cases:
  30717 certified matches,six explicit ambiguous refusals,15 explicit safety/
  ambiguity controls. Original Double sum/root spills are compared bitwise.
- Explicit mock-host real-VM368 checks/85 cases/26 refusals: copied aliases,
  frame contexts, typed members, malformed operands and same-root write-cut/
  late/caught-failure rollback.
- Android native library link and strict ARM64 VM/new-test compilation pass.

Logs:vector-to-rotator-{math-test,vm-test,focused-build,android-build,
strict-arm64}-20261025.log in artifacts/. Owned-map attack evidence is separate;
these checks alone do not prove effects, a fatal crate, loot or Quest gameplay.
