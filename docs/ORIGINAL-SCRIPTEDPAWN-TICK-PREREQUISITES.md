# Original ScriptedPawn Tick: a genuine native override

Read-only prerequisite audit, 2026-10-26. The immediate target remains the
entire first `Maps/00_Training.dx`, not other-map integration. The current
unfiltered Training diagnostic reaches the exact native-class boundary
`DeusEx.ScriptedPawn` after the separate scalar Float implementation passes
`TriggerLight.Tick`. That observation is not proof that NPC Tick, AI or Pawn
physics works.

This audit does **not** authorize adding ScriptedPawn to the exact-AActor
native-virtual allowlist. Its original Tick and physics slots are different.
No compiler input is changed by this document.

## Evidence and scope

No commercial DLL or EXE was loaded or executed. External objdump reads owned
PE32 bytes; an independent read-only package-table parser reads the owned
`.u` files. Addresses are preferred-image VAs, not portable layouts or live
process pointers. Owned inputs are the same GOTY installation as the preceding
Actor/Level audits:

| File | SHA256 |
| --- | --- |
| `System/DeusEx.dll` | `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04` |
| `System/DeusEx.u` | `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480` |
| `System/Engine.dll` | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| `System/Engine.u` | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Read alongside `ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md`,
`ORIGINAL-LEVEL-DYNAMIC-TICK-PHASE.md`, `ORIGINAL-NONPAWN-TICK-TAIL.md`,
`ORIGINAL-TOWORLD-VTABLES.md` and `AI-NATIVE-AUDIT.md`. In particular, the older
ToWorld slot census does not prove Tick-slot equivalence.

## Actual Class ancestry, constructor and vtable identity

The original package export records independently establish the native
boundary, not a class-name guess:

| Owned package | Object ref | Class export | Super ref/path | UObject flags | Serial offset/size |
| --- | --- | --- | --- | --- | --- |
| `DeusEx.u` | 5 | `ScriptedPawn` | -8 / `Engine.Pawn` | `040F0004` | 269857 / 6966 |
| `Engine.u` | 1 | `Pawn` | 2 / `Engine.Actor` | `040F0004` | 58581 / 2644 |
| `Engine.u` | 2 | `Actor` | -3 / `Core.Object` | `040F0004` | 61225 / 1236 |

Each selected export has Class reference zero and the native UObject flag
`04000000`. The first five raw compact links in those Class payloads are,
respectively, `-8,0,18725,18958,209`, `2,0,5397,3573,12`, and
`-3,0,5536,4084,456`. Class payloads start with the UField links; they must not
be parsed as if they started with a property-stream `None` terminator.

DeusEx.dll export-name index10 identifies the default AScriptedPawn
constructor, RVA44D0. Its body is `100044D0..100045B0`, and the literal store
at `1000459B` installs vptr `10027624`. Export-name index176 identifies
InternalConstructor at `100044C0`; its non-null path jumps to that same default
constructor at `100044C8`. The exported vtable itself is export-name index101,
RVA27624. The corresponding exported PrivateStaticClass is index197,
RVA3C020. These are zero-based export-name indexes, not ordinals.

## The four requested native slots

| Service / offset | Actual ScriptedPawn vtable cell | Stored target and authenticated destination |
| --- | --- | --- |
| ProcessEvent +10 | `10027634` | `1001AF98` -> IAT `10027268`, imported `AActor::ProcessEvent` |
| ProcessState +14 | `10027638` | `1001AF92` -> IAT `10027264`, imported `AActor::ProcessState` |
| Tick +64 | `10027688` | **`100195A0`, local `AScriptedPawn::Tick`** |
| performPhysics +8C | `100276B0` | **`1001AF14` -> IAT `10027210`, imported `APawn::performPhysics`** |

The actual import descriptors authenticate the destinations rather than relying
on disassembler nearest-export names:

| IAT VA | Hint/name RVA | Engine import hint | Decorated symbol |
| --- | --- | --- | --- |
| `10027268` | 2CFC6 | 1441 | `?ProcessEvent@AActor@@UAEXPAVUFunction@@PAX1@Z` |
| `10027264` | 2CFA6 | 1443 | `?ProcessState@AActor@@UAEXM@Z` |
| `10027210` | 2CC6A | 2057 | `?performPhysics@APawn@@UAEXM@Z` |
| `10027238` | 2CDBA | 1667 | `?Tick@AActor@@UAEHMW4ELevelTick@@@Z` |
| `100273E4` | 2DD36 | 1131 | `?InStasis@AActor@@QAEHXZ` |
| `100273E8` | 2DD52 | 1378 | `?PrivateStaticClass@APlayerPawn@@0VUClass@@A` |

Engine.dll's actual APawn vtable is `10426994`, exported name662. Its Tick
slot +64 is `10303DAA`, the ordinary AActor.Tick thunk; its physics slot +8C
is `10302860`, exported `APawn::performPhysics`, jumping to body `103C9A70`.
That is **not** the AActor physics thunk/body `10303242 -> 103C9770`.

For comparison, the authenticated AActor event/state bodies are
`10302D74 -> 10369100` and `10301F00 -> 103E5B60`, and Actor Tick is
`10303DAA -> 103A1AA0`. Therefore event/state aliases do not prove either
ScriptedPawn Tick or Pawn physics equivalence.

## Bounded native Tick control-flow findings

Export-name index235 gives `?Tick@AScriptedPawn@@UAEHMW4ELevelTick@@@Z`,
RVA195A0. Its ordinary body is `100195A0..10019DF5`, not an empty forwarding
thunk. The following observed instructions are sufficient to reject an Actor
substitution; this is **not** a full portable implementation contract for every
branch of that body.

- The prefix reads original RemoteRole at +32, Role at +31 and the caller's
  tick type. If RemoteRole equals3, Role is below2, or tick type equals1, it
  tests bit40 of byte +30C. With that bit clear it jumps to the base-call tail
  at `10019DD5`; other paths enter its custom work at `100195E8`. No reflected
  field name is assigned here to the unverified +30C bit.
- Custom work loads native XLevel +AC, uses its Double TimeSeconds +DC and
  Float LastRenderTime +34, and stores/clamps a local elapsed-since-render
  Float. It also performs actual Class/Super traversal, including the imported
  APlayerPawn Class identity. A type check here is not permission to replace
  the original class relation with a display-name predicate.
- A branch testing actor byte +788 bit1 calls original `AActor::InStasis`
  through IAT `100273E4` at `10019659`. Depending on that result or a native
  elapsed-time comparison, it invokes XLevel's DestroyActor virtual +94 with
  force argument0 and returns1 directly (`10019683..10019698`). This work
  occurs before the eventual base Actor Tick; returning1 is not proof that
  Actor's bTicked marker was assigned.
- Other branches update native Floats, including a timed interpolation from
  target +69C/+6A0/+6A4 into +180/+184/+188 under remaining time +6A8. The
  body invokes local routines `10019E30` and `10019F60` with the original delta
  at `100197C3` and `100197CB`, and later uses virtual ProcessEvent +10
  (one confirmed call at `10019D91`). These are observable writes/callbacks,
  not an ignorable preamble.
- The common tail at `10019DD5` pushes the original tick type and original
  Float delta, then calls genuine `AActor::Tick` through IAT `10027238` at
  `10019DDF`. It returns that call's EAX unchanged through `10019DF5`; this
  preserves Actor's deferred-owner return0 instead of forcing success1.

Thus neither a local stasis assumption nor an idle-pose observation permits
skipping the override. Its native work can precede Actor's own stasis test and
owner-defer logic. Capturing selected authored fields from a real Training NPC
will be necessary to determine the actual first-frame branch; this audit does
not claim that any particular NPC takes the custom or direct-base branch.

## Separate Pawn preprocessing and suffix prerequisites

Even implementing the ScriptedPawn wrapper does not by itself make the base
Actor path complete for these instances. The preceding Actor audit already
records bIsPawn at actor +28 bit800000. Static bytes
`103A1BA8..103A1BF0` independently show that the original base routine tests
that flag, traverses actual Class/Super ancestry against native APawn identity
`1058C660`, retains the actual Pawn pointer in a local and derives role-related
context before entering the shared animation body at `103A1BF0`.

Before the Pawn-context increment, the portable driver deliberately threw
`Original Actor Tick Pawn preprocessing is not implemented` after the marker
when bIsPawn or actual Engine.Pawn ancestry is present. The separately
implemented non-Pawn tail also explicitly refuses Pawn substitution. Those
guards expose missing work; removing them is not an implementation of the
original Pawn/player/simulated-proxy paths documented as separate in the Actor
Tick audit. Reaching a PhysicsNone value in one snapshot would likewise not
prove that later callbacks cannot select Pawn physics.

The new base-prefix adapter now performs the exact flag/Class/Super/Role
capture and feeds the captured animation gate into MAIN/BLEND. It still
refuses the unimplemented Pawn continuation, and the nearest-native
ScriptedPawn gate remains closed before its genuine wrapper. See
ORIGINAL-PAWN-TICK-PREPROCESSING.md; this foundation is not NPC admission.

An eventual implementation needs, in original order and within the same caller
transaction, the actual selected ScriptedPawn Tick wrapper, its own native
fields/AI/callback/lifetime boundaries, the real Pawn context and role paths in
AActor.Tick, and APawn.performPhysics when selected. Genuine failure must undo
the entire frame root; native callbacks must not be replaced with success/no-op
results merely to advance the map scan. Unknown nearer native subclasses
remain unsupported until independently authenticated.

## Deliberate limits

No native admission, NPC skip, fabricated AI result, generic Pawn allowance or
compiler-source change is made. This document authenticates the exact next
boundary and several required orders; it does not complete the full
ScriptedPawn AI Tick body, APawn physics, world perception/navigation,
state-machine callbacks, a working NPC simulation, or headset acceptance.

## Additional offset-based custom-body dependency map

Read-only continuation audit, 2026-10-26. The following records the complete
direct service/callback set in `100195E8..10019DD5` and the two called local
helpers. It remains a prerequisite map, not a complete portable arithmetic
implementation contract. In particular, logical property names are **not**
assigned to these native offsets without independent field-registration or
equivalent authored/native-layout evidence. All field accesses described
below are fresh where the original instructions reread them; callbacks can
change later inputs.

This custom-body audit does not identify byte+30C bit40 with either
bSimulatedPawn or bAlwaysTick: a named typed snapshot is separate authority
from a proven native physical bit mapping. Nor does this document imply that
the newly implemented base-Pawn context/prefix helper implements NPC Tick.
The future arithmetic adapter must state its PC64/nearest-even/masked-exception
profile explicitly and reproduce the actual Float spill boundaries; algebraic
formulas below are not permission to rely on host FP defaults, change NaN
behavior or fuse operations.

### External service order and eligibility

The custom body has the following service calls, in original order:

| Address | Service and exact authority |
| --- | --- |
|10019659|IAT100273E4: original AActor.InStasis|
|1001967D|actual XLevel virtual+94: DestroyActor(actor,force0)|
|100197C3|local exported UpdateAgitation, body10019E30..10019F55|
|100197CB|local exported UpdateFear, body10019F60..1001A005|
|10019B4A..10019B6A|construct EnableCloak FName, FindFunctionChecked, actual ProcessEvent+10 with one Bool|
|10019C4F..10019C67|construct ExtinguishFire FName, FindFunctionChecked, actual ProcessEvent+10 with no arguments|
|10019D08|IAT100271C8: Core.appSqrt(Double), retained x87 result|
|10019D79..10019D91|construct SpurtBlood FName, FindFunctionChecked, actual ProcessEvent+10 with no arguments; repeated by the native loop|

The three callback names are independent UTF-16 literals at10034D98
(EnableCloak),10034D78 (ExtinguishFire) and10034D60 (SpurtBlood). Their common
imports are100270F4, Core FName constructor with EFindName1, and1002703C,
Core FindFunctionChecked. These are actual selected ProcessEvent calls, not
invented direct calls to the class function or successful empty substitutes.
There are no direct visibility/line-of-sight, navigation or native AI-manager
calls elsewhere in this custom body or in the two local helpers.

The earliest lifetime branch requires byte+788 bit1, then either InStasis
returns nonzero or the captured Float render age is strictly greater than
Double5 at10028388. It calls DestroyActor(force0) and returns1 immediately,
without reaching the base Actor.Tick tail. The original class walks at
1001960E..1001964C read the actual imported APlayerPawn Class, field+5D4 and
DeusExPlayer Class1003AAD8; this bounded body contains no store or callback
in those walks. They do not justify a name-based class test.

When lifetime removal is not selected, remaining time+6A8 controls the
vector interpolation into+180/+184/+188 from target+69C/+6A0/+6A4. For the
partial step it retains Z's difference through more x87 operations while
spilling X/Y differences and the reciprocal to Float; do not replace that
with an assumed uniform Vector interpolation. A completed step raw-copies
the three target words and writes remaining time positive0. An additional
local flag is set1, then cleared only if DWORD+910 bit400 is set and Actor
Float+38 is strictly greater than Float1200 at10028380. This local flag gates
the later bleeding work, not UpdateAgitation/UpdateFear or all custom Tick.

### UpdateAgitation and UpdateFear

Both exported helpers contain **no external or virtual calls**. They have
native storage effects even when no script callback is selected. The formulas
here describe ordinary finite inputs; source comparison bits and PC64/Float
spill boundaries still govern exceptional and exact-boundary implementation.

UpdateAgitation first decrements positive Float+728 by supplied delta and
Float-stores it, comparing the retained x87 subtraction before zero-clamping
a negative result. It computes a decay amount as follows:

- Positive delay+724 less than delta: clear+724 and retain
  `(1 - delay/delta) * (delta * Float+720)` in the original operation order.
- Positive delay+724 at least delta: Float-store `delay-delta`, decay0.
- No positive delay: retain `delta * Float+720`.

If DWORD+650 bit8 is set and decay is positive, clear that bit and visit
exactly16 entries descending from address actor+8E4 at stride10 hex. Each
entry's FName at-8 must not be None, packed flag at+4 bit1 must be clear,
and its Float at+0 must be positive. For each eligible entry, freshly set
actor+650 bit8, subtract the retained decay from the entry Float, Float-store,
then compare the retained subtraction and zero-clamp a negative result.
These are native fixed-layout entries, not permission to reinterpret an
unverified portable script array schema or to skip aggregate/reference bounds.

UpdateFear's delay is Float+738 and rate is Float+734. A positive delay at
least delta is decremented/Float-stored and the helper returns immediately.
A shorter positive delay is cleared and gives retained decay
`(1 - delay/delta) * (delta * rate)`. Otherwise decay is `delta*rate`.
Only when decay and Float+73C are positive does it subtract decay from+73C,
Float-store, compare the retained result and clamp a negative result to0.

### Ordered native countdown/storage work

After both helpers, the body performs this ordered storage sequence:

1. Float+768 always subtracts delta, Float-stores, then negative retained
   result clamps to0. Float+76C adds delta when pointer+3D0 is nonnull;
   otherwise a nonzero value is reset to0.
2. Positive-only countdowns+770,+774. Float+668 decrements only when positive
   AND pointer+3D0 is nonnull; all other paths write it0.
3. Positive-only countdowns+980,+984,+988,+9E0,+9B0,+640, each with its own
   retained-result comparison/clamp after Float storage.
4. Positive+648 decrements. Its nonpositive/unordered retained result clears
   both+648 and+644. Then positive-only countdowns+64C,+9C0,+72C run.
5. A nonnegative+9C4 adds delta; a retained result greater than Float+730
   stores literal Float-1 instead.

Original C0/C3 status tests are not interchangeable with a blanket
`max(0,old-delta)` or a generic unordered policy. The Float stores occur before
several comparisons; the compared value is still retained x87 precision.

### Cloak, packed-state, fire and bleeding branches

Byte+9D8 bit8 selects EnableCloak. Its Bool parameter's low bit represents
signed native INT+428 less than or equal to INT+9DC. There is no separate
probe shortcut in this wrapper before the real FindFunctionChecked and
ProcessEvent boundary.

If DWORD+318 bit80000000 is set, the branch preserves it only when the three
acceleration Floats+150/+154/+158 are not all zero, Physics byte+30 equals1,
and byte+978 is nonzero. Otherwise it clears that packed bit while preserving
other bits; if byte+978 was nonzero, Float+348 subtracts Double4. It then ORs
DWORD+974 bit8 and clears DWORD+97C, bytes+979/+978 and Float+988. These
offsets do not establish logical crouch/dodge property names by themselves.

Byte+5A4 bit1 selects the fire branch: Float+5A8 adds delta and is Float-stored;
if the retained result is at least Float+9BC, invoke selected ExtinguishFire.
The owned script itself clears Pawn.bOnFire/burnTimer, calls SetTimer(0,false)
and iterates actual BasedActors Fire references to Destroy. Those nested
services and their whole-root effects cannot be replaced by this wrapper's
own guessed visual/fire cleanup.

Bleeding requires the captured distance-related local flag, positive Float+6BC
and byte+6B8 bit1. The original interval is Float-spilled after computing:

```
Float(Double1 - clamp(Float+6BC,Float0,Float1) + Double0.1)
  / clamp(appSqrt(Double(Velocity.X^2 + Velocity.Y^2 + Velocity.Z^2))
          * Double(1/512), Float0.05, Float1)
```

The numerator is Float-spilled before the velocity loads. The squared sum
and clamp arithmetic have their actual retained x87 order; the Double argument
to appSqrt is explicitly spilled. appSqrt's dead Double return spill does not
replace its retained x87 result in the subsequent multiply. The velocity channels
are native+144/+148/+14C. Float+6C0 adds delta and is stored. While its fresh
value is at least the captured Float interval, invoke selected SpurtBlood,
then reread/subtract the interval from+6C0, Float-store and repeat. This is a
genuine callback/remainder loop: do not invent a fixed iteration cap or keep
a stale callback-mutated counter. Existing work/state budgets must bound it
and roll back the whole caller root on failure. SpurtBlood's owned script
spawns an actual BloodDrop via original Spawn and vector arithmetic.

After that loop Float+6BC subtracts `delta / Float+6C4` and Float-stores. Within
the captured visibility gate, a final nonpositive/unordered+6BC clears both
+6C0 and+6BC. A false visibility gate skips that cleanup as well. The
common tail then calls actual AActor.Tick with the original delta/tick type.

These findings establish no new ScriptedPawn, Pawn-physics or callback
provider. Logical offset-to-property authority, exact exceptional arithmetic,
all callback dependencies and the surrounding Pawn context remain separate
implementation/test requirements. First Training live field observations are
needed to identify which of these branches the actual first NPC reaches.

## First typed Training boundary observation

The new rollback-only startup probe initially captures the first reached
native boundary as actual `00_Training.JaimeReyes0`, concrete Class
`DeusEx.JaimeReyes`, nearest native Class `DeusEx.ScriptedPawn`. Its typed
entry values are Role4, RemoteRole2, Physics2, bIsPawntrue, bAlwaysTickfalse,
bSimulatedPawnfalse and bStasisfalse. These are live reflected snapshots at
the already reached gate, not published gameplay, native packed-bit proof or
an implementation of ScriptedPawn.Tick. Final repeated/source/rollback checks
remain required separately.

Role4 and RemoteRole2 with the diagnostic's literal tickType0 satisfy the
unconditional custom-body entry condition, regardless of the unnamed+30C
bit. This observation rules out using a direct-base/no-op wrapper for this
first NPC. Physics2 also identifies a real Pawn-physics prerequisite if the
later virtual physics branch is selected; it does not authorize substituting
the decoration/Actor physics implementation.

## Independent reflected-name/native-offset layout proof

Read-only continuation, 2026-10-26. This closes a bounded subset of the
offset-to-name gap above. It does not implement or admit native Tick. Original
PE32 member offsets are not offsets into portable C++ objects.
Native offsets, sizes, masks and VAs below are hexadecimal; property counts,
references, dimensions and payload lengths are decimal.

The authority is a layout calculation from the owned packages' complete
Children/NextField chains and the owned Core linker, corroborated by native
registration sizes and the exported native copy. It is **not** a serialized
Offset field, a literal name/offset registration, or a reference SDK header.
The four Engine/DeusEx file hashes in the earlier evidence table were rechecked; Core.dll
is additionally SHA256
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

The independent PowerShell reader adapts the existing read-only extractor
`artifacts/decal-native-20261012-render/read-actor-property-chain.ps1` and its
table reader `artifacts/decal-native-20261011/read-decal-properties.ps1`.
This continuation ran its generalized reader inline: no new extractor file,
compiler input, build, or original DLL execution/loading was created. For
each actual owner it follows Children and NextField, verifies each child's
Outer, skips non-property children without resetting the previous property,
checks the complete property payload is consumed, and uses the declared type,
dimension and referenced Struct. Export-index sorting is not field order.

### Original linking and independently registered anchors

Actual `UStruct.Link` is `10101933 -> 10126060`. On Link1 its cursor starts
at the superclass's actual PropertiesSize, rounded to4
(`1012609D..101260CC`); each own property calls virtual Link with the previous
property and advances the cursor to Offset+ElementSize*ArrayDim
(`101260CF..10126116`); the final size rounds to4 (`10126118..10126123`).
The distinct Link0 path preserves supplied offsets around Link, so this
calculation is not permission to relayout arbitrary native Cpp properties.

The independently authenticated class-packing predicate is positive:
exported UClass vtable `101758F4`, cell+58 at `1017594C`, contains exported
`MergeBools@UClass` thunk `101024AF -> 10111560`, whose body is
`MOV EAX,1; RET`. Actual `UBoolProperty.Link` is
`10101230 -> 10166060`; it verifies the previous property's Class/Super chain
against actual UBoolProperty class `102A6BF0`, and packs only when the owner
predicate and previous mask permit it. It doubles the previous mask and reuses
that property's offset (`101660CA..101660E2`), otherwise starts mask1 in a
new aligned DWORD (`101660E4..10166105`). It receives the previous *property*,
not an arbitrary previous Function/Enum child.

Byte.Link supplies size/alignment1; Int/Float/Object/Class/Name supply4;
Str/Array supply0C. Struct.Link copies the actual linked Struct size and
chooses alignment2 for size2, alignment1 below4 otherwise, and alignment4
for size>=4 (`10169029..10169056`). Struct inheritance must be included:
Core.Object.Plane inherits the Vector bytes, yielding size10, not size4.
Omitting those bytes would incorrectly shift Actor's later fields by3C.

| Owner | Actual registered starting/base size | Calculated own property count / final size | Independent native size anchor |
| --- | --- | --- | --- |
| Engine.Actor | Core.Object28 | 212 /318 | Core.Object initializer `10150CFE` supplies28; Actor initializer `1035ED70` supplies318 to its actual Class `1057CDF0` |
| Engine.Pawn | Actor318 | 162 /5D4 | Pawn initializer `103B7E94` supplies5D4 to actual Class `1058C660`, Super Actor at `103B7E87` |
| DeusEx.ScriptedPawn | Pawn5D4 | 235 /A10 | initializer `10018F06` suppliesA10 to exported Class `1003C020`; `10018EFA..10018F00` supplies imported actual APawn Class |

DeusEx IAT `10027028` names the native UClass constructor; IAT `100273E0`
names `?PrivateStaticClass@APawn@@0VUClass@@A`. Its supplied size is retained
through Core native UClass/UStruct construction to PropertiesSize+3C
(`1012865C -> 1010126C -> 10125EF0`, store `10125F47`). Thus the matching
totals are checked against real registered sizes, not sizes guessed from the
last observed member access. The class references and first Children links
are the owned records already recorded above.

### Actor bit identity and interpolation anchors

The complete calculation maps **Actor+30C bit40 to Engine.Actor.bSimulatedPawn**,
not bAlwaysTick. Its actual Engine.u field is ref888, BoolProperty, dimension1,
flags2, Next227, file offset1AB02/length13. The +30C Bool run is, in actual
order, bNetInitial, bNetOwner, bNetRelevant, bNetSee, bNetHear, bNetFeel,
bSimulatedPawn, bDemoRecording, bClientDemoRecording, bClientDemoNetFunc;
their masks are1 through200. RenderIteratorClass follows at310, then
RenderInterface at314, completing Actor318.

Engine.Actor.bAlwaysTick is instead+1A0 mask20: ref4684, BoolProperty,
dimension1, flags2, Next995, file4EB1A/13. These calculated positions are
independently corroborated by the actual exported
`AScriptedPawn(AScriptedPawn const&)` body, export-name index11 at100045C0:
it masked-copies +30C bit40 at `1000539D..100053AC`, and +1A0 bit20 at
`10004E20..10004E2A`. A masked copy alone supplies no field name; its
agreement with the complete linked layout is the name/offset authority.

The same calculation maps Engine.Actor.PrePivot to180 (ref4709, actual
Core.Object.Vector), ScriptedPawn.DesiredPrePivot to69C (ref1958, Vector,
Next2592, file6C48E/14), and ScriptedPawn.PrePivotTime to6A8 (ref2592, Float,
Next4082, file70E7E/13). The native copy independently copies the three
69C/6A0/6A4 words at `100062E1..10006306`, then6A8 at
`10006309..1000630F`. These are the specific source/target/time identities
used by the interpolation body above.

### Bounded scalar and condition mappings for subsequent work

All unqualified names in this table are declared by DeusEx.ScriptedPawn,
except the explicitly qualified Pawn entries. Floats here are scalar
FloatProperty dimension1; the other declared types are explicitly stated.
References are actual one-based package export references, not native offsets.

| Native offset | Actual reflected property | Ref | Type when not Float |
| --- | --- | --- | --- |
| 640 | CarcassCheckTimer |8769 | |
| 644 | PotentialEnemyAlliance |4053 |Name |
| 648 | PotentialEnemyTimer |5593 | |
| 64C | BeamCheckTimer |8762 | |
| 668 | ReloadTimer |4067 | |
| 6BC /6C0 /6C4 | BleedRate /DropCounter /ClotPeriod |10204 /16814 /16813 | |
| 71C /720 | AgitationSustainTime /AgitationDecayRate |8576 /8575 | |
| 724 /728 /72C | AgitationTimer /AgitationCheckTimer /PlayerAgitationTimer |2201 /2547 /8565 | |
| 730 /734 | FearSustainTime /FearDecayRate |4168 /8552 | |
| 738 /73C | FearTimer /FearLevel |1705 /1706 | |
| 768 /76C | AlarmTimer /WeaponTimer |5521 /3269 | |
| 770 /774 | FireTimer /SpecialTimer |2520 /5528 | |
| 980 /984 /988 | AvoidWallTimer /AvoidBumpTimer /ObstacleTimer |8465 /5087 /4238 | |
| 9B0 /9BC | TakeHitTimer /BurnPeriod |5493 /10174 | |
| 9C0 /9C4 /9E0 | FutzTimer /DistressTimer /CloakEMPTimer |8429 /1725 /4258 | |
| 348 | Engine.Pawn.MoveTimer |1030 | |
| 3D0 | Engine.Pawn.Weapon |14 |Object, actual Engine.Weapon |
| 428 | Engine.Pawn.Health |53 |Int |
| 5A8 | Engine.Pawn.burnTimer |5176 | |

For the UpdateFear subset specifically, the actual consecutive owned records
are FearDecayRate ref8552 at file9BDDC/13, Next1705; FearTimer ref1705 at
file6A3D4/13, Next1706; FearLevel ref1706 at file6A3E1/13, Next611. The native
copy loads/stores734 at `100069EC..100069F2`,738 at
`100069F8..10006A04`, and73C at `10006A0A..10006A0D` (its source pointer
was advanced to+78C, so source displacement-50 is73C). These corroborate the
calculated scalar slots used by `UpdateFear10019F60`, without changing that
routine's audited operation order, retained intermediates or branch rules.

The bounded condition mappings are: +650 bit8=bAlliancesChanged;
+6B8 bit1=bCanBleed; +788 bit1=bDisappear; +910 bit400=bTickVisibleOnly;
+974 bit8=bClearedObstacle; +9D8 bit8=bHasCloak. Pawn+318 bit80000000 is
bAdvancedTactics, Pawn+5A4 bit1 is bOnFire. ScriptedPawn+978/+979 are
TurnDirection/NextDirection Byte properties constrained to its actual ETurning
enum; +97C is ActorAvoiding(Object Engine.Actor), and+9DC is
CloakThreshold(Int). In particular, the pointer+3D0 condition in the native
timers is **Weapon**, not an assumed Enemy pointer.

This appendix does not assign additional semantics to the raw native
alliance-loop entry offsets above, supply perception/navigation callbacks,
admit a native child, or establish a portable Tick implementation. The earlier
offset-based operation schedules remain the arithmetic/control-flow contract;
these names only close the stated typed-field lookup prerequisite. Current
base-Pawn-prefix/typed-boundary tests and any later ScriptedPawn implementation
must remain distinct, with actual qualified owner/type/dimension validation
and whole-root rollback.
