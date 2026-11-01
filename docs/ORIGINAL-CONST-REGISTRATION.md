# Original UConst class registration (2026-11-01)

The actual Training action diagnostic failed before its first command with
`Original Input field lacks its actual registered Class` after465.489s.
Unlike the old Byte/Float-specific reset iterators, FindButtonName/FindAxisName
first enumerate ALL inherited UProperties. Their IsA checks must authenticate
non-property fields too; treating an unresolved Class as a negative result
would hide missing native metadata.

The extended read-only class inspector independently lists actual
DeusExPlayer.NintendoDelay and MPMSG_* constants in its Children/Next chain.
They import Core.Const. No commercial executable or DLL is loaded/executed.

## Owned static evidence

Core.dll SHA256
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Preferred image VAs are evidence addresses, never portable object pointers.

UConst.StaticClass thunk10102DE2 reaches101121B0, which returns the genuine
PrivateStaticClass101F1218. Its initializer10129710 supplies the UTF16 UConst
literal101AA114 (class name begins101AA116), package Core101A7EF4,
config System101A82C8, flags04084004, zero GUID, native size40, supplied
ClassFlags0, Super Core.Field101F2B18 and Within Core.Struct101F2618.
The UClass ENative constructor call is1012975B ->10102D9C ->10128630;
the existing verified OR12 at1012866F yields constructor ClassFlags12.
Register's Super.ClassFlags&4EC adds no bits from Field13. This registers the
actual Class/index/relationships, not a Const object constructor or CDO.

## Implementation and verification scope

Core.Const is appended as metadata record16, retaining all previous indices
(especially ByteProperty record15). Existing closed-count fixtures now require
exactly16 records and17 owning GC objects (16 UClasses plus their domain),
not relaxed minimum counts. The independent literal oracle includes size40,
Field Super, Struct Within, flags12 and04084004. Every actual loaded Const
export is checked against its original source import and real Class index.

A rollback-only metadata probe uses the actual JCDentonMale Class, linked
Children/Next/Super traversal and shared input-property cache without spawning
a Pawn or fabricating Input/Viewport state. Expected source inventory is28
properties:13 Byte and15 Float. Its whole checkpoint/GC/lookup/input rollback
is checked before the longer real startup/action regression.

The fast owned metadata-cache/alias check now passes80 checks/10 refusals,
including the actual28/13/15 input inventory,3816 cache work and full rollback.
Both desktop and Android-native builds pass with the new registered Class.
The independent metadata run verifies131 original Const exports, including27
DeusExPlayer constants, then exposed an outdated expected16-object GC count
at map unload. The fixture now requires exactly17 and keeps the ordinary-name
collision at the new LAST record, Const. That failed fixture run is retained;
its corrected rerun now passes23470 checks/149 refusals. The authentic Training
action check also passes19 commands/10 actual player-field stores/shared28
properties and complete rollback45 unchanged sources in552.592s, reaching only
the expected unfinished Quest reconciliation boundary.
The failed465.489s startup run is retained as a failure, not relabeled passing.
This registration does not implement Quest
event reconciliation, Root handlers, ReadInput, completed startup or an APK.
