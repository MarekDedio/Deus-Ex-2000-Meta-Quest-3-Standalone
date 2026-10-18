# Original Engine defaults and native field links (2026-10-18)

Read-only static audit of the owned GOTY binaries. No original DLL/EXE was
loaded or executed and no owned game file was changed. The only authored file
in this audit task is this document. Addresses and offsets below identify the
original PE32 instructions/layout; they are not portable pointers or host C++
layout definitions.

SHA256 values were checked during inspection and again before/after writing
this report; the originals remain unchanged:

| Original file | SHA256 |
| --- | --- |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |

This extends ORIGINAL-NATIVE-METADATA-REGISTRATION.md,
ORIGINAL-NATIVE-DEFAULT-CONFIG.md and ORIGINAL-ENGINE-CYLINDER.md. It supplies
evidence for a typed equivalent-final default/field phase, not proof that the
port has implemented that phase, Engine.Init, movement, falling or crate loot.

## A class-default block does not run the instance constructor

`UObject.InitClassDefaultObject` thunk10101226 targets10152300..1015236D.
The complete normal path:

- Clears the28-byte UObject header at10152323..1015232F.
- Stores the supplied actual UClass into default.Class+24 at1015233A,
  default.Index=-1 at1015233D and the observed class virtual implementation
  at default+0 at10152344..10152349.
- Obtains the actual native destination size through the class virtual+64,
  reads the actual superclass at Class+28, and calls InitProperties through
  thunk1010102D at1015234B..10152355 with that superclass and null explicit
  template inputs.
- Returns without calling the native Engine/GameEngine instance constructor.

`InitProperties101520D0` uses the actual selected class default-buffer pointer
and byte count at Class+4D4/+4D8 (10152123..10152144), copies payload after the
28-byte UObject header (10152149..10152176), and zeroes the remaining extended
destination tail (10152185..101521A7). Constructor-linked properties are
handled separately at101521B1..10152200 using actual property+54 links and
virtual deep-copy slot90. Copying arrays as aliased native pointers would not
implement this operation.

For the audited native chain, the following payload consequences therefore
follow from the superclass/default-copy/zero-extension sequence. Native sizes,
empty static-constructor provenance, serialized tags and concrete constructor
bodies are recorded in the companion audits above.

| Default class/native size | Payload before applicable config |
| --- | --- |
| Core.Object /28 | No payload beyond the separately initialized UObject header; no authored default tags |
| Core.Subsystem /2C | Object defaults plus zero extension+28..2B |
| Engine.Engine /68 | Inherited Subsystem defaults plus zero extension+2C..67; actual authored tags CacheSizeMegs+5C=2 and UseSound+60 bit1=True; StaticConstructor explicitly sets CurrentTickRate+64=0 |
| Engine.GameEngine /D0 | Inherits the prepared Engine payload and zero-extends+68..CF before native array declarations/config |
| Extension.GameEngineExt /D0 | Copies prepared GameEngine defaults, then applicable own config |
| DeusEx.DeusExGameEngine /D0 | Copies prepared GameEngineExt defaults, then applicable own config |

Register compares a class's staticConstructor+4E4 to its superclass's callback
at101278A7..101278BE and skips the invocation when equal. Extension and DeusEx
share GameEngine's audited StaticConstructor; this does not create another
pair of array fields under each subclass.

In particular GameEngine's+68/+6C/+70 and complete FURL storage+74..B7 start
zero in the default block. Its default FURL.Valid at+B4 is0. Do not assign the
instance constructor's Valid1 to this class-default block.

The real UGameEngine instance constructor10388F40..10388F95 first calls
UEngine's constructor through10302923 to1037D5A0, then constructs FURL at+74
with a supplied nonnull empty string at10388F62..10388F72. The audited fresh
session FURL constructor path gives empty protocol/host/map/portal/options,
port0 and Valid1. UEngine's constructor installs UObject/FExec virtual
implementations and does not initialize Cylinder. Neither constructor writes
the three slots+68/+6C/+70, which retain copied defaults until later explicit
session/map operations.

The meaning of+6C/+70 was not independently established in this audit; do not
guess names such as Entry/PendingLevel. The actual loaded Level at Engine+68
and native Level.Engine assignment are separately established by LoadMap
1038D094..1038D09F in ORIGINAL-ENGINE-CYLINDER.md. Those later writes must not
be simulated merely to claim Engine startup.

## Actual GameEngine array and inner field construction

`UGameEngine.StaticConstructor10388FC0..10389110` creates actual native field
objects, not serialized PropertyDescriptor records:

| Field | Actual class | Actual Outer | Allocation size | Requested UObject flags | Cpp data offset | Initial property flags | Category |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ServerActors | Core.ArrayProperty | defaultReceiver.GetClass, actual Engine.GameEngine |60 |4 |B8 |4000 |Settings |
| ServerPackages | Core.ArrayProperty | same actual GameEngine class |60 |4 |C4 |4000 |Settings |
| Each distinct StrProperty0 inner | Core.StrProperty | its actual ArrayProperty |5C |4 |0 after Link |0 before Link |None |

All numbers in the table are hexadecimal. Both arrays have ArrayDim1 and
ElementSize0 before linking. Array flags4000 are not their final linked flags.
The two inners have the same unqualified literal name but distinct actual
Outers and identities; sharing one inner object would be incorrect.

ServerActors allocation/Cpp construction is1038900F/10389031; ServerPackages
is10389096/103890B8. Each array's actual Inner+5C pointer is assigned at
10389085/10389100. The actual class's Children order becomes
ServerPackages -> ServerActors -> priorChildren. Preserve or validate the
actual prior chain; do not infer it by sorting display paths.

UProperty's Cpp constructor1010276B ->10164FA0..10165010 calls UField with
null SuperField, writes ArrayDim+34=1, supplied property flags+3C, category
FName+40 and supplied data offset+48. It calls actual Outer virtual+54 at
10164FE9..10164FFC. UClass dispatches that to UStruct.AddCppProperty10125FD0,
which prepends to Children and puts the previous head into property.Next+2C.
It does not deduplicate/reconcile existing fields.

The inners deliberately use DEFAULT UStrProperty construction:
10101A87 ->10114520..10114531 -> UProperty default1010283D ->10164F80..10164F9A.
That calls UField(Super=null) through10102B85 ->10125BA0..10125BB8 and writes
ArrayDim+34=1. No AddCppProperty call occurs. The shared empty metadata static
constructor/default-copy provenance supplies zero native-tail defaults before
these constructors. Thus each inner starts with Super+28=0, Next+2C=0,
HashNext+30=0, ElementSize+38=0, property flags+3C=0, category+40=None,
replication words+44/+46=0, offset+48=0 and native property-list pointers
+4C/+50/+54/+58=0. The inner is not independently inserted into Class.Children.

ArrayProperty.AddCppProperty101689A0..10168A1C is distinct: it asserts the old
Inner is null and the new property is nonnull, then assigns Inner+5C. It does
not prepend Class.Children. The GameEngine constructor directly installs the
Inner pointer as observed above.

## Link effects and actual property lists

Str.Link10167640..101676A4:

- Writes ElementSize+38=0C at10167670.
- Writes offset+48=align4(actual Outer.GetPropertiesSize) at10167677..10167681.
- Unless property flags contain1000, ORs400000 at10167684..10167691.

An inner's Outer is the actual UArrayProperty. Its inherited
UField.GetPropertiesSize10101046 ->10125D30..10125D32 returns0. Therefore each
GameEngine inner ends with offset0, ElementSize0C, ArrayDim1, flags400000,
categoryNone and no independent class-list linkage.

Array.Link101683B0..1016842C first preloads/links actual Inner at
101683DB..101683F2, writes ElementSize+38=0C at101683F8, computes its temporary
aligned offset and ORs400000 unless1000 is set (1016840C..10168419). GameEngine
arrays' linked property flags are consequently404000, not4000.

The enclosing UStruct.Link body is10126060..1012654E. Its Link0 branch
1012613C..10126178 saves each own property's declared offset, calls its Link,
then restores that offset at1012616D. Thus the GameEngine Cpp offsetsB8/C4
remain authoritative; Array.Link's temporary offset does not replace them.
Do not run a recomputing Link1 layout over these native fields.

UStruct.Link's actual property iterator thunk10102F9A ->1012B1E0..1012B239
walks current Children/Next, checks actual Core.Property class ancestry and,
when the chain ends, proceeds through actual Super.Children. It does not use
the flattened qualified-path dictionary as field order.

Native list heads and links are separately established:

| List | Struct head | Property next | Eligibility |
| --- | --- | --- | --- |
| PropertyLink |+60 |+4C |Actual properties in iterator order |
| ConfigLink |+64 |+50 |Property flags4000 |
| ConstructorLink |+68 |+54 |Property flags400000 |

Head pointers are selected at1012622E..10126243. Per-property links are written
at10126466..1012649E; all three lists are null-terminated at101264A9..101264B3.
GameEngine's own prefix is ServerPackages ->ServerActors, followed by eligible
inherited properties in their original iterator order. Both arrays belong to
all three lists after linking. The inners do not independently appear in these
class lists; linking an inner alone does not build a separate Struct list on it.

## Implementation boundary and still-unproven work

The port eagerly loaded Engine.Engine's original serialized fields already.
Its equivalent-final preparation must preserve those actual identities,
serialized flags/category/Next and existing default-object identities. Replaying
Engine.StaticConstructor over the loaded CacheSizeMegs/UseSound can reconstruct
and prepend the same objects, producing cycles; it is not a safe merge.

An aligned next phase can attach typed immutable native default payloads to
the actual indexed classes/default objects and create the native-only GameEngine
array/inner field objects with actual classes/Outers/links. It should apply
actual inherited config against actual UObject lookup and freeze per-instance
deep copies. A genuine ImportText lookup miss writes None; unavailable legacy
driver classes must not be fabricated or implicitly loaded. This does not
itself recover the packed launcher's allocation inputs or claim a complete
GConfig implementation.

The resulting state is directly consumable by explicit portable Engine and
Transient/Primitive construction, real Engine->Cylinder GC reachability and
native Level.Engine binding. The next useful production consumer is the
currently refused Engine-owned Cylinder selection in NativeActorPrimitive,
then Training collision hash/Move/support/startup/falling and live crate damage.
No additional campaign-map integration is needed to prove this Training path.
Full UEngine.Init, configured devices, player possession, physics callbacks,
gameplay/save integration and headset acceptance remain separate required work.

## Reproduction: static tools only

From PowerShell, with objdump and the owned game path adjusted if necessary:

```powershell
$auditObjdump = 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe'
$auditCore = 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
$auditEngine = 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
Get-FileHash -Algorithm SHA256 -LiteralPath $auditCore,$auditEngine
& $auditObjdump -p $auditCore
& $auditObjdump -p $auditEngine
& $auditObjdump -d -Mintel --start-address=0x10152300 --stop-address=0x10152370 $auditCore
& $auditObjdump -d -Mintel --start-address=0x101520d0 --stop-address=0x10152235 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10127800 --stop-address=0x101279d0 $auditCore
& $auditObjdump -d -Mintel --start-address=0x1037d5a0 --stop-address=0x1037d6ed $auditEngine
& $auditObjdump -d -Mintel --start-address=0x10388f40 --stop-address=0x10389111 $auditEngine
& $auditObjdump -d -Mintel --start-address=0x10114520 --stop-address=0x10114532 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10164f80 --stop-address=0x10165011 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10125ba0 --stop-address=0x10125bb9 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10125d30 --stop-address=0x10125d33 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10167640 --stop-address=0x101676a5 $auditCore
& $auditObjdump -d -Mintel --start-address=0x101683b0 --stop-address=0x1016842d $auditCore
& $auditObjdump -d -Mintel --start-address=0x101689a0 --stop-address=0x10168a1d $auditCore
& $auditObjdump -d -Mintel --start-address=0x10126060 --stop-address=0x1012654f $auditCore
& $auditObjdump -d -Mintel --start-address=0x1012b1e0 --stop-address=0x1012b23a $auditCore
Get-FileHash -Algorithm SHA256 -LiteralPath $auditCore,$auditEngine
```

These commands parse original static bytes and PE tables only. They do not load
the DLL into a process, resolve a callable original function or execute it.
