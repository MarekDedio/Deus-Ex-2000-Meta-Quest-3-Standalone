# ClassContext: next original startup boundary

Read-binding hardening and read-only retail audit, 2026-10-13. No ClassContext implementation or successful original
world startup is claimed. Original Windows executables/DLLs were not executed.

## Actual original expression

The fresh non-publishing Training probe now passes native280 and stops at
PreBeginPlay slot1331, `00_Training.Camera4`, actual
`Engine.Pawn.InitPlayerReplicationInfo` PC36/opcode18. Original bytecode is:

```text
PC21 Let
PC22 Context PlayerReplicationInfo
PC31 InstanceVariable PlayerReplicationInfo.PlayerName
PC36 ClassContext
PC37 ObjectConst Engine.GameInfo
     nullSkip=5, zeroSize=0
PC45 DefaultVariable GameInfo.DefaultPlayerName
PC50 Return
```

The selector is the actual imported Engine.GameInfo UClass, not the active
DeusExGameInfo instance or its derived class defaults. Current VM execution
unconditionally refuses opcode0x12 with
`ClassContext requires class-default object identity`. This does not prove that
the class/default-value storage is missing: `DefaultClass`, `Read` and
`classDefaultValues` already represent this exact qualified reflected field.

`ReferenceFor` now validates qualified property ownership before exposing reads
as well as writes. New class-context reads must validate class/property ownership and
scalar/fixed-array bounds, preventing foreign fields from falling through by
unqualified name or default-zero. Class-default persistence and journal semantics
must remain concrete-class isolated.

Retail Engine.Actor and Engine.PlayerPawn both declare bSinglePlayer. Actor's
authored default tag is true; PlayerPawn redeclares it without an own tag (its
new field starts false). A compiled Actor field reference on Camera must remain
valid and distinct from PlayerPawn's field. Read binding therefore checks actual
declaring-class ancestry, not the nearest same-named field. Tagged/leaf-overlay
reads select the declaration at their own source class; inherited compiled reads
do not consume a more-derived same-name map/default tag, and new declarations do
not inherit another field's tag by name. Generated foreign-read and shadowed-field
and mutable-overlay/canonical-save controls cover these distinctions. Shadowed-base writes retain the existing
explicit refusal until qualified mutable-overlay storage is expanded; they are
not silently redirected.

This guard validates qualified declaring-class ancestry, not complete malformed-
package field linkage. Existing property name lookup still trusts indexed
Outer/path identities rather than requiring every Property export to occur in
Class.Children/Next. Orphan-property rejection and frozen-birth shadow controls
remain separate hardening work; no complete field-linkage validation is claimed.

The rebuilt generated defaults suite passes 1,379 checks/40 refusal controls.
Foreign reads assert the exact ownership diagnostic and complete checkpoint
rollback, including a prior default write. Shadow controls cover inherited and
redeclared authored tags, mutable class/instance overlays, canonical saves and
legacy reset. The unchanged owned Training startup probe still reaches its exact
PC36 ClassContext refusal with complete repeated rollback; original defaults
passes 1,002 checks and original Training/Combat save regression checks pass.
Strict NDK27 ARM64 syntax passes. These do not establish ClassContext execution
or live headset acceptance; no new APK is built for this checkpoint.

## Owned retail Core.dll evidence

SHA256: `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Named export thunks were followed through bounded static disassembly:

- execClassContext thunk10101091, body1012edb0. Selector executes once against
  FFrame.Object (`[ESI+8]`) at1012eddb..1012edef. A valid class supplies default
  storage as child receiver ECX at1012ee37..1012ee47 without replacing FFrame.
- Null selector warns, clears property/lvalue globals, skips `skip+3`, and
  zero-fills the encoded result byte count at1012ee61..1012eeb7. It does not throw.
- execDefaultVariable thunk10101690, body1012ed00 uses receiver's class default
  storage and actual reflected property offset.
- execSelf thunk101013ca, body10130060 returns receiver ECX directly. Thus child
  Self sees the default-object context; preserving FFrame does not imply Self
  stays the caller.

Pinned SurrealEngine differs in null handling and Self context, so it is useful
comparison evidence, not retail authority for these branches. The existing VM
hardcodes `frame.self` and ordinary child evaluation carries an outer context;
merely replacing the opcode refusal with `Eval(child,classPath)` would silently
misrepresent broader behavior. A UClass path is not a generic default-object
identity for casts, instance fields, function/native calls or event dispatch.
The parser currently discards ZeroFillSize; that metadata needs preservation and
validation before any null/result-layout behavior can be claimed.

## Implementation and proof requirements

The exact original property path can use a validated non-null loaded script
Actor UClass, direct DefaultVariable child and existing defaults/reference
journal, without inventing a generic CDO identity. Unsupported shapes must stay
explicitly guarded while broader identity/context semantics are implemented.
This bounded increment alone does not finish ClassContext or campaign startup.

Required controls include exact Engine.GameInfo.DefaultPlayerName versus derived
defaults; one selector evaluation; foreign/non-class/inactive identities;
qualified inherited field and fixed-array ownership; default writes and complete
later-failure/budget rollback; cold defaults persistence and concrete-class
isolation. General child Self, nested contexts and calls require their own
retail identity audit. Null behavior must follow actual zero-fill/result metadata,
not a guessed exception policy described as original engine behavior.

Then rerun the unchanged fresh-world probe, deliberately update its exact next
boundary, and retain complete original save/GC/actor/revision rollback controls.
No guessed player name or skipped original callback is an acceptable substitute.

## Complete owned-script census

The 2026-10-13 read-only census decodes 8,282 Function/State/Class script sections
in Engine (1,417), DeusEx (6,276) and Extension (589), with zero decode errors.
All 110 ClassContext nodes occur in 42 Function exports: Engine29/9 functions,
DeusEx81/33 functions, Extension0. This is a structural census, not execution.

Selector roots are ObjectConst37, LocalVariable42, MetaCast7, InstanceVariable14,
Context3, ArrayElement2 and StructMember5. Child roots are DefaultVariable92,
BoolVariable15 and VirtualFunction3. Encoded zero-size/count pairs are
size0:20, size1:5, size4:82, size12:1 and size32:2.
The 107 property uses include Str19, Object14, Bool15, Struct1 (Vector), Class4,
Float30, Name3, Int16 and Byte5. No literal NoObject selectors or selector
function/native calls occur; null paths and evaluate-once effects need generated
behavioral tests. No child subtree contains Context/ClassContext or direct
InstanceVariable/Self; Self occurs only in a real static-call argument.

Representative original paths that must not be reduced to a constant getter:

- Engine.GameInfo.PostBeginPlay122/171/220/269 selects WaterZoneType; Weapon.
  PostBeginPlay47/67/98/118 selects ProjectileClass/AltProjectileClass.
- ScriptedPawn.InitializeInventory205 selects an Ammo metacast of
  InitialInventory[i].Inventory, guarded by the original checks at119/174.
- ScriptedPawn.AICanShoot999/1033/1067 selects dxWeapon.ProjectileClass through
  ordinary Context and reads inherited collision dimensions.
- DeusExWeapon.UpdateInfo859 and UpdateAmmoInfo251 select AmmoNames[i].
- TeamDMGame.ChangeTeam212/354 reads Actor.MultiSkins, Texture ObjectProperty
  dimension8. The encoded zero size32 is the whole pointer array, not a struct;
  ArrayElement surrounds ClassContext rather than being its child.
- TestInfo.test1 writes Vector v1 on Let's LHS (encoded zero size12).
- GameInfo.Login729 wraps Pawn.bIsHuman in BoolVariable (skip6/zero4).
- DeusExBaseWindow.CanPushScreen22/45/81/212 reads ScreenType Byte from a
  class<DeusExBaseWindow> parameter. Its ancestry is through Extension windows
  to Core.Object, not Actor. Runtime-selected subclasses are not enumerated by
  this declared-constraint census. Current defaults/save support is Actor-only;
  UI support requires coordinated expansion, not just loosening one guard.
- PlayerPawn.ReceiveLocalizedMessage0 calls LocalMessage.ClientReceive9,
  with caller Self as argument14 (skip27/zero0). TestInfo.Tick875/892 calls
  TestStatic884/906 (skip8/zero4). These are real non-native Defined|Static
  functions, flags0x2002. TestStatic reads default xnum16 and calls OtherStatic31.

Owned package SHA256 values:

```text
Engine.u    1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA
DeusEx.u    257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480
Extension.u 9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7
```

The in-memory decoder follows production compact-reference expansion and script
headers, checks logical size and Function/State payload termination. Both existing
inspector binaries independently verify representative paths. No original code
is executed. Reproduce examples with `script_bytecode_inspect.exe <GOTY-root>`
followed by the qualified function names above; a permanent structural census
test remains to add.

## Retail default-block identity and lifetime

Fresh static Core.dll reads follow named thunks to these bodies:
GetDefaultObject10102b26->10111780, GetDefaultActor1010227a->101117d0,
Serialize101011f9->101280d0, InitClassDefaultObject10101226->10152300,
InitProperties1010102d->101520d0 and UClass.Destroy10102b62->10127c20.
GetDefaultObject returns existing Defaults.Data at class+4d4 after checking its
count at+4d8 against native property size. Serialize allocates that byte array
and calls InitClassDefaultObject at1012824f..10128252.

Initializer10152323..10152349 zeroes the0x28-byte UObject header, sets Class+24
to its owning UClass, Index+4=-1 and copies the vtable. Initial Outer+18 is null,
Name+20 is0/None, Flags+1c is0 and StateFrame+0c is null. InitProperties copies
inherited data starting at+28 (10152149..10152178), preserving this header.
These are initializer facts: tagged defaults/configuration/localization follow;
actual post-load header values require their own validation. Destroy invokes
ExitProperties on this block then frees its byte array10127c98..10127cb8.
It is class-owned default storage, not a separately registered map/spawned Actor.

ObjectToString10102f90->10131f00 evaluates its operand against FFrame.Object;
GetPathName101027ac->1014fc10 recursively uses Outer+18 and Name+20. An initialized
default block stringifies None while remaining a distinct nonnull UObject. Two
class defaults may stringify identically but must not compare equal. A class
export path is an observably incorrect substitute identity.

## Receiver, frame object and static dispatch

ClassContext's selector uses FFrame.Object, its child the default-block receiver.
Ordinary Context's selector uses the incoming opcode receiver, its child the
selected object receiver. Both preserve FFrame. Self returns the opcode receiver;
InstanceVariable uses receiver+property offset; DefaultVariable uses receiver's
Class->Defaults. Let/LetBool operands and ArrayElement's index instead use frame
Object. ArrayElement's base retains the incoming opcode receiver. This is not
uniform recursive-context inheritance.

Virtual10102ce8->101301e0, Final10102cb1->10130280 and Global101029c3->10130300
pass the incoming receiver to CallFunction+44 without Static-flag retargeting.
FindObjectField1010322e->10150b50 uses a live state node only for nonglobal lookup,
otherwise receiver.Class; initialized defaults have no StateFrame. CallFunction
101013cf->1013f4e0 evaluates script arguments with caller FFrame.Object at1013f5d6,
constructs the callee frame with Object=callee receiver1013f590..1013f593, and
calls ProcessInternal with that receiver1013f61d..1013f620. Its complete body
through1013f686 does not retarget based on FUNC_Static0x2000. Static is not
permission to treat a UClass export as the receiver/default object.

## Result destinations and reflected lvalues

ClassContext's null branch clears property/address globals, skips encoded
skip+3, and zeroes exactly the encoded byte count only when Result is nonnull.
This differs materially from returning Nothing and coercing it to typed zero.

- Let10102f95->1012fe40 evaluates LHS with nil Result and captures its address;
  RHS receives that address as its result destination1012feb3..1012fec3. A null
  RHS with zero size0 preserves the existing target. A null LHS uses a zeroed
  12-byte scratch destination, still evaluating RHS effects, without real writes.
- ArrayElement101028dd->1012ef90 evaluates its base with nil Result1012efe3..
  1012eff8. Missing reflected property OR address returns without writing the
  outer Result1012efff..1012f010. Thus a null ClassContext inside MultiSkins[i]
  does not zero the outer destination despite its encoded size32. Valid indices
  clamp, advance the reflected address and copy one element1012f012..1012f07d.
- BoolVariable1010284c->1012f300 captures reflected BoolProperty, evaluates its
  variable child with nil Result, restores property metadata and yields logical
  0/1 from address plus bitmask1012f310..1012f35d. A null address yields false.
- LetBool101011ef->1012ff30 independently prezeros its RHS integer temporary.
  Missing LHS address still permits RHS effects but skips assignment; valid LHS
  changes only its reflected bitmask. No-write RHS therefore assigns false to a
  valid target, unlike Let's zero-size String preservation.

The implementation needs separate frame/opcode receivers, a distinct class-owned
default handle and an explicit result sink/outcome (written value, no write, or
zero bytes), independent of lvalue property/address state. An EvalInto model can
represent nil destinations for array/Bool lvalue evaluation and forwarded Let
destinations. Preserve skip/zero metadata and prove supported portable layouts;
unaudited partial byte writes must refuse explicitly. Persistence, GC, references,
default handles, UI classes and call dispatch must expand together. No ClassContext
opcode implementation or successful original-world startup is claimed here.
