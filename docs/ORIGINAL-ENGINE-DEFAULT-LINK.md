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

## Closed Field/Property/Str/Array/Primitive default profiles

The specific default-copy closure needed to allocate GameEngine's native fields
and its Engine-owned Primitive does not need a generic blank-CDO fallback.
The actual native registrations and shared empty static-constructor provenance
establish a closed, typed set:

| Actual class | Actual Super | Native size | Proven default payload |
| --- | --- | --- | --- |
| Core.Field | Core.Object |34 |Zero extension+28..33: SuperField+28, Next+2C, field hash link+30 absent |
| Core.Property | Core.Field |5C |Copied zero Field payload, zero extension+34..5B, including **ArrayDim0**, ElementSize0, property flags0, CategoryNone, replication words0, offset0 and absent native property links |
| Core.StrProperty | Core.Property |5C |Exact copied zero Property payload; no additional tail |
| Core.ArrayProperty | Core.Property |60 |Copied zero Property payload plus absent actual Inner+5C |
| Engine.Primitive | Core.Object |54 |Zero extension+28..53: min/max bounds, Valid byte, sphere and original padding all zero |

These are native default-block values BEFORE any instance constructor. In
particular Property/StrProperty/ArrayProperty default ArrayDim is0, not the1
written by a later UProperty instance constructor. Typed native field creation
must first consume these actual prepared defaults, then apply its audited
default/Cpp constructor and Link effects below.

Core native registration sites Field10125E50, Property10165650,
StrProperty10167BD0 and ArrayProperty10168C10 explicitly pass
UObject.StaticConstructor10101041 ->1014F690, whose complete body is RET.
Engine.Primitive registration103D8200..103D8250 passes the named imported
UObject.StaticConstructor through105999FC (loaded at103D8202, pushed at
103D820A), native size54, and actual Core.Object Super/Within. Its constructor
baseline flags12, and Field/Property/Str/Array baselines13/13/12/12, contain
neither Config4 nor Localized20; superclass inheritance mask4EC does not add
those flags for this actual chain. Their ordinary config/localized phases are
therefore gated no-ops, not unknown source values requiring a guessed INI.
There are no serialized replacement Class exports for these native-only
registrations, as established in the metadata/engine registration audits.

The actual Field/Property constructor distinction matters:

- Their abstract native registrations use the shared UObject internal
  constructor101017DA ->1010AAC0..1010AACD, which dispatches to actual UObject
  default construction through101021C6 ->1014F340 for a nonnull object.
  This does not authorize allocating abstract Field/Property instances; these
  profiles are needed as actual superclass/default ancestry.
- A concrete StrProperty registration supplies InternalConstructor10101AB4
  ->10114500..1011450D ->default constructor10101A87 ->10114520. That calls
  the UProperty default constructor and writes its distinct implementation.
- A concrete ArrayProperty registration supplies InternalConstructor101033DC
  ->10114C30..10114C3D ->default constructor10101AC3 ->10114C50..10114C61.
  It likewise calls UProperty default1010283D and writes its implementation.
  It does not independently initialize/attach Inner or call AddCppProperty;
  copied ArrayProperty defaults supply the absent Inner.
- GameEngine's arrays use the separately audited Cpp constructor rather than
  that ArrayProperty default constructor. Both routes consume the same actual
  prepared class defaults, but only the Cpp route writes Settings/4000/B8 orC4
  and prepends the array to actual Class.Children.

Primitive's distinct internal constructor is103023CE ->1031E1F0..1031E1FD,
which dispatches a nonnull object through10303530 ->1031E210..1031E245.
The concrete constructor calls actual UObject at1031E213, explicitly writes
zero min bounds+28/+2C/+30, max bounds+34/+38/+3C, Valid byte+40 and sphere
+44/+48/+4C/+50 at1031E219..1031E239, then installs actual Primitive virtual
implementation1042655C at1031E23C. It does not initialize a radius/height,
attach a Level, create an Engine or run collision queries. The corresponding
UEngine.Init inline construction1038083C..10380865 has the same observed
constructor effects. Padding+41..43 retains the proven copied default zeros;
do not confuse that with a wider constructor write to Valid.

A portable equivalent can therefore retain separate immutable typed payloads
for these actual class IDs, native sizes, actual Super/Within and shared-empty
static-constructor provenance; copy that payload when constructing concrete
Str/Array/Primitive objects; then apply only the audited constructor mutations.
The owner still supplies and validates the actual object header/index/class/
Outer/name, GC references and lookup publication. No legacy vtable, pointer or
32-bit memory image should be copied into host storage. This closed profile
does not grant default/instance authority to another native class merely
because its path or size resembles one of these five.

## Two remaining native classes for the inherited Engine field chain

The actual inherited field iterator also visits Core.Object.Name, whose actual
type is Core.NameProperty, and Engine.Engine.CurrentTickRate, whose actual
type is Core.FloatProperty. Genuine class identity for those fields requires
these additional closed native registrations; a field type spelling or generic
Core.Property wrapper is not a substitute. They are distinct from the existing
eleven-class metadata-domain contract.

| Actual UClass | PrivateStaticClass | Initializer / UClass constructor call | Native size | Supplied ->constructor class flags | Requested UClass UObject flags | Actual Super | Actual Within |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Core.FloatProperty |102A66F0 |10166A00..10166A5F /10166A4B |5C |0 ->12 |04084004 |Core.Property102A7AF0 |Core.Field101F2B18 |
| Core.NameProperty |102A57F0 |101675B0..1016760F /101675FB |5C |0 ->12 |04084004 |Core.Property102A7AF0 |Core.Field101F2B18 |

Both supply owning package literal Core101A7EF4, config literal System101A82C8,
zero16-byte GUID, native constructor enum0 and shared static constructor
10101041 ->1014F690 RET. Exact UTF16 names are FloatProperty101B13FA and
NameProperty101B15A2, verified from static bytes. Requested UObject flags are
the native UClass constructor baseline, not field-instance allocation flags.
Superclass inheritance of Property's13 through mask4EC does not alter either
class's12 baseline. No Config4/Localized20 default imports are enabled.

FloatProperty's distinct InternalConstructor101033FA ->10113630..1011363D
dispatches through default-constructor thunk1010312F ->10113650..10113661.
NameProperty's InternalConstructor10102153 ->10114170..1011417D dispatches
through101026BC ->10114190..101141A1. Each concrete default constructor calls
UProperty default1010283D, which supplies instance ArrayDim1, then installs its
distinct implementation (Float10175D44, Name10175F84). Neither constructor
creates another native field, attaches Class.Children or adds an extended tail.

Their CLASS DEFAULT payloads are instead exact same-size copies of the prepared
Core.Property5C payload: all bytes+28..5B zero, including ArrayDim0. The native
default-copy/shared-empty-static-constructor evidence above applies without
running these instance constructors. These two closed profiles grant no
authority to another unregistered Core metadata class.

Float.Link thunk10102C89 ->101666A0..101666F6 and Name.Link thunk10102BB7
->10167250..101672A6 each write ElementSize+38=4 and offset+48 aligned to
actual Outer.GetPropertiesSize; neither body mutates property flags or adds
the Constructor400000 flag. The enclosing Link0 offset-preservation rule
above still applies. Existing original serialized Name/CurrentTickRate field
objects must retain their own flags/category/Next/identity; preparing these
two UClasses is not permission to recreate the fields.

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
& $auditObjdump -d -Mintel --start-address=0x103d8200 --stop-address=0x103d8251 $auditEngine
& $auditObjdump -d -Mintel --start-address=0x1031e1f0 --stop-address=0x1031e246 $auditEngine
& $auditObjdump -d -Mintel --start-address=0x10380811 --stop-address=0x10380875 $auditEngine
& $auditObjdump -d -Mintel --start-address=0x10125e50 --stop-address=0x10125ea1 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10165650 --stop-address=0x101656a1 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10167bd0 --stop-address=0x10167c20 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10168c10 --stop-address=0x10168c60 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10166a00 --stop-address=0x10166a60 $auditCore
& $auditObjdump -d -Mintel --start-address=0x101675b0 --stop-address=0x10167610 $auditCore
& $auditObjdump -s --start-address=0x101b13fa --stop-address=0x101b1418 $auditCore
& $auditObjdump -s --start-address=0x101b15a2 --stop-address=0x101b15be $auditCore
& $auditObjdump -d -Mintel --start-address=0x10113630 --stop-address=0x10113662 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10114170 --stop-address=0x101141a2 $auditCore
& $auditObjdump -d -Mintel --start-address=0x101666a0 --stop-address=0x101666f7 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10167250 --stop-address=0x101672a7 $auditCore
& $auditObjdump -d -Mintel --start-address=0x1010aac0 --stop-address=0x1010aace $auditCore
& $auditObjdump -d -Mintel --start-address=0x10114500 --stop-address=0x10114532 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10114c30 --stop-address=0x10114ca5 $auditCore
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
