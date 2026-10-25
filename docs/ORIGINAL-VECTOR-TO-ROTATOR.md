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
Precise active nonfinite/overflow policy and _ftol rounding/operand handling
still need complete verification before implementation. Finite axis inputs
alone do not authorize a guessed general-angle implementation.

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
binary must remain data only. No implementation or completed attack-after-hit,
fatal break or impact effect is claimed by this audit.
