# Original active Physics9 MovingBrush contract

Read-only owned GOTY audit, 2026-10-25. This extends
`ORIGINAL-MOVING-BRUSH-PHYSICS-PREREQUISITES.md`; it is not evidence that a
playable first Training frame, movement query, or APK has succeeded. Only
external objdump and the port's read-only package inspector were used. No
commercial DLL/EXE was loaded or executed.

Owned inputs:

- Engine.dll SHA256 `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- Core.dll SHA256 `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

Addresses below are preferred-image virtual addresses, not portable object
layout. Inner AActor.physMovingBrush is thunk10303238 to
103DE690..103DEA40; its Physics9 caller is103C9856..103C98D1.

## Entry and captured key

103DE6B8..103DE6DB follows actual actor Class/Super pointers for native AMover
UClass1058BB58. Failure returns before Mover fields; the outer caller still
performs its stores. Success zero-extends Byte+377 and captures a local
`min(KeyNum,8)`. The emitted lower bound0 cannot be reached by an ordinary
unsigned Byte. This never writes KeyNum. The local key is captured ONCE,
including across movement and event callbacks and loop reentry.

Each iteration first tests actor+1EC bit80 (bInterpolating). If clear, return.
Only if set, compare that iteration's Float delta with Double+0 at10429838;
nonpositive or unordered delta returns without alpha/rate/pose reads.
There is no deletion, Physics-byte, begun-play, or extra stasis gate here.

## Numeric notation and retained alpha

`E(op)` below denotes the original x87 arithmetic operation rounded at its
PC64 precision, not host Double and not a fused expression. `F(x)` denotes
an actual Float32 spill. Inputs read from Float fields already have Float32
precision. A bounded portable adapter may explicitly refuse exceptional
arithmetic in its caller's rollback root; it must not quietly replace it
with successful empty movement.

At103DE728..103DE76A, in this order:

1. Load current iteration delta; multiply by freshly read PhysRate(+218).
2. Add freshly read PhysAlpha(+214), retaining `a=E(E(dt*rate)+oldAlpha)`.
3. Compare retained `a` with exact Double1 at104298A0. Spill `F(a)` into a
   captured alpha local WITHOUT reloading it into the retained register.
4. If retained `a>1`, compute
   `dt=F(E(E(E(a-1)/E(a-FRESH PhysAlpha))*previousDt))`, in that order. The
   denominator rereads PhysAlpha. Replace retained `a` and the captured
   alpha with exact1 (the latter from Float1 at10429C50).
5. Otherwise (including unordered under original masked arithmetic), set the
   local delta raw bits to positive0. Keep retained `a` for interpolation.

Thus a retained value just below1 can spill to captured Float1: it consumes
all delta, uses the retained value for glide, but later completes according
to captured Float1. Conversely, an alpha just above1 must enter the residual
branch even if its initial Float spill rounds to1. Clamping an already
spilled Float or deriving residual delta from it changes this contract.

## Glide

103DE771 compares actual MoverGlideType Byte+375 with1. Every other Byte
uses `g=F(a)`. For1,103DE77A..103DE795 executes:

```
s = E(a*a)
t = E(s*Double3)
c = E(a*s)
d = E(c+c)
g = F(E(t-d))
```

Double3 is at10429890. This is the literal stack sequence for
`3*a*a-2*a*a*a`, including its intermediate rounding and final Float spill;
do not substitute arithmetic on the captured Float alpha.

The owned Engine.u Enum payload independently confirms value0
`MV_MoveByTime`, value1 `MV_GlideByTime`: zero-based export2934,
outer37, class reference-29, flags70004, payload9 bytes at163840;
base0, next811, count2, both compact names and all9 bytes consumed.

## Pose fields and native adjacency

The actual Engine.u declarations identify KeyPos and KeyRot as arrays of8
Vector/Rotator elements; BasePos/OldPos are scalar Vectors and BaseRot/OldRot
scalar Rotators, all flags0. Physical native facts corroborate array bounds
and adjacency independently of interpolation:

- Exported AMover default constructor10302545 goes to103AF6F0. It constructs
  exactly8 FVector objects from+3C8 at stride12 (103AF722..103AF739), then
  exactly8 FRotator objects from+428 at stride12 (103AF73B..103AF752).
- Engine IAT10599B14 is Core FVector default constructor;1059A080 is Core
  FRotator default constructor. These identities come from the import table,
  not the data values or a replacement engine's headers.
- Exported AMover assignment10303CFB to10330710 independently copies8
  triples from+3C8 (1033096F..10330998),8 from+428
  (1033099A..103309C9), then individual scalar triples from+488,+494,
  +4A0,+4AC,+4B8,+4C4 onward.
- Original Spawned103AF790..103AF7CA copies Location to+488 and Rotation to
  +4B8. The owned Mover.BeginPlay bytecode identifies the corresponding
  logical operations as BasePos+KeyPos and BaseRot+KeyRot. These facts
  correlate native+488/+4B8 with BasePos/BaseRot. This audit did not discover
  a separate property-name/native-offset registration table; that limitation
  must not be represented as independently observed registration evidence.

The consumed layout is:

| Operand | Native address, component order |
| --- | --- |
| KeyPos[k] |+3C8+12k: X,Y,Z|
| KeyRot[k] |+428+12k: Pitch,Yaw,Roll|
| BasePos / OldPos |+488 /+494: X,Y,Z|
| BaseRot / OldRot |+4B8 /+4C4: Pitch,Yaw,Roll|
| current Location |+114: X,Y,Z|

For the original local key8, KeyPos's address is EXACTLY+428: it reads the
three raw INT words of KeyRot[0] as Float bits. KeyRot's address is EXACTLY
+488: it reads the three raw Float words of BasePos as signed INT bits.
These are native adjacent-field aliases, not ninth authored array elements
and not numeric INT/Float conversions. A portable provider must implement
those explicit aliases or refuse that reached path; cap7, modulo8, invented
zero, and out-of-bounds access to a portable array are all incorrect.

## Rotation arithmetic and conversion

103DE7C3..103DE86D uses raw signed32 rotation units. Every native integer
ADD/SUB wraps modulo2^32; there is no shortest-arc or low16 normalization.
With the captured key, it forms `(KeyRot+BaseRot)-OldRot` for each channel.
Exact field read order is:

```
BaseRot.Roll, BaseRot.Yaw, KeyRot[k].Roll,
KeyRot[k].Pitch, KeyRot[k].Yaw, BaseRot.Pitch,
OldRot.Roll, OldRot.Pitch, OldRot.Yaw
```

Roll sum/subtraction precedes the other delta arithmetic; yaw and pitch
deltas are retained as native INT locals. It converts Roll, then Yaw, then
Pitch by signed INT FILD, multiply by spilled Float `g`, and call10413652.
That thunk jumps through1059A34C, the imported MSVCRT `_ftol`. The consumed
result is low32 EAX from signed64 truncation toward zero, not a Float32
product, signed32 range clamp, nearest rounding, or an angle normalization.
INT32*Float32 has at most55 significant bits, so its PC64 product is exact.
See `ORIGINAL-PHYSICS-ROTATION.md` for the existing independently bounded
conversion service and exceptional-conversion policy.

It then FRESHLY reads OldRot.Roll, OldRot.Yaw, OldRot.Pitch, adds the
corresponding converted steps modulo2^32, and constructs the proposed
Rotator in Pitch,Yaw,Roll order. Core import1059A034 is the three-INT
FRotator constructor: Core export-name83 RVA14F6 thunk101014F6 reaches
10118D80..10118D98 and only raw-copies the three arguments.

## Position arithmetic and actual imported subtraction

Each of the first four stages below computes Z, then Y, then X, with an
actual Float spill per component before its three-Float FVector constructor.
No stage is fused with its successor:

```
target     = F(KeyPos[k] + BasePos)       //103DE877..103DE8AA
difference = F(target - OldPos)          //103DE8B0..103DE8DA
scaled     = F(difference * g)           //103DE8E0..103DE901
position   = F(scaled + FRESH OldPos)    //103DE907..103DE934
```

The first stage reads BasePos.Z before KeyPos.Z, BasePos.Y before KeyPos.Y,
but KeyPos.X before BasePos.X. Fresh OldPos is read separately by component
in both its stages. Core import10599B28 is FVector(Float,Float,Float), export
name115 RVA2860 thunk10102860 to10117690..101176A8: only raw copies, no
extra arithmetic or normalization.

At103DE93A capture actual native XLevel(+AC). Then103DE954 invokes Engine
IAT1059A070, Core `FVector::operator-(const FVector&)`, with `position` as
this and actual current Location as argument. Exact independent mapping:
Core export-name444 RVA2969, thunk10102969 to10117780..101177A2.
It computes and Float-stores **X, then Y, then Z**:

```
translation.X = F(position.X - FRESH Location.X)
translation.Y = F(position.Y - FRESH Location.Y)
translation.Z = F(position.Z - FRESH Location.Z)
```

It does NOT use the earlier Z/Y/X order. Address101403C0 is padding in this
owned Core and is not this helper. Capturing a whole old Location earlier
would not preserve the original field-read ordering or callback authority.

## Hit seed, MoveActor, completion, and remainder

Each active iteration constructs a fresh hit output before target arithmetic
(103DE79C..103DE7F2): null next/actor, zero Location and Normal vectors,
null primitive, Float time1, item-1. At103DE999, invoke captured native
XLevel virtual+88 with the actual actor, by-value translation and proposed
Rotator, this hit output, and four zero integer options. This is real
ULevel.MoveActor, including its swept collision/callback behavior in the
same caller root; direct pose writes and presentation door motion are not
equivalent. `ORIGINAL-ACTOR-MOVE.md` owns that service's contract.

103DE99F..103DE9F9 then executes:

1. If MoveActor returns0, do NOT store PhysAlpha or dispatch InterpolateEnd.
   Reenter through103DE705 with the residual Float delta already computed.
   MoveActor's own permitted effects are not locally undone on a0 return.
2. On nonzero return, compare the captured Float alpha with Double1 and
   store its raw Float word into PhysAlpha(+214). This is not a fresh alpha
   field read; it overwrites a PhysAlpha mutation made inside MoveActor.
3. If captured Float alpha compares equal1, clear only bInterpolating bit80
   at+1EC BEFORE selecting/calling InterpolateEnd. Preserve other packed
   bits as observed at this fresh read. Original unordered comparison also
   sets the equality bit; bounded nonfinite arithmetic may instead refuse
   explicitly and must not manufacture normal finite behavior.
4. Resolve the InterpolateEnd FName from105836D8 through imported
   FindFunctionChecked (10599A88), then call actual ProcessEvent virtual+10
   with one Actor Other parameter initialized to None and null result.
   There is no separate probe/no-op substitute in this body.
5. Reenter103DE705. Fresh bInterpolating controls whether the loop runs;
   a callback may reenable it and update rate/alpha/poses. Positive residual
   delta is consumed by another iteration, with the ORIGINAL captured key.
   Otherwise the next gate returns. Do not return immediately after the
   first event, force another key, or erase residual delta on a failed Move.

The original loop does not introduce a retry cap. The portable service's
existing work/state/reference budgets must bound genuine repeated movement
or reentrant callbacks, with failure rolling back its entire caller root.

## Outer caller is still required

103C9856..103C9873 raw-copies Location.X/Y/Z to OldLocation.X/Y/Z, then
calls the above inner method with the original whole Float delta. On return
it FRESHLY reads Location and OldLocation. Its asymmetric sequence is:

```
zDiff = E(Location.Z - OldLocation.Z)     //retained PC64
yDiff = F(Location.Y - OldLocation.Y)
xDiff = F(Location.X - OldLocation.X)
recip = E(Double1 / originalWholeDelta)
fRecip = F(recip)                        //retain recip without reload
zVelocity = F(E(zDiff * recip))
yVelocity = F(E(yDiff * fRecip))
xVelocity = F(E(xDiff * fRecip))
```

Arithmetic Float spills occur X,Y,Z, with Velocity.X stored before the final
Z spill, then Velocity.Y and Z stores (103C98B7..103C98CE). No game callback
is between those arithmetic/stores. The inner's residual local delta never
replaces this original whole delta. Even an idle/nonMover inner return must
not skip this wrapper; zero delta and overflow/NaN cases require faithful
support or explicit same-root refusal. Original rotation/PendingTouch suffix
at103C98ED onward must still run after the whole Physics9 dispatch.

This document authenticates the static active operation, not completion of
its portable providers, collision reachability, selected script callbacks,
save/replay behavior, first Training visual tests, or a device installation.
