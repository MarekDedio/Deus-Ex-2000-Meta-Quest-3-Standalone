# Actual Core.ByteProperty registration (2026-11-01)

This closes a concrete missing native-class identity exposed by the first-
Training Input.ResetInput prefix. It does NOT complete Input.Init, Quest input
reconciliation, startup, frames, crates or a new APK. The original modules were
read as PE/file data only; no commercial DLL/EXE was loaded or executed.

## Independent owned source evidence

Owned Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
The external MinGW objdump reads this file, not an original in-process module.

Core's exported UByteProperty.StaticClass thunk1010287E jumps to10112A40, whose
MOV EAX,102A75F0 / RET supplies the actual PrivateStaticClass. The exported
PrivateStaticClass and autoclass records also identify102A75F0 and102A75E4.
The latter is assigned the former at10165C50. Native initializer10165BD0 calls
the UClass ENative constructor thunk10102D9C at10165C1B with:

| Registration input | Actual source |
| --- | --- |
| Class identity | 102A75F0 |
| Native instance size | 60, pushed10165C0D |
| Supplied ClassFlags | 0, pushed10165C09 |
| Super | Core.Property102A7AF0, pushed10165C04 |
| Within | Core.Field101F2B18, pushed10165BFF |
| UTF16 unqualified name | ByteProperty101B122A, pushed10165BEB |
| Package | Core101A7EF4, pushed10165BE6 |
| Config | System101A82C8, pushed10165BE1 |
| UObject registration flags | 04084004, pushed10165BDC |
| Internal constructor | 10102653, pushed10165BD7 |
| Static constructor | 10101041, pushed10165BD2 |

The16-byte GUID area is explicitly zeroed10165BF3..10165C18. The already
authenticated UClass ENative constructor10128630 ORs12 at1012866F, so this
record's constructor baseline is12, not its supplied0. Original source class
flags are not overwritten by that constructor baseline.

InternalConstructor10102653 jumps10112B60, which forwards a nonnull target to
the actual UByteProperty default constructor thunk10102CDE. The latter's
body10112B80 stores the actual UByteProperty vtable10175B04 at10112B88; it does
NOT itself zero the UEnum member. The concrete NetSerializeItem body reads that
member at+5C at10165850, independently agreeing with the initializer's native
size60 rather than base UProperty size5C. This
audit does not grant permission to instantiate a default or replay a C++
property constructor on an existing serialized field.

## Reached defect and exact correction

The initial real reset-prefix diagnostic reached its named still-missing Quest
UpdateInput continuation, but failed the requirement that genuine Byte inputs
were reset. It executed15 reflected stores rather than the required28. The
new host iterator queried actual field.Class ancestry; Core.FloatProperty was
already materialized by the Engine collision/default domain, whereas
Core.ByteProperty was absent from the native registration closure. Treating a
missing Class pointer as an ordinary nonmatch silently skipped the Byte pass.
The correct fix is actual indexed registration and fail-closed missing-Class
handling, NOT removing the Byte-input oracle or substituting a property name.

The closed native metadata domain now appends ByteProperty as record15. Records
1..14, including Enum and Function, keep their prior indices. The generic
authenticated Class-import binding attaches genuine serialized ByteProperty
fields to this actual UClass. Its class owns the real Property/Field/Core.Class
relationships and index leases; no invented class descriptor or CDO is added.

An independent read-only Engine.u table scan finds these13 original Pawn input
Byte declarations, all dimension1 and flags4:

`bAltFire, bFire, bDuck, bRun, bLook, bStrafe, bFreeLook, bSnapLevel,
bExtra2, bExtra1, bExtra0, bZoom, bExtra3`.

It finds these15 original PlayerPawn input Float declarations, also dimension1
and flags4:

`aLookUp, aStrafe, aTurn, aForward, aMouseY, aMouseX, aUp, aBaseY, aBaseX,
aExtra2, aExtra3, aExtra4, aBaseZ, aExtra1, aExtra0`.

The metadata integration oracle now independently checks all loaded original
ByteProperty exports against the actual registered index and verifies those13
Pawn declarations. Native metadata header/count/GC/lifetime/cast tests now expect
15 classes plus one owning domain; the atomic late-name collision fixture targets
the new final ByteProperty name. Other dependent fixtures retain exact15-count
expectations, not a weakened minimum count. Runtime prefix acceptance and all
build/test outcomes must still be demonstrated separately and recorded in STATUS.
