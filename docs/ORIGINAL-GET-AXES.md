# Original Core.GetAxes native229

Read-only first-Training melee prerequisite audit, 2026-10-24. This records the
owned native ABI and scalar operation, not an implemented binding or evidence
that the new attack diagnostic has reached it. No original DLL/EXE was loaded
or executed, and no existing compiler input or build was changed.

Owned `Core.dll` SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Owned `Core.u` SHA256:
`4FB22EE4E3A6868DFABB2B46B15FAD8E5094F5FDA5927A14FF11670A744EC36C`.
Addresses below are preferred PE32 static evidence locations, not portable
object layouts or an instruction to execute the commercial image.

## Declaration and native entry

The actual `Core.Object.GetAxes` has native229, flags`0x2401`, logical20 bytes,
and exactly these four UStruct.Children/Next fields in order:

| Field | Actual property | Flags | Constraint |
| --- | --- | --- | --- |
| A | StructProperty, dimension1 | `0x80` | Core.Object.Rotator |
| X | StructProperty, dimension1 | `0x180` | Core.Object.Vector |
| Y | StructProperty, dimension1 | `0x180` | Core.Object.Vector |
| Z | StructProperty, dimension1 | `0x180` | Core.Object.Vector |

It has no ReturnValue, optional parameter or local. Its normalized body is four
NativeParm tokens at logical0/5/10/15 referencing precisely those fields. The
flags identify a static/final native; native229 alone is not declaration
authority. The named export table entry1524 (`execGetAxes`) has RVA`2D06`,
thunk`10102D06` jumping to body`10139E90..1013A268`. Registration at
`1013A270..1013A2CE` installs native`0xE5` into`101F454C`, using the pointer
stored at exported `intUObjectexecGetAxes`, RVA`AA974`/VA`101AA974`.

## Evaluation, OUT destinations and copies

The wrapper evaluates all four expressions, once each, BEFORE any result is
written. Each Step invokes the opcode with `Frame.Object` from native frame+8,
not the receiver of an enclosing script Context. Frame.Code is advanced past
each opcode and then once over EndFunctionParms at`10139F66..10139F76`.

1. A Step at`10139EA3..10139EBB` writes to an uninitialized local12-byte Rotator
   at post-prologue SP+54. The input is already detached before evaluating X,
   Y or Z. An OUT expression that mutates the original A source must not change
   this captured A. No default Rotator constructor/zero initialization occurs.
2. Before each X/Y/Z Step, `GProperty`/address global`101E32F8` is explicitly
   cleared to0. The expression's result destination is a separate uninitialized
   12-byte temporary: X at SP+C0, Y at SP+D8, Z at SP+CC. If Step supplies a
   nonnull property address, that address is retained; otherwise the wrapper
   retains the corresponding temporary address. These are NOT output defaults.
3. X's retained pointer is selected at`10139EE2..10139EFC` and stored SP+24;
   Y's at`10139F20..10139F31` in EBP; Z's at`10139F55..10139F66` in EBX. All
   argument evaluation and all coordinate calculations finish before writes.
4. Final writes are X.xyz then Y.xyz then Z.xyz, raw32-bit copies at
   `1013A212..1013A25E`. All three sources are the completed local coordinate
   block; no OUT value is used to compute an axis. The native result pointer is
   unused, and the call has no value result.

Consequences for an adapter:

- A must be copied at its own Step, not loaded after later OUT expressions.
  NoWrite/Nothing on A leaves native undefined bytes; do not manufacture a
  zero rotation. A complete trusted12-byte null-Context fill is defined input;
  partial/unknown fills must refuse. All three input integers cover the full
  signed32 domain, including INT_MIN; there is no Float-angle coercion.
- OUT expressions still execute for side effects/address calculation, but their
  preexisting Vector contents are never native inputs. Do not eagerly read or
  finite-validate them before writing. A non-lvalue expression, including an
  explicit Nothing result, merely directs that output to disposable local
  storage. This does not permit omitting a required expression from the packet.
- Retain guarded lvalue capabilities through later argument side effects. Do
  not resolve the same expression again. Read prior values only when the
  transaction's actual write/undo machinery requires them, not to supply a
  fictitious native input default.
- Aliases X==Y, Y==Z or X==Y==Z are legitimate: later copies win in that order.
  Compute all axes detached before the first journaled output write. Partial
  output overlaps must follow the same order if the typed reference system can
  represent them; otherwise explicitly refuse rather than invent raw offsets.
- No UObject Actor/Level/possession/begun-play gate belongs to this Core static
  numeric wrapper. Native declaration authority and reference lifetime still do.

## Exact coordinate operation: inverse order, not forward Rotate

At`10139F79..10139F8B`, the wrapper copies all48 bytes of actual
`GMath.UnitCoords` from`101B3230` (GMath`101B3218`+18). The independently audited
original GMath constructor initializes positive-zero Origin and identity axes;
see ORIGINAL-BRUSH-COORDINATES.md. That initializer is evidence of canonical
values, not a claim that a native GMath instance exists in the portable process.

It then composes THREE coordinate matrices in this exact order:

| Stage | Origin | XAxis | YAxis | ZAxis | Compose call |
| --- | --- | --- | --- | --- | --- |
| Roll | `(0,0,0)` | `(1,0,0)` | `(0,cR,sR)` | `(0,-sR,cR)` | `1013A09D` |
| Pitch | `(0,0,0)` | `(cP,0,-sP)` | `(0,1,0)` | `(sP,0,cP)` | `1013A14C` |
| Yaw | `(0,0,0)` | `(cY,-sY,0)` | `(sY,cY,0)` | `(0,0,1)` | `1013A20D` |

Every zero in these constructed matrices is positive-zero. FCHS on the stored
table sine produces the explicit negative-sine components, retaining signed
zero. Full composition executes even for zero angles. There is NO transpose
call at the end and no shortcut using only a forward-vector formula.

This differs from the existing `RotateOriginalModelCoords` helper's FORWARD
yaw→pitch→roll matrices. Do not call that helper with A or simply negate A:
the order is different, and signed integer negation overflows at INT_MIN.
An exact pure helper can reuse the existing `ComposeOriginalModelCoords` for
these three separate inverse-order coordinate temporaries.

Actual native helpers used here:

- `101027F7`→`1011A710..1011A91E`: FCoords::operator*=(const FCoords&), called
  three times. Each supplied matrix is a separate temporary, not receiver alias.
- `10102860`→`10117690..101176A8`: FVector(Float,Float,Float), plain copied
  three32-bit words; used while building the pitch/yaw matrices.
- `10102AF4`→`10118990..101189F1`: FCoords constructor from Origin/X/Y/Z
  references; copies all twelve32-bit words in Origin/X/Y/Z order.

The known full FCoords composition transforms Origin first, then XAxis, YAxis,
ZAxis, storing/copying each entire Float3 before the next vector. Vector output
components are `(vx*axis.x + vy*axis.y) + vz*axis.z`, with retained x87 PC64
products/additions and a single final binary32 component store. Origin also
performs its documented point subtraction. Relevant final result Float stores
are `1011A787..1011A79F` (Origin), `1011A806..1011A81C` (X),
`1011A883..1011A899` (Y), `1011A900..1011A917` (Z). This is not a host-double
matrix multiply or algebraically regrouped closed-form Euler expression.

## Integer indexing and sine loads/spills

The wrapper consumes captured A at SP+54/+58/+5C as Pitch/Yaw/Roll respectively.
For each signed32 angle:

```
sinIndex = SAR(angle,2) & 0x3FFF
cosIndex = SAR(wrap32(angle + 0x4000),2) & 0x3FFF
```

Unsigned wrapping addition followed by `(bits>>2)&0x3FFF` is equivalent; there
is no clamp, modulus requiring a positive angle, radian conversion or integer
overflow UB. Table address is`101B32A4` = GMath+8C,16384 binary32 entries.

Roll at`10139F8D..1013A09D` loads the sine first, FCHS/Float-stores negative sine
at`10139FCB`; loads/stores cosine at`10139FCF..10139FE1`, then reloads cosine
and sine and Float-stores the other matrix components at`10139FF3` and
`1013A04C`. Pitch at`1013A0A2..1013A14C` loads/stores sine at
`1013A0C2..1013A0D6`, cosine at`1013A0DA..1013A0E8`, also copies their raw
words, then FCHS/Float-stores the negative sine argument at`1013A11F`. Yaw at
`1013A151..1013A20D` first reads cosine raw bits at`1013A170`, then sine raw
bits at`1013A189`; copies/reuses them for the matrix and FCHS/Float-stores
negative sine at`1013A1D8`. Table reads contain no arithmetic interpolation.

All active table inputs and coordinate arithmetic should use the existing
finite-value/PC64 profile. Rotator input itself is integer-only. No libm call,
Float-angle conversion, normalization, cardinal-zero correction or controller
coordinate conversion occurs. The existing explicit `OriginalSineProfile::Table()`
contains the audited original-instruction data profile, not a live original
process capture. ORIGINAL-SINE-PROFILE.md records all16384 words and limitations:
profile`retail-core-x87-fsin-intel-306a9-nearest-pc64-v1`, payload SHA256
`B8D756044134EFB1BBEDBE2B842138D6B8D8DC970EF1A69EDD0D03CB07ECE10C`.

## Minimal binding work, not implementation evidence

1. Add a small detached axes helper using explicit UnitCoords/table values and
   the inverse roll/pitch/yaw compositions above. Reuse PC64 Float-store math,
   not the pinned legacy RotatorToVector/libm formula. Test all angle residues,
   full32-bit wrap, mixed rotations, signed-zero and exact profile words.
2. Authenticate the real Core declaration, four canonical native12-byte
   Rotator/Vector layouts, flags, order/constraints, native flags/logical bytecode
   and no extra fields. The existing Core316 declaration adapter is a useful
   schema pattern, not permission to accept an arbitrary four-value packet.
3. Extend BOTH raw-native and declaration/direct VM paths for MIXED behavior:
   A uses the existing audited Copy/uninitialized-full-layout semantics at its
   own Step; X/Y/Z use address-only evaluation and retain aliases/discards, not
   ordinary Validate-mode reads. Whole-call SnapshotNativeInput would destroy
   OUT aliases, while CopyNativeOutTemporary would incorrectly read unused OUT
   values. Current MaterializeUninitializedNativeArgument also detaches its
   result, so it is suitable for A, NOT OUT aliases. Direct RunEvaluation's
   generic early Load of every incoming argument likewise needs this explicit
   mixed packet handling rather than a host-only handler after the damage.
4. A native-with-Execution adapter computes detached axes then writes actual
   captured OUT references X/Y/Z through `execution.WriteReference` in order,
   discarding genuinely non-lvalues. No public Execute/new root or Actor-only
   validation gate. The same shared write/retained/native-work budgets, first
   failure poisoning, state-local revision/lifetime guards and rollback apply.
5. Verify nested Context uses Frame.Object, A mutated by later OUT expressions,
   aliased/all-equal outputs, non-lvalue/NoWrite outputs, full versus partial
   null-Context A fills, every write cutoff, and later native failure undo.

Static original `DeusExWeapon.TraceFire` places GetAxes229 at logicalPC149 with
Pawn(Owner).ViewRotation and three local OUT Vectors. Its presence in bytecode
does NOT prove that the current attack probe has reached it: earlier original
Fire, sound or state work can still expose a different missing service. No
trace/hit/crate damage, completed attack cycle, live trigger or headset result
is claimed by this document.

## Reproduction (data/static disassembly only)

```powershell
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:/Steam/steamapps/common/Deus Ex' 'Core.Object.GetAxes'
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -d -Mintel --start-address=0x10139E90 --stop-address=0x1013A2D0 'D:/Steam/steamapps/common/Deus Ex/System/Core.dll'
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -d -Mintel --start-address=0x1011A710 --stop-address=0x1011A920 'D:/Steam/steamapps/common/Deus Ex/System/Core.dll'
```
