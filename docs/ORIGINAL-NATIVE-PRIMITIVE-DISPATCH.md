# Original native primitive virtual dispatch

Read-only audit2026-10-17, owned Engine.dll SHA256
0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.
No commercial code loaded/executed. Preferred PE32 VAs identify source evidence,
not portable object offsets or invented native instances.

## Selected object and virtual methods

Original AActor.GetPrimitive1034C9A0..1034C9C0 returns actual Brush first,
otherwise actual Mesh, otherwise XLevel->Engine->Cylinder. Engine fallback
lifetime is inactive when Brush or Mesh is nonnull. Do not require a fabricated
Engine/Cylinder to authorize an actual earlier Mesh branch, or replace Brush
with cylinder because its renderer lacks geometry.

The actual exported native vtable DWORDs are:

| Native type / vtable | LineCheck slot58 | CollisionBoundingBox slot64 |
| --- | --- | --- |
| UPrimitive / 1042655C | 10301B9F -> 103D8AF0 | 10302A9A -> 103D8460 |
| UMesh / 104290B8 | 1030259F -> 103A6D10 | 10302A9A -> 103D8460 |
| ULodMesh / 1042942C | 1030259F -> 103A6D10 | 10302A9A -> 103D8460 |
| UModel / 104265D8 | 10303E5E -> 103F3C20 | 10301794 -> 103F0E70 |

The Primitive/Mesh/LodMesh nonnull-owner bounds consume actual actor Location
+114, CollisionRadius+1D4 and CollisionHeight+1D8 with the verified+1 padding.
They are not render bounds, animation-frame extents or rotated mesh boxes.
Null-owner UPrimitive bounds copy its own native BoundingBox+28 (seven DWORDs).
UModel overrides bounds and, with nonnull owner, captures Owner.ToWorld slot88
and transforms its eight actual box corners. That requires actual Model native
box storage; actor collision dimensions are not a substitute.

## Complete UMesh.LineCheck wrapper

Normal body103A6D10..103A6E34 never dereferences its Mesh ECX receiver; it only
forwards that same receiver to the actual UPrimitive implementation. It takes
Hit[ebp+8], Owner+C, End+10/14/18, Start+1C/20/24, Extent+28/2C/30, Flags+34.

At103A6D2B it compares Extent.x to Float+0 at10426D54. C3=0 goes to103A6D6B,
the forwarding branch. With C3=1 it compares y at103A6D4B, then z at103A6D5B.
Any C3=0 forwards **all original extent values**, including earlier components.
For finite inputs this is the any-nonzero case.

Only when all three comparisons have C3=1 does103A6DD1 explicitly write literal
DWORD0 to all three extent argument slots (103A6DD4/6DE2/6DEB), copy the other
same arguments and jump at103A6E34 back to103A6DB9. Both branches call actual
UPrimitive.LineCheck10301B9F there, returning through103A6DCE. Addresses
103A6E36..103A6E79 are exception diagnostics, not another geometry branch.

Consequences: finite(-0,+0,-0) becomes(+0,+0,+0); a finite nonzero anywhere
preserves every original component bit. Unordered/NaN comparisons also set
C3=1, so all-zero-or-NaN extents canonicalize to zero; an earlier NaN followed
by a nonzero forwards the original NaN. A portable finite-supported policy
inside the cylinder arithmetic is a separate boundary; it must not be confused
with the wrapper predicate or reject discarded inactive NaNs before this branch.
There is no animated-triangle line query in this complete Mesh wrapper.

## Native registrations

All three below use Core.Object Within through105999F8, UClass constructor
RF04084004, supplied class flags0 (constructor baseline12), zero Guid, native
constructor enum0 and actual UObject.StaticConstructor import105999FC. Package
is Engine1047A148; config is System10479E58.

| Native type | Initializer / UClass | Size / superclass | Native internal/default constructor |
| --- | --- | --- | --- |
| UMesh | 103A6770..103A67C6 / 10589758 | 12C / UPrimitive1058D368 | 103020BD ->103492B0 ->10301A0A ->103A6130 |
| ULodMesh | 10354F40..10354FA6 / 105359D0 | 1B0 / UMesh10589758 | 10303855 ->1034A740 ->10302F09 ->1034A760 |
| UModel | 103AD260..103AD2C6 / 10589D58 | 704 / UPrimitive1058D368 | 103036ED ->1031E3C0 ->1030122B ->1031E3E0 |

Class-name UTF16 literals are Mesh10480C1E, LodMesh1047A37A, Model10480C86.
LodMesh default construction installs1042942C at1034A7FA. None has a serialized
Class export in the owned Engine.u. No native SkeletalMesh export was found in
the twenty owned System DLLs; compatibility NativeCastClass support for that
name is not original collision-vtable authority. A strict provider should
admit only the actually established Mesh/LodMesh/Model implementations and
refuse unknown nearest native subclasses.

Concrete owned crate evidence: DeusExDeco.u SHA256
1ACF34ABAC96AD371881C617BA69A6DF529B3E1C343277CED55329AC272BCB71,
export632 DeusExDeco.CrateBreakableMed, ObjClass-4 to exact Engine.LodMesh,
RF000F0004 and641-byte payload. Import-4 has ClassPackage Core, ClassName
Class, Outer-1 to Engine and ObjName LodMesh; import-11 identifies Engine.Mesh.
The asset RF is not native UClass evidence; the actual registrations/vtables
above supply that distinct authority. This selected Mesh branch needs no LOD
decode or Engine.Cylinder receiver state merely to perform a line query.

This audit creates no primitive objects, cache authority, live queries, hash
registration, movement or gravity. Next implementation must bind actual typed
Actor.Brush/Mesh handles and lazy virtual selection, retain owner/current fields
and per-map Model identity, share the outer VM work/rollback lifetime, preserve
empty-Model inactivity, and only then connect hash/collector/Move/support.
