# First Training TriggerLight Tick: authored inputs and scalar IEEE gap

Read-only owned-data/static-code audit, 2026-10-26. This records why the
first Training dynamic phase authentically reaches `Engine.TriggerLight.Tick`
and why its zero ChangeTime is not permission to clamp a map field, preserve
an obsolete Disable mask, or skip the actor. Only external objdump/static
BinaryReader reads and the existing read-only script inspector were used.
No original DLL/EXE was loaded or executed, no compiler input was edited,
and no build or gameplay/device acceptance is established here.

## Owned inputs

| Input | SHA256 |
| --- | --- |
|Core.dll|8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4|
|Engine.u|1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA|
|00_Training.dx|BB66D95AAFC6AAE4179F8444E98661F3D5D27DAB06D13D7DDE7F4B5F4E24068D|

Preferred-image addresses below are static evidence, not portable layouts.
Export indexes are zero-based unless explicitly marked as references.

## Actual authored actor and class values

Independent raw package reads identify all four Training actors:

| Actor | Map export / reference | InitialState Name payload offset | Value |
| --- | --- | --- | --- |
|TriggerLight3|109 /110|66940|TriggerToggle|
|TriggerLight0|110 /111|67052|TriggerToggle|
|TriggerLight1|112 /113|67278|TriggerToggle|
|TriggerLight2|113 /114|67397|TriggerToggle|

All have ObjFlags `02070001` and a serialized stopped Class-backed stack:
function/state reference -106, live probe mask all ones, latent0, PC-1.
Their tagged properties contain no ChangeTime, Alpha, Direction or
bInitiallyOn override. Their InitialState is not absent or Auto.

Actual `Engine.TriggerLight` is Engine.u export5295/reference5296, payload
231 bytes at418277, ObjFlags `000F0004`, base631 (`Engine.Light`). Its
class default tags are only bStatic, bHidden, bMovable and RemoteRole.
ChangeTime, Alpha and Direction are scalar FloatProperty declarations on
TriggerLight, and bInitiallyOn is its BoolProperty. There is therefore no
parent-owned default for these newly declared fields: absent scalar defaults
are positive Float0 / false, not a guessed nonzero ChangeTime.

## BeginPlay Disable is genuinely superseded by SetInitialState

Actual BeginPlay starts with native118 `Disable('Tick')` at logicalPC0.
It then copies LightBrightness into InitialBrightness. Because the authored
bInitiallyOn is false, it assigns Alpha0 and Direction-1; its final ordinary
assignment sets DrawType0.

Later original Actor.SetInitialState invokes native113 GotoState with the
actor's explicit InitialState (`TriggerToggle`). The original Core
GotoState1012E8F0 body at1012EA1F..1012EA47 computes and stores:

```
liveProbeMask = (receiverClass.ProbeMask | selectedNode.ProbeMask)
                & selectedNode.IgnoreMask
```

This is an actual replacement of the current live mask, not an AND with the
old live mask and not persistence of earlier disabled names.

Independent owned Engine.u mask words:

| Node | Reference / payload offset | ProbeMask | IgnoreMask | StateFlags |
| --- | --- | --- | --- | --- |
|TriggerLight Class|5296 /418277|0000001000000000|FFFFFFFFFFFFFFFF|0|
|TriggerTurnsOn|5220 /346832|0000000000000040|FFFFFFFFFFFFFFFF|1|
|TriggerTurnsOff|5224 /347008|0000000000000040|FFFFFFFFFFFFFFFF|1|
|TriggerToggle|5228 /347229|0000000000000040|FFFFFFFFFFFFFFFF|1|
|TriggerControl|5232 /347397|00000000000000C0|FFFFFFFFFFFFFFFF|1|

Tick is probe bit36, set by the receiver Class. TriggerToggle does not ignore
it, so this genuine transition reenables Tick. All four listed States have
logical script length1 (`EX_Stop`) and labelTableOffsetFFFF. The runtime's
missing Begin-label observation is expected; it does not imply a failed
selection or authorize replacing the selected node. Current mask-reset
behavior agrees with these original instructions. This is not evidence of a
disabledNames, IsEnabled or ProcessEvent bug.

## Reached script arithmetic

Actual TriggerLight.Tick begins:

```
Alpha += (Direction * DeltaTime) / ChangeTime
if (Alpha > 1) { Alpha = 1; Disable('Tick'); ... }
else if (Alpha < 0) { Alpha = 0; Disable('Tick'); ... }
```

The first native172 division is at logicalPC6. With the actual false-branch
initialization, a positive finite frame delta and ChangeTime+0, the numerator
is negative and finite. Ordinary masked x87 division produces negative
infinity, not a normal finite Float. Native184 then stores negative infinity
in Alpha. The real Less comparison succeeds, after which the script itself
assigns Alpha0 and disables Tick. This describes source semantics, not a
successfully executed port frame. The current reported `Non-finite VM float`
boundary prevents reaching that original recovery branch.

## Independently resolved Core native bodies

All mappings below come from actual name/EAT tables followed by direct JMP
thunks, not inferred from a replacement engine.

| Operation | Name/EAT index | Export thunk | Actual body |
| --- | --- | --- | --- |
|AddEqual_FloatFloat, native184|1442|101024DC|10135AA0|
|Less_FloatFloat, native176|1583|10101979|101352A0|
|Greater_FloatFloat, native177|1540|1010340E|10135380|
|LessEqual_FloatFloat, native178|1578|101025FE|10135460|
|GreaterEqual_FloatFloat, native179|1534|10102EF5|10135540|
|EqualEqual_FloatFloat, native180|1505|10101E92|10135620|
|NotEqual_FloatFloat, native181|1621|10103184|10135700|
|EX_Let|1586|10102F95|1012FE40|

The parent audit independently resolves native172 Divide to name/EAT1493,
thunk10102C0C, body10134F50. Independent reinspection confirms FLD lhs at
10134F9C, unconditional FDIV rhs10134FA0, and FSTP Float Result10134FB0;
there is no zero-divisor or finite-result guard.

### AddEqual evaluation, stores and exceptional values

10135AA0 first evaluates the left reference, retaining its actual DWORD
address (falling back to the temporary if no lvalue), then evaluates the
right expression. It loads the spilled right Float at10135B05 and adds the
FRESH actual left Float at10135B09. This operand order matters for exact NaN
payload semantics and aliased side effects. It does not reuse a left value
captured before right-expression callbacks.

FST Float [left]10135B16 stores the rounded sum without popping; FSTP Float
[Result]10135B18 stores the still-retained PC64 sum. No finite, infinity or
overflow test intervenes. Finite plus either infinity retains that infinity;
opposite infinities produce masked-invalid NaN. Both Float destinations must
receive the original arithmetic result, not a clamped value or a substituted
successful no-op. Exact NaN payload selection requires its own numeric proof;
this audit does not declare one canonical NaN for every operand packet.

### Ordered and unordered comparison branch bits

The bodies use FCOMP lhs,rhs followed by FNSTSW, not C/C++ comparisons.
For unordered operands x87 sets C0=C2=C3=1. The literal tests therefore give:

| Native | Actual test and true condition | Result when unordered |
| --- | --- | --- |
|176 `<`|101352FE TEST AH,01; C0 set|true|
|177 `>`|101353DE TEST AH,41; C0 and C3 both clear|false|
|178 `<=`|101354BE TEST AH,41; either C0 or C3 set|true|
|179 `>=`|1013559E TEST AH,01; C0 clear|false|
|180 `==`|1013567E TEST AH,40; C3 set|true|
|181 `!=`|1013575E TEST AH,40; C3 clear|false|

These return ordinary DWORD Boolean0/1. In particular NaN `<` and `==` are
true in these exact original bodies, unlike ordinary C++ operators. Infinity
ordering is the normal extended-real order; signed zeros compare equal.

### Plain Float assignment and property copy

EX_Let1012FE40 evaluates its left operand to obtain the actual destination,
then forwards that pointer directly to the right expression at1012FEC3.
There is no Float-range admission, normalization, arithmetic or result write
of its own. Missing-LHS scratch/error handling is separate from this actual
valid-lvalue assignment path.

Actual UFloatProperty.CopySingleValue is name/EAT628, thunk1010338C to
10166740: MOV sourceDWORD then MOV destinationDWORD. CopyCompleteValue is
name/EAT618, thunk1010100F to10166750: the scalar ArrayDim1 path at
10166758..10166764 performs the same raw DWORD copy; the multi-element path
uses REP MOVSD. These copies preserve all IEEE payload bits.

The FloatConst opcode is a distinct service, not a raw-copy claim:
name/EAT1519, thunk1010201D to10130810, FLD Float literal10130817 and
FSTP Float destination10130823. Arithmetic loads can quiet a signaling NaN;
they must not be conflated with UFloatProperty's raw copying.

## Bounded implementation consequence

The required fix is shared original scalar Float behavior and its typed
value/reference/assignment boundaries, with caller-owned write/work/state
rollback. It is not a TriggerLight exception, a nonzero map override,
an invented permanent Disable, or a renderer lighting change. Reached
infinities must be usable by the following actual script comparisons and
assignments; unordered comparisons require the original branch semantics.

Vector/Rotator/struct envelopes, native movement arithmetic, saves and other
boundary policies need their own explicit scope. This evidence alone does
not authorize indiscriminately admitting malformed composite values or
publishing an unsupported frame. Actual Training retries, deterministic
repeats, failure-prefix rollback, static-source immutability and Quest
acceptance remain separate work.
