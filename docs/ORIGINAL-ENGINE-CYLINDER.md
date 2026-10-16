# Original Engine-owned collision primitive

Read-only static audit of the owned GOTY Engine.dll and Core.dll, 2026-10-16.
No commercial executable/DLL was loaded or executed; no original file was
changed. Preferred PE32 virtual addresses below are evidence, not portable
object addresses or a suggestion to allocate a fabricated native instance.

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Extension.dll | `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C` |
| DeusEx.dll | `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04` |

These hashes were recomputed during this audit. The primitive commonly
called Engine.Cylinder below means the **actual UPrimitive object at native
UEngine+40 selected by original AActor.GetPrimitive**. A script-property name
or an invented Model/Mesh object is not used as authority for its existence.

## Construction in original UEngine.Init

The named Engine export UEngine.Init, export-table index1139, RVA3DA0,
thunk `10303DA0`, targets `1037FFB0`. Its receiver is retained in EDI at
`1037FFD7`; EBX is explicitly zeroed at `1037FFD9` and remains the zero
used in the following original construction at `10380811..10380871`:

1. `10380811` pushes zero object flags. `10380812..1038081F` allocates the
   by-value FName slot and calls actual Core FName(EName) with integer0.
   This is not a string-name lookup or a synthetic "Cylinder" FName.
2. `10380825` calls actual UObject.GetTransientPackage; its result is the
   Outer passed at `1038082B`.
3. `1038082C` passes allocation size54 hex; `1038082E` calls the named
   UPrimitive.operator new export486, thunk `10302711` to `1031E190`.
4. That operator uses actual UPrimitive.PrivateStaticClass `1058D368`
   at `1031E1AB` and calls imported UObject.StaticAllocateObject at
   `1031E1B0`. The original size argument is not a reflected Actor scale,
   cylinder radius or collision extent.
5. If allocation returned nonnull, `1038083C..1038083E` calls the actual
   UObject default constructor. `10380844..10380862` initializes the
   primitive's native bound values: Float3 min at28/2C/30, Float3 max at
   34/38/3C, valid Byte at40, and Float4 sphere at44/48/4C/50, all zero.
   `10380865` installs actual exported UPrimitive vtable `1042655C`.
6. If allocation returned null, `1038086D` explicitly chooses null instead.
   Both branches converge and `10380871` stores the result into Engine+40.
7. `1038086F` restores ECX to the Engine receiver. `10380874` calls
   UObject.AddToRoot **on Engine**, not on the newly allocated primitive.

The exact resolved original imports are:

| Engine IAT slot | Actual imported symbol | Additional original Core evidence |
| --- | --- | --- |
| `10599BB8` | `FName::FName(EName)` | thunk `10102919` to `101099C0`; copies supplied enum integer into FName, ret4 |
| `10599DB4` | `UObject::GetTransientPackage()` | thunk `10102450` to `101573E0`; returns actual GObjTransientPkg at `102A2660` |
| `10599B58` | `UObject::UObject()` | actual named import, not UClass or UPrimitive constructor |
| `10599B70` | `UObject::StaticAllocateObject(UClass*, UObject*, FName, flags, template, error, subobject)` | operator passes UPrimitive class `1058D368` |
| `10599DE8` | `UObject::AddToRoot()` | thunk `1010345E` to `10157400` |
| `105999F4` | `UObject::RemoveFromRoot()` | thunk `10101F9B` to `101574B0` |
| `10599B10` | `UObject::Serialize(FArchive&)` | Engine and Level serializers call this original base |
| `10599AE0` | `UObject::Destroy()` | Engine destruction calls this original base |

The PE import table, not a guessed IAT function signature, supplies those
names. Core exports GObjTransientPkg itself at RVA1A2660; its getter performs
only the observed pointer read, not allocation or per-level construction.

## Engine root and primitive GC reference

UEngine.Serialize export1491, RVA2A1D, thunk `10302A1D`, targets
`10380DF0..10380E63`. After actual UObject.Serialize at `10380E21`, it feeds
addresses of **Engine+40, +44, +48 and +4C**, in that order, through
FArchive's object-reference virtual slot18 at `10380E27..10380E50`.
The primitive's Engine+40 reference is explicitly included; it is not only
a render cache or an untraced raw owner field.

Together with AddToRoot on the Engine, this supplies the original reachability
chain: rooted Engine -> Engine.Serialize's object reference -> UPrimitive.
It does not prove that a port has initialized an Engine, registered this
session object, or attached it to an actual native Level. Nor does it require
adding an unrelated per-map UPrimitive or individually rooting a cylinder.

UEngine.Destroy export879, RVA2EF0, thunk `10302EF0`, targets
`10380C20..10380C8C`. It calls RemoveFromRoot(engine) at `10380C48`, zeros
Engine+4C/+48/+44 at `10380C4E..10380C54`, performs other engine cleanup,
then calls actual UObject.Destroy at `10380C76`. This body does **not** zero
Engine+40, directly delete Cylinder, or invoke a Cylinder-specific Destroy.
Removing the Engine root is not itself a claim that immediate GC collection
occurs inside this method. Core.RemoveFromRoot's body scans the actual root
array and removes matching receiver entries; later GC/destruction is a
separate lifecycle boundary.

## Actual Level-to-Engine binding

ULevel(UEngine*,int), named export182, RVA3841, thunk `10303841`, targets
`1039CAC0`. It loads the explicit Engine argument at `1039CB19` and stores
it into Level+40 at `1039CB24`. Thus this pointer is native constructor
state, not inferred from actor Owner, reflected LevelInfo or map filename.

A loaded Level also receives the current engine explicitly. Actual
UGameEngine.LoadMap export1277, RVA33A5, thunk `103033A5`, targets
`1038C1F0`. Its receiver is saved at `[ebp-14]` at `1038C21B`. At
`1038D094..1038D09F`, it reloads that receiver into ESI, reads the actual
loaded Level at Engine+68, and writes ESI into that Level+40:

```
1038D094 mov esi,[ebp-14]  ; actual UGameEngine receiver
1038D09C mov ecx,[esi+68]  ; loaded ULevel
1038D09F mov [ecx+40],esi  ; native Level.Engine binding
```

This observed LoadMap assignment is independent of the ULevel constructor
evidence. It does not imply that constructing portable package tables or a
dormant map has executed LoadMap or established a native Engine lifetime.

ULevel.Serialize `10301343` to `1039D560` calls ULevelBase.Serialize
`10302644` to `1039C5F0` first. The inspected ULevelBase serializer explicitly
serializes its actor array, URL, and selected other native references, but
does not archive Level+40 in that body. Do not manufacture a serialized
Engine pointer or reconstruct Cylinder from checkpoint actor properties.
This audit does not claim exhaustive inspection of every editor/travel
Level construction path.

## AActor.GetPrimitive exact lazy precedence

Named export1105, RVA3EC2, thunk `10303EC2`, targets the complete short body
`1034C9A0..1034C9C0`:

1. Read Actor+178 (Brush/UModel). If nonnull, return that exact object.
2. Otherwise read Actor+174 (Mesh). If nonnull, return that exact object.
3. Otherwise read Actor+AC (native XLevel), then Level+40 (Engine), then
   Engine+40 (the constructed default UPrimitive), and return it.

The fallback sequence is `1034C9B4..1034C9BD`. There is no DrawType,
hidden/render state, actor-class-name, collision-radius or extent test in
this body. Later LineCheck dispatch is on the selected primitive's actual
vtable and must not be replaced with a cylinder just because an actor is
visually simple. Conversely the cylinder itself is a session Engine-owned
UPrimitive, not a UMesh guessed from the Actor's dimensions.

## Remaining portable authority boundary

The existing pure original cylinder LineCheck arithmetic does not establish
the original object's identity. A production provider still needs an actual
initialized Engine-owned UPrimitive lifetime and an actual Level.Engine
binding, plus selected Brush/Mesh identity and virtual implementation checks.
None of those are created by this read-only audit, the coordinate snapshot
API, the collision hash's detached packets, or a successful scalar test.
No Engine constructor/init implementation, new native flags, live movement,
world startup, APK build or headset acceptance is claimed here.

## Native-only class and concrete Engine admission

Further actual metadata/native registration evidence matters before implementing
that bridge. Engine.u's Engine.Engine export3349 derives Core.Subsystem and
has Children5284. Its actual Cylinder property is export5279, payload5AFCC,
length14: Core.ObjectProperty, dimension1, flags0, Category None, referenced
type **Engine.Primitive**, Next5278 (Client). It is a real typed original
property, but Engine.Primitive has no serialized Class export in this Engine.u.
Do not invent a missing serialized class descriptor or infer primitive identity
from a null import resolution.

The actual native Primitive UClass initializer `103D8200..103D824A` registers
native size54, supplied class flags0 and superclass Core.Object, with Primitive/Engine
name identity. Its concrete native registration must be represented explicitly
if the port creates this native-only class; a copied Mesh/Model UClass is not
equivalent.

Conversely actual UEngine's native initializer `1037D230..1037D281` passes
class flags`0xD` at `1037D271`, native size68 and Core.Subsystem superclass.
Original StaticAllocateObject `101579C0` tests the **current** abstract flag
bit1 at `10157AA8`, not a remembered constructor argument. Native constructor
baseline1F is not Engine.Engine's loaded serialized flags11E: loading replaces
that state as detailed below. A bridge needs the actual configured concrete
GameEngine/EditorEngine class and real owned session lifetime, not an
assumption that Cylinder declaration or a constructor flag is allocation authority.
UEngine's default constructor `1037D5A0` installs its UObject/FExec vtables;
it does not initialize Cylinder. The Init phase above is distinct from default
construction, class defaults, viewport binding and loading map package tables.

Reproduce with static tools only: `objdump -p Engine.dll` for the named
exports/imports; `objdump -d -Mintel --start-address=... --stop-address=...`
for each stated thunk/body, and the same operations on Core.dll. Resolving
the listed Engine IAT slots reads the original import thunk DWORD and the
corresponding IMAGE_IMPORT_BY_NAME string; it does not load either DLL.

## Core-session allocation and class registration details

Core's native UClass constructor `1012866F` ORs supplied class flags with12
hex. Consequently Primitive's supplied0 becomes12, Engine's suppliedD
becomes1F at native construction, and GameEngine's suppliedC becomes1E. Confusing
supplied constructor arguments with the effective flags would misstate
admission. UClass+49C is **ClassWithin**, not a default or registration pointer:
the constructor stores the explicit Within argument at `101286B6..101286D0`.

StaticAllocateObject `101579C0` requires nonnull ClassWithin at `10157A0D`,
native constructor+4E0 at `10157A2E`, and valid registration at
`10157A4F..10157A5E` (UClass UObject.Index+4 is not minus1 unless the explicit
load-context global102A2654 permits it). Before unique naming, the supplied
Outer's actual concrete-class/Super ancestry must satisfy ClassWithin at
`10157B37..10157BB8`. Primitive's Within is actual Core.Object, which the real
transient package satisfies. Do not turn string path membership into that
native registration or Outer authority.

Actual Core.StaticInit `101011C7` to `10152790` constructs the transient
package separately: UTF16 literal Transient at101AE600 is passed to
FName(Add1) at `10152816..1015281C`; UPackage allocation at
`10152821..10152824` uses native size34, Outer None and flags0, followed by
UPackage constructor thunk10101A9B to1015A3E0. The actual object is stored in
GObjTransientPkg102A2660 at1015283D and AddToRoot is called on that package
at10152843. It is not the loaded map package and is not recreated for each map.

NAME_None in primitive allocation invokes actual MakeUniqueObjectName at
`10157BB9..10157BD3`. The already audited `quest_object_names.h` models its
concrete-UClass+488 counter, any-class collision check in the actual Outer,
and candidate-consumption order. A future session allocation must retain that
counter across maps and journal allocation/index/name state on portable
failure; fixed Primitive0 or a fresh per-map counter is not equivalent.
StaticAllocateObject establishes Outer/name/class/flags and initializes
native-tail defaults/zeros through InitProperties101520D0 before the primitive
constructor installs its bounds/vtable.

## Configured concrete engine chain

The owned DeusEx.ini selects `DeusEx.DeusExGameEngine` in the GameEngine key
of Engine.Engine. There is no serialized Class export for that class,
Extension.GameEngineExt or Engine.GameEngine in their respective packages.
This is native-only metadata, not a corrupt missing script descriptor. Actual
static registrations establish this chain:

```
DeusEx.DeusExGameEngine -> Extension.GameEngineExt -> Engine.GameEngine
  -> Engine.Engine -> Core.Subsystem -> Core.Object
```

Engine.GameEngine PrivateStaticClass10587F50 is initialized at
`10388660..103886C5`: native UClass constructor call103886B0 through named
Core import10599DE0, staticConstructor10301D70 to10388FC0, internal native
constructor103029FF to10347A40, requested object flags04084004 at1038866C,
config System10479E58, package Engine1047A148, name GameEngine1047E48E and
zero16-byte Guid. ClassWithin is actual Core.Object.PrivateStaticClass through
import105999F8 at1038868F..94, superclass actual UEngine10583C40 at10388695,
supplied class flagsC at1038869D, native instance sizeD0 at1038869F, native
constructor enum0 at103886A4, receiver10587F50 at103886A8.

Extension.GameEngineExt PrivateStaticClass1009E100 (StaticClass1001B6A0)
is initialized at `100260F0..10026159`: native sizeD0, supplied class flagsC,
RF flags04084004, native constructor1001BA00, superclass import100592F4
explicitly UGameEngine.PrivateStaticClass. Its original UTF16 class/package/
config strings are GameEngineExt10098526, Extension10097C9C and System10097950;
native UClass constructor import100590F4 and UGameEngine.StaticConstructor
are named imports, not inferred from similar class names.

DeusEx.DeusExGameEngine PrivateStaticClass10039BD8 (StaticClass10012430)
is initialized at `1000EC20..1000EC7C`: native sizeD0, supplied class flagsC,
RF flags04084004, native constructor100126E0, superclass import10027418
explicitly XGameEngineExt.PrivateStaticClass. Its class/package/config strings
are DeusExGameEngine1003447A, DeusEx10034308 and System100341CC; native UClass
constructor import10027028 and UGameEngine.StaticConstructor are named imports.
These registrations do not themselves create a portable engine object.
Their requested UClass object flags are not Engine instance-allocation flags.
The launcher's concrete instance allocation/Outer/name/flags remain a separate
authority boundary; do not substitute the class-registration arguments there.

The bounded portable INI reader now supports Root and GameEngine separately,
without a hardcoded fallback. The explicit class-registration phase binds the
audited native-only chain and validates the inherited Engine.Cylinder field;
it creates no Engine/Primitive instances. Future concrete engine admission
must establish the separate session instance roots/GC edges and constructor
state, not treat successful UClass registration as instance initialization.
Substituting Engine.Engine for the selected concrete GameEngine, or
manufacturing serialized UClass descriptors to evade that work, is not an
implementation.

Crucial phase correction verified against original Core.UClass.Serialize:
`10128123..10128130` passes this+484 and byte count4 directly to the archive's
Serialize slot4. Loading unconditionally replaces ClassFlags without masking
or restoring the native constructor flags. Owned Core.Object/Core.Subsystem
serialized flags are112/112; Engine.Engine is11E, without Abstract bit1.
The v68 path does not take the <=61 legacy extra-DWORD/object-flags branch.
The remaining loading/default/config work never restores that abstract bit.
UClass.Register inheritance `101278F2..1012790C` applies
`currentFlags |= superclass.currentFlags & 4EC`; the five native-only binding
baselines12/12/1E/1E/1E remain unchanged for these known bases.

Likewise04084004 is a proven native UClass constructor object-header baseline:
UClass10128630 through UState10125EF0 reaches UObject1014F4C0, which ORs Native
04000000 at1014F4F1 and stores ObjectFlags+1C at1014F4F9. The native type-binding
snapshot labels it registrationObjectFlags, not a claim that full Register/
defaults/config/post-load or reflected Core.Class header storage is implemented.

Finally the original UEngine.Init prefix also calls FURL.StaticInit:
`10380765` to export1661 thunk10303328 to103F6C50, reading actual global
GConfig through import10599F38. Names/localization, memory-stack/cache and URL
configuration precede the cylinder construction. An explicitly named
collision-service initialization subphase can implement only that subphase;
it must not claim full UEngine.Init, GameEngine startup, BeginPlay or possession.

## Constructor and fresh FURL phase (2026-10-17)

Further read-only inspection closes the default-constructor chain. UGameEngine
internal constructor10347A40 dispatches through10303EE5 to10388F40, which calls
UEngine constructor10302923 to1037D5A0. The latter calls actual UObject
constructor10599B58 and installs its UObject/FExec virtual implementations;
it does not write Cylinder+40 or call Init. UGameEngine then constructs FURL
at Engine+74 with actual nonnull empty UTF16 input10485C3C through103037A6
to103F7850 and installs its own two vtables10428DFC/10428DF8.

Extension internal constructor1001BA00 to10026190 calls the named imported
UGameEngine default constructor through100592FC, then installs its two
vtables1005C264/1005C260. DeusEx internal constructor100126E0 to1000ECD0
calls the named imported XGameEngineExt default constructor through10027434,
then installs10027D90/10027D8C. Neither subclass has a hidden Init/startup call.

FURL construction is substantive native state, not interchangeable with a
vtable/class label. The complete empty-input path103F7850..103F7A75 captures:

| FURL offset / Engine offset | Native value |
| --- | --- |
| 00 / 74 | Copy current global DefaultProtocol1058F3E0 |
| 0C / 80 | Copy current DefaultHost1058F390 |
| 18 / 8C | Copy current signed DefaultPort1058F3EC |
| 1C / 90 | Empty Map from supplied nonnull empty string, not DefaultMap |
| 28/2C/30 / 9C/A0/A4 | Empty options array |
| 34 / A8 | Copy current DefaultPortal1058F380 |
| 40 / B4 | Valid1 |

Engine CRT initializer table10479964..10479984 contains103F68F0,103F6950,
103F69B0,103F6A10,103F6A70,103F6AD0,103F6B30,103F6B90,103F6BF0. Each calls
actual FString default constructor import10599A40 to Core101027CF to101072F0,
setting Data/Num/Max0 for its global string. DefaultPort is in the PE zero-fill
tail: data sectionVA10479000, rawD000, virtual11F2F4, Port offset1163EC.
Consequently an explicitly fresh process/session constructor before Init
captures empty protocol/host/portal/map/options, port0 and Valid1. It must not
prematurely copy later INI defaults into that already-constructed URL.

FURL.StaticInit103F6C50..103F70D3 updates globals in order Protocol,
ProtocolDescription,Name,Map,LocalMap,Host,Portal,MapExt,SaveExt,Port. Every
lookup uses actual GConfig virtual slot14, section URL1048272C, key literal
and filename0. Returned NUL-terminated UTF16 values are copied without URL
normalization or local fallback. Port uses appAtoi10599DD8 to Core1010111D to
101235B0 to named MSVCRT _wtoi102AB514, not strict decimal validation/clamping;
overflow remains a CRT-profile boundary. The concrete config implementation's
missing-key, duplicate and filename0 semantics are not established here.
StaticInit changes global defaults, not the Engine instance URL already made.

UPackage construction is also not vtable-only: Core1015A3E0 installs101753A0
then calls BindPackage101017E9 to10152FF0. With Transient's zero Outer/handle/
attempted baseline, it sets Package+2C attempted1 at10153082 and stores the
platform native-library probe result in Package+28 at1015308D. A portable
static-native registry binding policy must represent this attempted/binding
result explicitly; it must not pretend an original DLL was loaded. Package+30
is PackageFlags; these three native-tail fields are not GC UObject edges.

The owned launcher DeusEx.exe is packed (64000 bytes), SHA256
E7217090C435C7AE01F875A08FA25C10303CD56203C25A29C6E77FB041B992A2.
Entry10901000 jumps to unpacker10967CE8; visible imports are only kernel32
LoadLibraryA/GetProcAddress/VirtualAlloc/VirtualFree. Its original concrete
Engine instance allocation arguments remain unproven. A documented explicit
portable embedding construction request is distinct from those unknown
launcher arguments, and must preserve all implemented constructor effects.
No commercial executable code was loaded or executed to obtain this evidence.
