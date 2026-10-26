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

## Scalar arithmetic operand order and IEEE follow-up

Read-only follow-up,2026-10-26. The same owned Core.dll hash above was
inspected with external objdump; no commercial binary was loaded/executed and
no compiler input/build was changed by this audit. Actual zero-based name/EAT
indexes, direct JMP thunks, bodies and final arithmetic order are:

| Native | Name index / thunk / body | Actual register-load and memory operation |
| --- | --- | --- |
|169 unary negation|1676 /101014D3 /10134CF0|FLD operand10134D20; FCHS10134D25; FSTP Float10134D27|
|171 Multiply|1605 /10101EFB /10134E80|FLD RHS10134ECC; FMUL LHS10134ED7; FSTP Float10134EE0|
|172 Divide|1493 /10102C0C /10134F50|FLD LHS10134F9C; FDIV RHS10134FA0; FSTP Float10134FB0|
|174 Add|1447 /101020CC /10135100|FLD RHS1013514C; FADD LHS10135157; FSTP Float10135160|
|175 Subtract|1674 /1010265D /101351D0|FLD LHS1013521C; FSUB RHS10135227; FSTP Float10135230|
|182 MultiplyEqual|1599 /101021F3 /101358E0|FLD RHS10135945; FMUL fresh LHS10135949; FST LHS10135956; FSTP Result10135958|
|183 DivideEqual|1489 /101010B4 /101359C0|FLD fresh LHS10135A25; FDIV RHS10135A27; FST LHS10135A36; FSTP Result10135A38|
|184 AddEqual|1442 /101024DC /10135AA0|FLD RHS10135B05; FADD fresh LHS10135B09; FST LHS10135B16; FSTP Result10135B18|
|185 SubtractEqual|1666 /10101645 /10135B80|FLD fresh LHS10135BE5; FSUB RHS10135BE7; FST LHS10135BF6; FSTP Result10135BF8|

Each ordinary binary wrapper initializes/copies its first Float temporary,
then initializes/copies its second Float temporary, before arithmetic. Later
RHS callbacks cannot retroactively alter the already copied ordinary LHS.
Compound wrappers instead capture the actual LHS address first (falling back
to the Step temporary if no lvalue), finish/copy RHS, then dereference the
fresh LHS for arithmetic. Their FST of LHS does not pop/reload ST0: the result
store consumes the same retained arithmetic value, not a callback-mutated
reference re-read. There is no finite/zero/result admission test in these
native bodies. This does not authenticate every unrelated scalar math native.

Intel's primary [SDM Volume1](https://cdrdv2-public.intel.com/874241/253665-090-sdm-vol-1.pdf),
section4.8.3.5/Table4-8, documents masked x87 NaN propagation: a quiet NaN wins
against a signaling NaN; two NaNs of the same kind choose the larger
significand; quieting sets the highest fraction bit. Tables4-3 and8-10 give
Float indefinite `FFC00000` for invalid0/0, infinity/infinity, infinity*0,
opposite-infinity addition and same-infinity subtraction. This is distinct
from SSE's operand-selection rule. The manual does not specify the opposite-
sign/equal-significand tie in that table; a positive-sign tie policy requires
the separate hardware-x87 oracle/profile evidence, not an invented SDM claim.

The explicit FLD precedes the binary operation and can already quiet its
signaling-NaN input. Therefore the helper must distinguish the loaded register
operand from the still-raw memory operand, including RHS-first Add/Multiply/
AddEqual/MultiplyEqual. It must not apply a commutative raw-input NaN chooser
or flip a NaN's sign merely by implementing finite subtraction as addition of
a negated RHS. FloatConst's FLD/FSTP quieting is likewise separate from raw
UFloatProperty copies; unary169 quiets through FLD before changing sign.

Ordinary PC64 precision is supported by owned appEnableFastMath
10102ECD to1016E050, which calls actual imported `_controlfp` through
102AB4A4 with precision mask30000. The initialization path calls its false
form at1016DEFA, selecting64-bit significand precision. These calls change
precision only; nearest-even rounding and masked exceptions are the explicit
ordinary portable/oracle profile unless separately established for the actual
CRT/launcher environment. No literal037F control word is written by those calls.

Finite Float operands' product/quotient exponent range fits the extended
register format. Float overflow or gradual underflow can occur at the final
FST/FSTP, which must produce the profile's signed infinity/subnormal/zero
rather than reuse an existing geometry-only Store32 that refuses overflow.
Existing finite PC64 Add/Multiply/Divide primitives can supply the retained
finite operation; scalar storage needs its own IEEE spill policy. Geometry,
unsafe Float-to-integer conversion, scheduling delta and native-layout
boundaries remain separately guarded. No arithmetic helper, payload oracle,
successful Training frame, checkpoint migration or headset acceptance is
claimed by this doc-only follow-up.

## Subsequent scalar implementation (2026-10-26)

The shared VM now uses quest_scalar_float.h for native169,171,172,174,175,
176..181 and182..185. These are explicit original scalar services, not a
TriggerLight-specific arithmetic override. Finite arithmetic uses integer
dyadics and explicit PC64/binary32 round-to-nearest-even spills, including
gradual underflow and signed infinity overflow. Raw-word classification,
FLD quieting, actual register/memory operand order, masked-invalid indefinite
and literal C0/C3 predicates avoid Android/SSE NaN-policy differences. The
opposite-sign equal-payload tie policy is separately checked by an independent
hardware-x87 corpus, not attributed to an unspecified Intel manual rule.

Original FloatConst quiets through its own FLD-equivalent load; property copies
retain signaling bits unchanged. Ordinary scalar arguments are detached by
their actual expression Step. Compounds retain the left address, copy the
right input, then read fresh left storage; the return is the arithmetic result,
not a later reread of the destination. An absent left lvalue uses the original
private temporary fallback; malformed writable non-Float aliases still fail.

The generic scalar finite ban is removed from VM value validation and runtime
state-budget measurement. Vector/CoreVector12, unsafe Float-to-Int/Byte,
frame deltas, native animation/timers/geometry/window services keep their
separate guards. Exceptional packets for unauthenticated transcendental/
selection natives still fail explicitly; this increment does not claim all
original Float math services are complete.

Scalar payloads also have a real typed checkpoint path: codec14 is selected
only when an active scalar Float is nonfinite. It preserves raw32 bits across
properties, nested Struct scalar fields, mutable defaults, frozen birth
defaults and state locals. Codec14 has an explicit parity-presence Boolean;
codec13's mandatory parity wire remains unchanged. The matching runtime
envelope is19, including the equipment wrapper's nested-version admission.
Legacy codecs1..13 retain their byte format and reject nonfinite scalar words;
native Vector/geometry/clock/AI/timer Float payloads remain separately finite.
Both codec passes reject a noncanonical14 without an active nonfinite scalar.

Initial direct verification: the independent owned-code x87 test passed303367
arithmetic/comparison controls; script-state codec9460 checks/7601 rejections
and state-frame codec1770/911 passed. The first production Scalar-Light run
passed arithmetic, stored intermediate observation, script recovery, raw NaN
save/reload and rollback controls before exposing an incorrect test-only finite
checkpoint version assumption. The finite fixture now explicitly supplies
native parity0, and obsolete VM finite-only scalar expectations were updated.
These test edits require the final build/retest; no finished Training frame,
live crate/loot/gravity or headset acceptance follows from initial results.

Final targeted verification passed303367 independent x87 arithmetic/comparison
controls,696190 VM checks/900 rejections (including107 new scalar checks/4
write-prefix refusals),9460 script-codec checks/7601 rejections and1770
state-frame-codec checks/911 rejections. Generated production scalar-Light
controls log4273 cumulative checks/87 refusals; the full driver exits0 after
Physics95384/113 and the final malformed-schema case. Real CDO integration
also verifies raw IEEE codec14/envelope19 and restores the exact old finite
codec3/envelope6 bytes. Equipment-wrapper15 and mismatched-envelope rejection
are tested; a native Mover-wrapper19 runtime result is not claimed.

Full desktop, Android-native and strict ARM64 syntax builds pass. The later
full-suite run exposed four old scalar-ingress expectations (two invocations
of one CDO test, a packet-seed negative test, and timer failure status). These
were updated with positive IEEE CDO coverage, a malformed nonfinite Vector
seed and an explicit assertion that nonfinite timer input reaches the native
finite-rate guard with zero writes. All four targeted regressions now pass.
VSize's separate finite-result boundary is also retained. The final whole
suite result is recorded in STATUS.md, not inferred from those targeted runs.

The actual first Training phase now gets past the reached TriggerLight.Tick
and stops at the genuine DeusEx.ScriptedPawn native override. Ordinary repeat2
is identical205999 instructions/15731 writes,54 slots/53 admitted calls/8 stasis
returns and14 Physics9 blocks. Paired original doors retain two56-unit moves
and2 completion callbacks, repeat2 identical206335/15799 before the same later
Pawn boundary. All roots verify full save/state/revision/hash/RNG/client/
lookup/Actor/GC undo and41 directly consumed original files unchanged.
No actor is filtered, no authored ChangeTime or probe mask is rewritten, and
no frame is published. See ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md for the
next real override and the separate still-missing Pawn/base/physics paths.
