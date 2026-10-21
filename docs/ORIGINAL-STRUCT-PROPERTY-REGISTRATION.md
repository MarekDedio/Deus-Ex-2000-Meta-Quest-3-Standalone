# Original Core.StructProperty native registration (2026-10-21)

Read-only static prerequisite for actual first-Training ConstructorLink and
selected DestroyValue dispatch. No original commercial DLL/EXE was loaded or
executed, no map was loaded, and no owned source file was changed. Preferred
PE32 addresses and offsets identify the original binary profile; they are not
portable pointers or host C++ layout definitions.

Owned `D:/Steam/steamapps/common/Deus Ex/System/Core.dll` SHA256 was checked
before and after this audit:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

This adds one independently authenticated registration to the historical
eleven-class audit in ORIGINAL-NATIVE-METADATA-REGISTRATION.md. It does not
change the meaning of those eleven records or expand the closed eleven native
default profiles in ORIGINAL-ENGINE-DEFAULT-LINK.md. An actual UClass identity
for an existing serialized StructProperty is distinct from constructing a new
StructProperty instance or allocating its class-default block.

## Exact native registration

All numbers in this section are hexadecimal.

| Item | Original evidence |
| --- | --- |
| Actual native UClass | Core.StructProperty, PrivateStaticClass102A3EF0 |
| Initializer / UClass constructor call | 10169D50 /10169D9B ->10102D9C |
| Supplied native size | 64, pushed at10169D8D |
| Actual Super | Core.Property102A7AF0, pushed at10169D84 |
| Actual Within | Core.Field101F2B18, pushed at10169D7F |
| Supplied ClassFlags ->constructor baseline | 0 ->12 |
| Requested UObject flags | 04084004, pushed at10169D5C |
| Owning package | UTF16 Core101A7EF4, pushed at10169D66 |
| Config source | UTF16 System101A82C8, pushed at10169D61 |
| Exact unqualified name | UTF16 StructProperty101B1BA6, pushed at10169D6B |
| GUID | Four zero words, written at10169D7D/10169D8A/10169D95/10169D98 |
| Distinct instance InternalConstructor | 101030A8, pushed at10169D57 |
| Shared StaticConstructor | UObject.StaticConstructor10101041, pushed at10169D52 |

The initializer sets ECX=102A3EF0 at10169D90 before the native UClass
constructor call. Exported StaticClass10102F9F independently reaches
10115250..10115255, which returns that same102A3EF0 pointer. Direct static
bytes confirm the package, config and class-name UTF16 literals, including
their terminators; they are not inferred from a neighboring symbol label.

The native UClass constructor thunk10102D9C reaches10128630. It ORs12 into
the supplied ClassFlags at1012866F, writes actual Within+49C at101286D0,
StaticConstructor+4E4 at1012871A, InternalConstructor+4E0 at10128732 and config
name+4A0 at1012873E. The native superclass inheritance mask in Register is
4EC at10127905; Core.Property's13 contributes no additional bits. The
resulting12 baseline contains neither Config4 nor Localized20. This is not
permission to overwrite independently loaded serialized class flags.

## Default-copy provenance, not instance-constructor replay

The shared static-constructor thunk10101041 reaches1014F690, whose complete
body is RET. Core.Property supplies this same callback. Register compares
the actual class and actual superclass callbacks at101278A7..101278BE and
skips equal callbacks; there is no distinct StructProperty static-constructor
field initialization or Cpp-property insertion hidden in this registration.

InitClassDefaultObject10152300..1015236D does not call the registered instance
InternalConstructor. It prepares the UObject header, obtains the actual native
size and actual superclass, then calls InitProperties with that superclass.
InitProperties10152123..10152176 copies the actual superclass default payload
after the28-byte UObject header, and10152185..101521A7 zeroes the extended
destination tail.

Therefore, if this class-default block is needed, its established closed
payload is a copy of the prepared Core.Property5C defaults followed by zero
extension+5C..63. The Struct reference+5C is absent; the remaining+60 word is
also zero but is not assigned an unaudited semantic name here. Inherited
default ArrayDim is0, ElementSize/flags/offset are0, and native property-list
links are absent. Those are default-block values, not the later instance
constructor's ArrayDim1. The native class flags above gate ordinary config
and localized imports off. This is typed superclass-copy/extension evidence,
not a generic blank-CDO fallback for unrelated native classes.

The distinct instance route is separately verified:

- InternalConstructor101030A8 ->10115370..1011537D checks a nonnull destination
  and jumps to the default-constructor thunk10102577.
- Default constructor10102577 ->10115390..101153A1 calls UProperty's default
  constructor through1010283D at10115393, then installs distinct native
  implementation10176344 at10115398. It does not initialize+5C or+60.
- UProperty default body10164F80..10164F9A calls UField with null SuperField,
  writes instance ArrayDim+34=1 and installs its own intermediate implementation.
  It does not supply a nonnull referenced Struct or call AddCppProperty.

Authenticating this route is not permission to replay a constructor over an
existing loaded field, discard its serialized flags/category/Next, or infer
an existing field's referenced Struct from an empty native class default.

## Selected DestroyValue authority and bounded portable use

Direct static vtable bytes at10176344+94=101763D8 contain10101212. That exported
UStructProperty.DestroyValue thunk reaches10169CF0..10169D46, the nested
ConstructorLink body audited in ORIGINAL-CONDITIONAL-DESTROY.md. It selects
the referenced Struct's ConstructorLink, loops inner field first and ascending
outer ArrayDim element second, and calls each inner field's selected native
DestroyValue. This distinct selection must come from the actual bound
Core.StructProperty class, not just a descriptor type string, an Outer/path
coincidence or a Struct-shaped VM value.

The narrow runtime consumer can append this registration after the existing
eleven metadata records, preserving their indices, bind exact authenticated
source Class imports to the new actual indexed UClass, and use that identity
when preparing the real ConstructorLink graph and selecting nested teardown.
No StructProperty instance or new default block is required merely to bind
the already loaded NanoKeyData field. Source Children/Next, declaring Outer,
flags, dimension8 and referenced SNanoKeyInitStruct must remain independently
authenticated. A null unprepared native list head is not proof of an empty
ConstructorLink.

Selected Actor native implementation uses the actual UClass export header's
RF_Native04000000, NOT ClassFlags80 (which is RuntimeStatic). A table-only
first-Training dependency probe confirms PickupDistributor/CrateBreakable/
Containers export flags000F0004 (not native), Keypoint/Actor/DeusExDecoration/
ScriptedPawn/DeusExPlayer/PlayerPawn export flags040F0004 (native). Thus the
distributor's nearest native implementation is Keypoint and the crate's is
DeusExDecoration; the separately audited finite vtable profiles apply. The
DeusExPlayer native override is not a base-Actor bypass. Static table probe
`artifacts/conditional-destroy-audit-20261021/class_export_flags.cpp` reads only
Core/Engine/DeusEx/Extension.u and confirms all four source fingerprints
unchanged; no map or runtime initialization is involved.

The first-Training PickupDistributor needs destruction of all eight nested
Description strings plus the four inherited Actor strings; the destructible
crate needs its two DeusExDecoration strings plus those four Actor strings.
Nonconstructor fields, clocks, frozen defaults and immutable authored
provenance are outside that payload mutation. This registration audit alone
does not implement ConditionalDestroy, live linker detach, deferred deletion,
startup, surviving usable loot, gravity, checkpoints or headset acceptance.

## Reproduction: static tools only

The audit used an external objdump executable to parse original static bytes;
it did not resolve or call an original native function. For example:

```powershell
$auditObjdump = 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe'
$auditCore = 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
Get-FileHash -Algorithm SHA256 -LiteralPath $auditCore
& $auditObjdump -p $auditCore
& $auditObjdump -d -Mintel --start-address=0x10169d50 --stop-address=0x10169db0 $auditCore
& $auditObjdump -s --start-address=0x101b1ba6 --stop-address=0x101b1bc4 $auditCore
& $auditObjdump -d -Mintel --start-address=0x101030a8 --stop-address=0x101030ad $auditCore
& $auditObjdump -d -Mintel --start-address=0x10102577 --stop-address=0x1010257c $auditCore
& $auditObjdump -d -Mintel --start-address=0x10115370 --stop-address=0x101153a2 $auditCore
& $auditObjdump -s --start-address=0x101763d8 --stop-address=0x101763dc $auditCore
& $auditObjdump -d -Mintel --start-address=0x10101212 --stop-address=0x10101217 $auditCore
Get-FileHash -Algorithm SHA256 -LiteralPath $auditCore
```
