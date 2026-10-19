# Original rotator addition: next first-Training startup dependency

2026-10-19. This is a complete static wrapper audit, not an implemented native
or evidence of successful world startup. Only the first Training map is in
scope. No commercial executable or DLL is loaded/executed.

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

## Actual boundary and next verification

The current real first-map startup observation is DeusExMover16,
Engine.Mover.BeginPlay PC197/opcode97, unsupported native316. Original bytecode
at PC195 calls SetRotation299, but first evaluates the native316 argument:
BaseRot + KeyRot[KeyNum]. Therefore this boundary does not prove299 entered.

Before advancing that pin, implement the exact copied-input/defined-result
protocol, independent modulo-word tests (including mixed extremes), argument
mutation/alias-order controls, typed/NoWrite/partial-result refusals, ordinary
VM limits and late-root rollback. Then run the real first-map startup again
and inspect its actual next boundary; do not force loaded mover BaseRot/KeyRot,
begun-play, possession or success to get past this expression.
