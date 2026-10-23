# Original Actor physicsRotation and fixedTurn

Read-only owned GOTY static audit, 2026-10-23. This completes the bounded
Actor rotation algorithm that was left partial in
[ORIGINAL-ACTOR-PHYSICS-EPILOGUE.md](ORIGINAL-ACTOR-PHYSICS-EPILOGUE.md).
It is evidence for a future original-runtime implementation, not evidence that
rotation, the full crate/loot/fragment cohort, startup or a headset build works.

## Sources and reproduction

Owned installation: `D:\Steam\steamapps\common\Deus Ex`. Rechecked SHA256:

- `System/Engine.dll`:
  `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- `System/Engine.u`:
  `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
- `System/Core.dll`:
  `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

No commercial DLL/EXE was executed or loaded. Static instructions were read
with the installed MinGW objdump; original metadata/script was read with our
package inspector. From the port directory, reproduction commands are:

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103CA1E0 --stop-address=0x103CA45D 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103C9DC0 --stop-address=0x103C9EDF 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10413652 --stop-address=0x10413658 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10115670 --stop-address=0x101156B9 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -p 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'artifacts\prerequisites-20261009\build\script_bytecode_inspect.exe' 'D:\Steam\steamapps\common\Deus Ex' 'Engine.Actor.EndedRotation'
```

Original Actor field-chain output is also retained in the ignored audit
artifact `artifacts/collision-hash-save-audit-20261018/engine-actor-properties.log`.
Native offsets below are corroborated by that chain and the original Actor
copy routine, not treated as portable reflected-memory offsets. The pinned
engine's different implementation is not the authority for this algorithm.

## Identities and entry from performPhysics

Original exported `AActor.physicsRotation` is export-table index2068,
thunk103010C8, body103CA1E0..103CA45C. `AActor.fixedTurn` is export-table
index2037, thunk10303B34, body103C9DC0..103C9EDE. These are native C++ methods,
not authored script functions to synthesize.

The caller's common performPhysics epilogue first tests low16 bits of
RotationRate.Pitch, then Yaw, then Roll (103C98ED..103C990A). Any nonzero
low16 value calls physicsRotation with the original whole deltaSeconds. An
upper16-only rate does not select the call. This caller gate is separate from
the complete physicsRotation body below: a standalone helper must not quietly
add that gate inside the callee or claim complete performPhysics by calling
only this helper. Original PendingTouch processing follows rotation.

Actual declaring fields are all scalar, owned by `Engine.Actor`:

| Field | Native evidence position | Original reflected schema |
| --- | --- | --- |
| Rotation | 120/124/128 | Core.StructProperty, flags0x23, Core.Object.Rotator |
| bFixedRotationDir | 1EC bit0x20 | Core.BoolProperty, flags0x21 |
| bRotateToDesired | 1EC bit0x40 | Core.BoolProperty, flags0x21 |
| RotationRate | 1FC/200/204 | Core.StructProperty, flags0x21, Core.Object.Rotator |
| DesiredRotation | 208/20C/210 | Core.StructProperty, flags0x21, Core.Object.Rotator |

Each Rotator is three signed32 INT channels, Pitch/Yaw/Roll in that order,
not floating degrees or a quaternion. The existing canonical Rotator schema
checker is relevant, but the binding must also prove exact declaring owner,
metaclass, dimension1, original flags, no inherited base-field alias and actual
canonical Core struct/member identities. A same-named subclass property is
not permission to reinterpret a different field.

## physicsRotation: exact bounded protocol

The original method returns void. There are no stores to RotationRate,
DesiredRotation or either rotation flag, no Physics-mode test and no
bDeleteMe test anywhere in this bounded body.

### Entry gates and captured current rotation

1. At103CA20A load actual packed actor word+1EC once. Capture bit0x40 as
   `initialRotateToDesired`. If it is zero, test bit0x20 from that same loaded
   word; when both are zero return immediately (103CA218..103CA21C).
   Neither deltaSeconds, rates, DesiredRotation nor current Rotation is then
   active input. Do not reject their inactive exceptional data first.
2. Only when the captured bit0x40 is nonzero, compare actual current Rotation
   with actual DesiredRotation as **raw signed32 bit patterns**, short-circuit
   Pitch, then Yaw, then Roll (103CA226..103CA254). If all three are exactly
   equal, return immediately without converting rates, moving or reporting
   EndedRotation. Equality modulo65536 is not this gate.
3. Read RotationRate.Roll as signed INT (103CA25A), begin its product with
   original Float deltaSeconds, and capture raw current Pitch/Yaw/Roll into
   local proposed rotation (103CA260..103CA278). Convert the Roll product
   first. Then read/convert RotationRate.Yaw, then RotationRate.Pitch
   (103CA280..103CA29C). All three conversions occur before any axis update.
   Current rotation is not reloaded as the source for each axis.

### Integer delta conversion is not a Float multiply/store

Each conversion is signed32 `FILD`, `FMUL Float(deltaSeconds)`, then a call to
10413652 with the product still on the x87 stack. The helper is a jump through
IAT1059A34C to imported **MSVCRT.dll `_ftol`**, not a nearest-integer Float
store or Core.appRound. The finite product uses original PC64 arithmetic and
is truncated toward zero to signed64; the caller consumes only its low32 EAX
bits as signed32 Delta. It does not require that the truncated result fit
signed32. For example, a finite result4294967296 produces Delta0, not overflow
or INT_MAX. INT32*Float32 has at most55 significant bits, so its finite product
needs no intervening Float32 rounding. A host expression that first converts
the rate to Float32 or spills the product to Float32 can differ.

A portable initial implementation may explicitly refuse a reached nonfinite
product or a truncation result outside signed64. This audit does not claim
portable equivalence for CRT exceptional conversion/indefinite results. Such
a refusal must propagate through the caller's journal. It must not clamp to
signed32, substitute0, or turn an inactive early-return path into a refusal.
The bounded original method does not impose positive deltaSeconds: finite
negative values and either signed zero must retain their ordinary arithmetic.
Any world-scheduler validation is a separate caller contract.

### Axis update order and fresh/captured fields

Updates are Yaw, Pitch, Roll, even though conversion order was Roll/Yaw/Pitch.
Only a nonzero converted Delta enters that axis's checks.

- **Yaw**,103CA2A1..103CA2CF: test captured `initialRotateToDesired`.
  If false, call fixedTurn regardless of DesiredYaw equality. If true, read
  actual DesiredYaw and compare it to captured proposed Yaw; equal skips the
  call. When calling, reread actual DesiredYaw for the argument. fixedTurn
  receives captured/proposed Yaw, actual DesiredYaw and converted Yaw Delta.
- **Pitch**,103CA2D5..103CA302: reread live bit0x40. If false, call fixedTurn;
  if true, compare actual DesiredPitch to captured proposed Pitch and skip only
  on equality. A reached call rereads actual DesiredPitch as its argument.
- **Roll**,103CA305..103CA32D: the same live-bit0x40/equality/reached-argument
  protocol for captured Roll and actual DesiredRoll.

Each actual fixedTurn call performs its own fresh packed-flag read as described
below. The body does not take one DesiredRotation snapshot for every use,
normalize all axes in advance, or substitute the initial bit0x40 for Pitch,
Roll and completion. A zero-Delta axis retains its original raw32 current
channel, including upper16 bits; fixedTurn's output, when reached, is low16.

### Rotation is applied only through original Level.MoveActor

At103CA32F..103CA34F compare proposed Pitch, then Yaw, then Roll with **fresh
actual** Rotation channels as raw32 values. If all match, skip the movement
call and continue to the completion test. Otherwise:

1. Construct the ordinary local hit packet with zero pointer/location/normal
   fields, Time Float1 and Item INT-1 (103CA355..103CA3C1).
2. Read the actual actor's Level at+AC, then call its original MoveActor virtual
   slot+88 with this source actor, zero translation `(0,0,0)`, proposed Rotator,
   that hit reference and four integer options all0 (103CA372..103CA3D7).
3. Ignore the returned boolean and hit packet. There is no direct Rotation
   property store, no fallback store when movement fails, and no shortcut that
   declares completion because the proposed rotation equals desired.

The existing original MoveLevelActor binding is the relevant service; it must
retain its real collision, child/support, touch/zone and synchronous callback
semantics in the same root. A false return is not an exception manufactured by
physicsRotation. Reentry may alter actual Rotation, DesiredRotation or flags.

### Completion callback is based on fresh actual state

At103CA3DD reread live bit0x40. When false, return. Otherwise compare actual
Rotation and actual DesiredRotation, fresh raw32 Pitch/Yaw/Roll, short-circuit
in that order (103CA3E6..103CA414). Only complete equality reaches:

1. Construct FName **0x145/325** and call imported Core.UObject.IsProbing at
   IAT1059A02C (103CA416..103CA424).
2. If false, return. This is the original probe25 (`325-300`), EndedRotation.
   Core's bounded10115670..101156B8 implementation returns true outside probe
   range300..363 or when no StateFrame exists; otherwise it tests the frame's
   **live positive uint64 mask** at+1C/+20. Do not freshly derive eligibility
   from class/state declaration masks when a live frame exists.
3. Load ENGINE_EndedRotation at105836DC, resolve the selected function on this
   actor and call actual ProcessEvent with null/no-parameter packet and null
   result (103CA42E..103CA447). This name address independently matches export
   ordinal931/export-table index930/RVA2836DC. Do not call an unconditional
   hand-picked empty base body or replace the original state-aware selector.

`Engine.Actor.EndedRotation` in owned Engine.u is native0, flags0x800,
logicalBytes2 (`Return Nothing`), with no child parameters, no return value,
next field Actor.InterpolateEnd and no base/source alias. This is the base
declaration, not a guarantee that a selected override is empty or has the same
event flag. Binding must authenticate the actual declaration and packet and
use normal real original selected ProcessEvent services/gates. The native
method adds no deletion shortcut before that call; normal ProcessEvent's own
eligibility rules remain separate.

There is no post-callback work, no clearing bRotateToDesired, no stopping
RotationRate and no completion latch. The method's initial raw-equality return
is significant: beginning a call already at the desired rotation does **not**
report EndedRotation again.

## fixedTurn: exact finite integer algorithm

Let `wrap32` denote original two's-complement32 add/subtract/negate, with the
result interpreted as signed32. All comparisons below are signed. `low16`
means retain bits0..15, yielding0..65535. No signed-overflow C++ expression
may substitute for these explicit wrapping operations.

1. If input Delta==0, return `low16(Current)` immediately; do not read flags
   or Desired (103C9DC1..103C9DD3).
2. Otherwise load one fresh actual word+1EC (103C9DDE), set C=low16(Current)
   and D=low16(Desired), and test bit0x20, then bit0x40 as reached from that
   **same loaded word**. No field writes or callbacks occur in fixedTurn.

### Fixed direction bit0x20 set

If bit0x40 is false, return `low16(wrap32(C+Delta))` directly
(103C9E3C..103C9E45). Desired has no further effect.

If bit0x40 is true:

- Delta>0,103C9DFC..103C9E1B: if C>D, temporarily add65536 to D. Let
  distance=D-C, in0..65535. Use `step=min_signed(Delta,distance)` and return
  `low16(wrap32(C+step))`.
- Delta<0,103C9E1E..103C9E39: let E=C+65536 if C<D, else C. Let
  distance=D-E, in-65535..0. Use `step=max_signed(Delta,distance)` and return
  `low16(wrap32(C+step))`. The addition uses original C, not E.

Thus direction follows Delta, not necessarily the shortest arc, and reaching
the desired low16 angle clamps that step. Equal low16 current/desired means
distance0, not a full extra revolution.

### Fixed direction bit0x20 clear

If bit0x40 is also clear, return C (103C9E48..103C9EDC).

Otherwise compute `magnitude = Delta<0 ? wrap32(-Delta) : Delta` on each reached
branch. In particular, INT_MIN negation remains INT_MIN. Do not saturate or use
a wider positive absolute value. Use signed minimum when choosing a step:

| Low16 relation | Direction/distance | Return |
| --- | --- | --- |
| C>D and C-D<32768 | negative, C-D | low16(wrap32(C-min_signed(distance,magnitude))) |
| C>D and C-D>=32768 | positive, D-C+65536 | low16(wrap32(C+min_signed(distance,magnitude))) |
| C<=D and D-C<32768 | positive, D-C | low16(wrap32(C+min_signed(distance,magnitude))) |
| C<=D and D-C>=32768 | negative, C-D+65536 | low16(wrap32(C-min_signed(distance,magnitude))) |

Addresses are103C9E51..103C9E76,103C9E79..103C9E96,
103C9E99..103C9EBA and103C9EBD..103C9EDC respectively. At an exact half-turn,
the chosen sign is positive when C>D and negative when C<=D. This preserves the
original asymmetric tie, not a guessed nearest-angle convention. A Delta of
INT_MIN selects the signed negative magnitude and produces the same low16 C
after wrapped add/subtract; it does not move toward D by2147483648 units in a
wider arithmetic domain.

## Binding and test requirements

A bounded coordinator needs actual flag-byte, signed channel and level/move
providers, plus live IsProbing and selected no-argument EndedRotation delivery.
A suitable detached boundary can represent the packed relevant flags as
`uint8_t` masks0x20/0x40, but it must preserve captured versus fresh reads above.
It must not conflate the generic bBounce/bit0x10 or bJustTeleported/bit0x100
fields with rotation flags. Rotator channels remain signed32, conversion
arithmetic must retain the original PC64 product and low32 CRT result, and
rotation writes belong exclusively to MoveActor and its existing root journal.

Required controls before binding/publishing include:

- Inactive early gates, including exceptional dt/rates never consumed; initial
  raw equality versus low16-only equality; both flags false, Fixed only,
  Rotate only and both flags; all three converted deltas zero.
- Original Roll/Yaw/Pitch conversion and Yaw/Pitch/Roll update order; captured
  Yaw flag versus live Pitch/Roll/fixedTurn/completion flags; desired read skips
  and fresh reached argument reads; current captures versus fresh movement and
  completion comparisons.
- Positive/negative/signed-zero dt, truncation toward zero, products that differ
  if Float32-spilled, representable signed64 results outside signed32, low32
  wrap, INT_MIN and finite out-of-signed64/nonfinite explicit refusal.
- All fixedTurn directions, overshoot clamps,0/65535 boundaries, exact32768
  half-turn ties, signed Delta extremes and raw upper16 input bits. An exhaustive
  sweep or independent instruction-level oracle for bounded integer inputs is
  stronger than comparing with a copied algebraic implementation.
- Proposal unchanged skips MoveActor; changed proposal passes exact zero
  translation/options0 and real hit seed; false MoveActor result is ignored;
  blocked/changed actual pose cannot be replaced by proposal; reentrant flags,
  desired/current changes and real support/child-carry effects.
- Completion after both moved and unchanged proposals; live probe off/on,
  absent-frame semantics, selected override/no-argument identity, begun/deletion
  rules delegated to real ProcessEvent, and no callback on initial equality.
- Work/read/callback failures at each reached service and every actual journaled
  write cut, with caller-owned whole-root rollback preserving state, support,
  random/audio effects, clocks and immutable owned source bytes.
- The real performPhysics low16-rate gate and its original subsequent
  PendingTouch protocol, followed by the actual first-Training interleaved
  upper-crate/Lockpick/WoodFragment scene. Isolated fixedTurn success is not
  acceptance of the whole scene or of automatic world scheduling.

No compiler inputs, build or test binaries were changed or run by this audit.
The body is now bounded enough for implementation; the runtime and actual
first-Training cohort still require that implementation and verification.
