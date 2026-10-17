# Native Core metadata and field binding audit (2026-10-17)

Static read-only prerequisites for real native Engine defaults, not constructed
metadata/defaults/instances or a crate-physics completion claim. No commercial
DLL/EXE was loaded or executed. Original preferred PE VAs below identify static
source evidence, not portable pointers or host-size layout.

Owned Core.dll SHA256:
8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4.
Owned Engine.dll SHA256:
0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.

## Core registration closure

All numbers are hexadecimal. Flags are supplied ClassFlags -> native-constructor
baseline, not loaded serialized flags or proof of Register/default preparation.

| Core class | PrivateStaticClass VA | Initializer / UClass constructor call | Native size | Super | Within | Flags |
| --- | --- | --- | --- | --- | --- | --- |
| Field | 101F2B18 | 10125E50 /10125E9C | 34 | Object | Object | 1 ->13 |
| Struct | 101F2618 | 101273D0 /1012741B | 6C | Field | Object | 0 ->12 |
| State | 101F2118 | 10127760 /101277AE | 484 | Struct | Object | 0 ->12 |
| Class | 101F1C18 | 10128750 /1012879E | 4F4 | State | Package | 0 ->12 |
| Property | 102A7AF0 | 10165650 /1016569C | 5C | Field | Field | 1 ->13 |
| ObjectProperty | 102A61F0 | 10166FC0 /1016700B | 64 | Property | Field | 0 ->12 |
| ClassProperty | 102A5CF0 | 101671C0 /1016720B | 68 | ObjectProperty | Field | 0 ->12 |
| IntProperty | 102A70F0 | 10165FD0 /1016601B | 5C | Property | Field | 0 ->12 |
| BoolProperty | 102A6BF0 | 10166610 /1016665B | 60 | Property | Field | 0 ->12 |
| StrProperty | 102A52F0 | 10167BD0 /10167C1B | 5C | Property | Field | 0 ->12 |
| ArrayProperty | 102A48F0 | 10168C10 /10168C5B | 60 | Property | Field | 0 ->12 |

The initializers supply package Core101A7EF4, config System101A82C8, exact
unqualified UTF16 names, zero GUID and UObject flags04084004. The native
UObject constructor ORs04000000, leaving that baseline unchanged. Each supplies
its distinct instance InternalConstructor, not a generic metadata constructor.
All share UObject.StaticConstructor10101041 ->1014F690, a single RET.

UClass ENative constructor10102D9C ->10128630 ORs12 at1012866F, stores Within
at+49C (101286D0), instance/static constructor callbacks at+4E0/+4E4 and config
source at+4A0. Its native State constructor10125EF0 explicitly supplies genuine
Core.Class101F1C18 as the UObject metaclass (10125F05), including Class itself.
Class.Within is Package, not Object. Property classes' Within is Field, not
Object. Thus actual Package and metadata-class roots/relationships are required;
string cast profiles or class names are not indexed UObject capabilities.

Register101278F2..1012790C additionally ORs Super.ClassFlags&4EC. In particular
112&4EC=0: the loaded Core.Object100 bit does NOT propagate through that mask.
Constructor baselines still must not overwrite independently serialized class
flags. The existing original Engine.Engine loaded11E versus constructor1F
distinction remains intact.

## Engine config constraint-class closure

Complete owned-table scans distinguish actual Classes from same-name properties:
Core.u699 exports/342 names has no Language Class. Engine.u5622 exports/3574
names has Console Class but no RenderDevice, AudioSubsystem or NetDriver Class.
Engine entries237/5280/5282 with similar names are properties, not substitutes.

Actual Engine.Console export309 has ObjClass0, ObjBase-3 ->Core.Object,
serialized Outer0, UObject flags040F0004, payload offset94163/length2082. Its
serialized ClassFlags are13A (file offset94205), WithinCore.Object and
ConfigNameSystem. Reuse this serialized UClass; do not create a competing
native-only Console registration. This table does not infer a runtime Outer
Package from the serialized Outer0 field alone.

The four absent constraints have these genuine native registration records:

| Class | PrivateStaticClass VA | Initializer / UClass call | Size | Super | Within | Supplied ->constructor flags |
| --- | --- | --- | --- | --- | --- | --- |
| Core.Language | 1029AEE0 | 1015ACD0 /1015AD1C | 2C | Core.Object | Core.Object | 9 ->1B |
| Engine.AudioSubsystem | 1057FE68 | 10371000 /10371077 | 2C | Core.Subsystem | Core.Object | 5 ->17 |
| Engine.RenderDevice | 10584630 | 1037D3F0 /1037D444 | BC | Core.Subsystem | Core.Object | 5 ->17 |
| Engine.NetDriver | 10592448 | 10409F90 /10409FE4 | 90 | Core.Subsystem | Core.Object | D ->1F |

All supply UObject flags04084004, zero GUID, configSystem and owning package
Core/Engine. Static PE import thunks independently map Engine105999F8 to actual
Core.Object,10599CC4 to Core.Subsystem,105999FC to UObject.StaticConstructor,
10599CC8 to UObject.InternalConstructor and10599DE0 to the native UClass
constructor. These records establish constraint-class identity baselines, not
indexed configured subclasses, completed defaults or instance construction.

## Native field insertion and exact Engine declarations

UProperty Cpp constructor1010276B ->10164FA0..10165010 calls UField with null
SuperField; sets ArrayDim+34=1, property flags+3C, category FName+40 using
FNAME_Add, and offset+48. It calls actual Outer(+18)'s virtual+54.
UClass vtable101758F4+54 is10101DCF ->UStruct.AddCppProperty10125FD0:

```
old = Outer.Children
Outer.Children = property
property.Next = old
```

No duplicate detection, unlinking, name reconciliation or cycle prevention is
hidden in this operation.

UEngine.StaticConstructor1037D610..1037D6EC uses defaultReceiver.GetClass as
the two fields' actual Outer. CacheSizeMegs is UIntProperty size5C/data offset5C;
UseSound is UBoolProperty size60/data offset60, bitmask1. Both use UObject flags4,
Settings category and property flags4000. It zeros default CurrentTickRate+64.
Those constructor flags are NOT the final serialized4001 flags in Engine.u.

UGameEngine.StaticConstructor10388FC0..10389110 creates ServerActors first,
ServerPackages second. Each UArrayProperty object is60 bytes, actual Outer is
defaultReceiver.GetClass, UObject flags4, category Settings, property flags4000,
and FArray data offsetB8/C4. Each Inner is a5C-byte UStrProperty named
StrProperty0 (literal1047E638), Outer=its actual array property and UObject flags4.
The inners use DEFAULT UStrProperty construction10101A87 ->10114520, not the
Cpp-property constructor, so are not prepended independently to Class.Children.
Actual array.Inner+5C assignments are10389085/10389100. The class child order is
ServerPackages ->ServerActors ->priorChildren.

UArrayProperty.AddCppProperty101689A0 separately asserts old Inner is null and
new property nonnull, then assigns Inner; it does not prepend Class.Children.
Array.Link101683B0 preloads/links Inner and establishes12-byte FArray element
storage/alignment/constructor-link rules; Str.Link10167640 likewise uses12-byte
FString storage. These are distinct from descriptor-object size or proof of the
complete native Link/default/deep-copy phase.

## Same-name allocation is destructive, not a post-load merge

StaticAllocateObject10102EC3 ->101579C0 calls StaticFindObject with requested
class/exact Outer/name/Exact0 at10157C51..10157C69. An existing match must have
the exact requested class. It preserves only prior ObjectFlags&0C000000 plus
the prior index, calls the existing object's destructor with delete0 at
10157DB3..10157DB9, reconstructs its header/InitProperties and re-adds that saved
index at10157EB0..10157EFF. It does not create a second same-name field or
reconcile the owning Children chain.

Blindly rerunning Engine's Cpp constructor over eagerly loaded CacheSizeMegs/
UseSound could therefore reconstruct the SAME property then prepend it into a
chain already containing that pointer. Neither Register nor
InitClassDefaultObject resets the class's Children to make that replay safe.

Original chronology resolves the apparent conflict:

1. Native UStruct constructor10125EF0 starts Next/Children at0
   (10125F1D..10125F26). Native UClass construction delegates through it.
2. ProcessRegistrants10152A40 drains native registrations; ConditionalRegister
   1014FEA0 calls virtual Register only while Index==-1. UObject.StaticInit
   calls ProcessRegistrants at1015280A; successful UPackage.Bind does so at
   101530C5. This is not proof of the packed launcher's complete package order.
3. UClass.Register initializes defaults, invokes the distinct StaticConstructor,
   links, then imports config/localized data against that registration state.
4. Later UField.Serialize10125C70 replaces Next through the serialized reference
   at10125CB3..10125CB9. UStruct.Serialize10126FE0 replaces Children at
   10127023..10127029. UClass.Serialize101280D0 initializes/applies loaded
   defaults and config/localized data WITHOUT rerunning StaticConstructor.

## Portable integration boundary

The current eager-loaded serialized Engine chain is already past that original
empty-native-registration stage. Implement actual registration-before-loading,
or explicitly validate an equivalent final native field-binding/default state
without claiming to replay original Register. Preserve actual serialized field
identity, flags/category/Next; native-only GameEngine arrays and their actual
inner ownership must be prepared separately. A fabricated PortableClassDescriptor,
blank CDO or generic property.zero fallthrough does not satisfy this contract.

This audit establishes source evidence only. Actual metadata registration,
default storage/config class lookup, frozen instance copies and Engine/session/
Primitive lifetime remain unimplemented, followed by hash/Move/support/startup/
player/falling and the original live crate damage graph. No APK or headset
acceptance is implied.
