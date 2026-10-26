# Original ScriptedPawn Tick: portable runtime binding audit

Read-only audit, 2026-10-26, based on commit `abc4c32`. This is a binding
prerequisite, not admission of ScriptedPawn/Pawn Tick or a gameplay completion
claim. It covers the first `00_Training` native boundary only. No original
commercial DLL/EXE was executed or loaded; no compiler input or build was changed
by this audit.

The owned packages are `D:/Steam/steamapps/common/Deus Ex/System/Engine.u` and
`DeusEx.u`. Property and callback metadata were read with the already built
`artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe` (the port's
static reader). Struct child order was independently read from the package's
bounded Children/NextField chain, adapting the existing read-only
`artifacts/decal-native-20261011/read-decal-properties.ps1` table reader inline.
The original binary identities, native offsets, entry branches and service
order remain those in `ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md`.

## Declaring-field authority

All fields in the following tables have `baseField=0` and dimension 1 unless a
dimension is stated explicitly. Relative subtype names reported by the static
reader must be qualified using the field's actual source table; they are not
free-form names or cross-package aliases. The stated type is both the actual
Property type and its `Core.*Property` metaclass.

| Actual declaring owner | Fields | Type | Flags | Actual referenced type |
| --- | --- | --- | --- | --- |
| Engine.Actor | Role | ByteProperty | 0x20 | Engine.Actor.ENetRole |
| Engine.Actor | RemoteRole | ByteProperty | 0x21 | Engine.Actor.ENetRole |
| Engine.Actor | Physics | ByteProperty | 0x23 | Engine.Actor.EPhysics |
| Engine.Actor | bSimulatedPawn | BoolProperty | 0x2 | none |
| Engine.Actor | LastRenderTime, DistanceFromPlayer | FloatProperty | 0 | none |
| Engine.Actor | PrePivot, Acceleration | StructProperty (portable Vector) | 0x20 | Core.Object.Vector |
| Engine.Actor | Velocity | StructProperty (portable Vector) | 0x21 | Core.Object.Vector |
| Engine.Pawn | Weapon | ObjectProperty | 0x10020 | Engine.Weapon |
| Engine.Pawn | Health | IntProperty | 0x10021 | none |
| Engine.Pawn | MoveTimer | FloatProperty | 0 | none |
| Engine.Pawn | bAdvancedTactics | BoolProperty | 0 | none |
| Engine.Pawn | bOnFire | BoolProperty | 0x20 | none |
| Engine.Pawn | burnTimer | FloatProperty | 0x20 | none |
| DeusEx.ScriptedPawn | DesiredPrePivot | StructProperty (portable Vector) | 0 | Core.Object.Vector |
| DeusEx.ScriptedPawn | bTickVisibleOnly | BoolProperty | 0x1 | none |
| DeusEx.ScriptedPawn | bDisappear, bAlliancesChanged, bHasCloak, bCanBleed, bClearedObstacle | BoolProperty | 0 | none |
| DeusEx.ScriptedPawn | CloakThreshold | IntProperty | 0 | none |
| DeusEx.ScriptedPawn | TurnDirection, NextDirection | ByteProperty | 0 | DeusEx.ScriptedPawn.ETurning |
| DeusEx.ScriptedPawn | ActorAvoiding | ObjectProperty | 0 | Engine.Actor |
| DeusEx.ScriptedPawn | PotentialEnemyAlliance | NameProperty | 0 | none |
| DeusEx.ScriptedPawn | ClotPeriod | FloatProperty | 0x1 | none |

The following actual **DeusEx.ScriptedPawn** FloatProperty fields all have
dimension 1, flags 0, base 0, and no referenced type:

```
PrePivotTime
AgitationCheckTimer AgitationTimer AgitationDecayRate
PlayerAgitationTimer AgitationSustainTime
FearTimer FearDecayRate FearLevel FearSustainTime
AlarmTimer WeaponTimer FireTimer SpecialTimer ReloadTimer
AvoidWallTimer AvoidBumpTimer ObstacleTimer CloakEMPTimer
TakeHitTimer CarcassCheckTimer PotentialEnemyTimer BeamCheckTimer
FutzTimer DistressTimer BurnPeriod BleedRate DropCounter
```

`DistanceFromPlayer`, not `LastRenderDistance`, is the Actor distance field.
`Weapon`, not Enemy, is the native +3D0 pointer tested by the countdowns.
`bAdvancedTactics`, not an invented crouch flag, is Pawn +318 bit80000000.
The actual +30C bit40 is `bSimulatedPawn`; `bAlwaysTick` is unrelated to that
wrapper entry test. These identities come from the independently linked native
layout proof, not a convenience renderer field.

The actual `DeusEx.ScriptedPawn.ETurning` Enum contains exactly three names in
this order: `TURNING_None` (0), `TURNING_Left` (1), `TURNING_Right` (2). It is
DeusEx.u export5141, Core.Enum import-104, outer ScriptedPawn5, base0, next141,
file payload547483/11. A static Enum-body read consumed the complete payload.
The spelling is **TURNING_**, not TURN_. This constrains fixture metadata; it
does not authorize rejecting arbitrary raw Byte values at a native Byte read.

`OriginalAnimationField` in `native/portable_actor_animation_phase.inl` already
checks exact qualified declaring owner, OwnsProperty, kind, dimension, flags,
base field and metadata metaclass. It accepts the scalar cases above; add the
exact enum/object/vector constraint checks for cases with referenced types.
For native state, use the actual `nativeXLevel` and its Double
`nativeTimeSeconds`, and the existing native `lastRenderTime` mirror after
authenticating Actor.LastRenderTime, as `OriginalActorStasis` does. Do not
replace the native Double clock with reflected Float Level.TimeSeconds.

## The fixed 16-entry alliance schema

`DeusEx.ScriptedPawn.AlliancesEx` is **StructProperty, dimension 16, flags 0,
base 0**, constrained to the actual `DeusEx.ScriptedPawn.AllianceInfoEx`
Struct. It is not a dynamic array and does not contain Actor pointers.

The actual Struct is DeusEx.u export reference 10216, outer ScriptedPawn
reference 5, base 0, next 10219, source 0, Children 1249, file payload offset
681451/length 22. Its complete child chain is:

| NextField order / ref | Actual member | Type | Dimension / flags / base | Native member offset |
| --- | --- | --- | --- | --- |
| 1249, next 5592 | AllianceName | NameProperty | 1 / 0 / 0 | +0 |
| 5592, next 1783 | AllianceLevel | FloatProperty | 1 / 0 / 0 | +4 |
| 1783, next 4028 | AgitationLevel | FloatProperty | 1 / 0 / 0 | +8 |
| 4028, next 0 | bPermanent | BoolProperty | 1 / 0 / 0 | +C, mask 1 |

Every member's actual Outer is that Struct; every member has no referenced
type and the exact corresponding Core Property metaclass. Each entry is native
16 bytes. AlliancesEx begins at actor+7EC; descending agitation addresses
8E4,8D4,...,7F4 visit slots 15 through 0. The loop's value at agitation-8 is
the FName `AllianceName != None`, **not** an Actor pointer. `AllianceLevel`
exists and must survive whole-struct read/modify/write unchanged.

For the portable provider, authenticate the containing Property, actual Struct,
its complete descriptor and four-member schema before touching live values.
Use `Read(actor,field,slot)` for each freshly read slot, with `slot<16`; require
Kind::Struct and exactly the four canonical member keys and concrete kinds.
The native Name test is a Name identity test (the VM's canonical Name equality
also treats an empty Name spelling as `None`), not an object-presence conversion.
A native agitation store should retain
the entire freshly read entry, change only `agitationlevel`, normalize with
`TypedLiveValue`, and publish through a same-root Reference using `Touch`,
`InstanceStorageKey(actor->cls,field)` and `execution.WriteReference` plus
`ValidateStateBudget`. This retains sibling fields, callback changes and root
rollback; it must not go through generic script Let/clock updates.

## Actual selected callback packets

| Exact original base Function | Native / flags | Native packet | Non-parameter locals |
| --- | --- | --- | --- |
| DeusEx.ScriptedPawn.EnableCloak | 0 / 0x2 | one BoolProperty bEnable, dim 1, flags 0x80, base 0, no subtype | none |
| DeusEx.ScriptedPawn.ExtinguishFire | 0 / 0x2 | no parameters or result | F: ObjectProperty, Fire, flags 0 |
| DeusEx.ScriptedPawn.SpurtBlood | 0 / 0x2 | no parameters or result | bloodVector: Core.Object.Vector, flags 0 |

The selected Function can belong to an actual receiver-ancestry Class or State
and can have locals/different input spelling. Validate all `flags&0x80`
variables: exactly one scalar Bool with **flags exactly0x80** for EnableCloak;
zero parameter/result variables for the other two. Do not reject harmless
locals or assume the three functions are lifecycle events (`flags0x800`).

`LifecycleEventKind` / `LifecycleEventDeclaration` in
`native/portable_actor_lifecycle_events.inl` do **not** fit unmodified: they
cover different event names, assume Core.Object/Engine.Actor owners, and accept
only no-parameter or Actor Other packets. In particular EnableCloak is Bool.

The exact reusable native-call machinery is already in
`native/portable_actor_animation_phase.inl`:

1. `OriginalAnimationActorVirtuals` (near line121) authenticates nearest actual
   native Class and both ProcessEvent+10 / IsPendingKill+2C aliases. It already
   includes actual DeusEx.ScriptedPawn, Engine.Pawn and related audited classes.
   This narrow virtual proof does **not** prove their Tick or physics slots.
2. `AdvanceOriginalMainAnimation`'s local `find` (near line218) reproduces
   checked scriptability gating, `CurrentDispatchContext`, selected-node-aware
   `ResolveFunctionAtSelectedNode` / `ResolveFunction`, and actual
   `FunctionIdentity`. It validates real authored Core.Function metadata and
   Class/State ancestry, and retains shared Function identities across calls.
3. Its `queries.processEvent` (near line350) performs the original order:
   actual Actor virtuals and Level binding, Level.bBegunPlay, GIsScriptable,
   selected probe, virtual pending kill, nativeIndex, then nativeIndex0/flags400
   RPC refusal. Only after these gates does it materialize the selected
   declaration, validate its packet and invoke `execution.CallFunction` on the
   **already retained identity**. `OriginalAnimationPendingKill` is the existing
   authenticated pending-kill query.

A minimal implementation can extract a narrow common retained FindChecked /
ProcessEvent helper from that pair, or reproduce that pair locally for these
three callback kinds. Extend packet validation to the single Bool
`Value::Bool(enable)` input. Keep scratch/retained identity reservations in the
existing NativeScratchScope ledger and use the caller's Execution/transaction.
Checked lookup returns null while GIsScriptable is disabled; begun/scriptability
suppression must not dereference that null or invent a successful callback.
The selected function's own native-index/RPC gates apply before executing a
packet; do not force selected flags to equal the base's flags0x2.

Generic `execution.CallEvent` / host `ResolveEvent` does selected resolution,
probe/begun/deletion suppression, but does not by itself implement this exact
GIsScriptable/nativeIndex/RPC native boundary. Bare CallFunction also bypasses
those gates unless they have already run. The ordinary lifecycle helper is
therefore a useful packet-journal pattern, not complete callback equivalence.

ExtinguishFire's script really changes Pawn fire state, SetTimer and destroys
BasedActors Fire objects; SpurtBlood really spawns BloodDrop. Do not substitute
visual cleanup or empty callbacks. SpurtBlood can change its receiver: reread
DropCounter after each call, and retain whole-root work/state budget failure
rather than adding a silent native iteration cap.

## Minimal safe Tick integration

The current `AdvanceOriginalActorTick` and `AdvanceOriginalTickTail` both use
`RequireOriginalActorTickPhysicsImplementation`. That combined gate currently
permits only audited classes whose native Tick and physics aliases are already
represented. Merely adding ScriptedPawn to its list is incorrect:
ScriptedPawn.Tick+64 is a real wrapper and its physics+8C is APawn.performPhysics,
not Actor.performPhysics.

Separate these authorities without broadening native-child admission:

1. Select the nearest **actual native Class** from the bounded, authenticated
   runtime Class/base chain (Class descriptor/field links, no default-object
   class substitute, cycle/depth guard). Exact ScriptedPawn needs its proven
   custom wrapper. Other unimplemented native Tick overrides still refuse.
2. Enter the ScriptedPawn wrapper before AActor's stasis/owner/parity/animation
   processing. Its custom work genuinely precedes AActor.Tick, including on an
   owner-deferral path. The disappearance branch destroys and returns1 without
   calling the base.
3. Give only the authenticated wrapper an explicit original AActor base-body
   entry. That entry must not redispatch the wrapper or reject it through the
   combined native Tick gate. Preserve the real base return0/1, original delta
   and tickType, and same-root journal. Do not synthesize a successful base return.
4. Keep physics admission separate: no AActor physics substitution for native
   APawn.performPhysics. The actual first Jaime boundary has Physics2, so this
   remains a real prerequisite if that later branch is reached.
5. After genuine AActor animation, the currently explicit pending Pawn
   continuation must remain a visible refusal until its original preprocessing,
   callbacks/state/physics path has actually been implemented. Do not route it
   through `AdvanceOriginalNonPawnTickTail` or suppress it based only on bIsPawn.

This audit supplies metadata/schema and integration constraints only. The
arithmetic wrapper/decay helpers, actual runtime implementation, rollback tests
and live first-Training evidence remain separate work.
