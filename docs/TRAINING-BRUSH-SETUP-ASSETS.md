# First Training SetupActor source/binding notes (2026-10-31)

Read-only source/data discovery for the positive moving-brush constructor,
not a new loader, runtime tracker, test, or APK. No commercial module was
loaded/executed and no compiler inputs/builds/tests were changed. The new
document is the only edit. Original data was read with bounded table/body
parsing in PowerShell; this script was not saved into the project.

## Actual native and portable identity boundaries

Original Engine.dll UPolys.PrivateStaticClass export1428 is RVA`287948`,
VA`10587948`. UPolys is derived from Core.Object, **not** UPrimitive.
The runtime's existing NativeCastClass fallback includes Engine.Polys ->
Core.Object, but that is class identity only, not a constructed UPolys payload.
NativePrimitiveAsset deliberately admits Model/Mesh/LodMesh only and must
not be reused as if Polys were a primitive.

Native UModel.Polys is the actual object pointer at Model+54. Original
UModel.Serialize thunk`103026B2 -> 103ACAA0` serializes that object-reference
slot at`103ACBB0..103ACBB8` after the model geometry/zone records. Current
`portable_model_geometry.cpp` retains its signed source package reference
as `polysReference`; it is **not a reflected property** and decoding it does
not itself establish a RuntimeObject/GC edge. UPolys Data/Num/Max is at
28/2C/30; original UPolys.Serialize thunk`1030367F -> 1030A2E0` processes
its actual native polygon array.

The full detached codec authenticates actual Engine.Polys import/class/
package provenance, qualified export identity and complete body consumption.
A runtime binder must resolve the matching current source-local export to
the actual indexed UPolys RuntimeObject, retain Model -> Polys ownership,
and attach the mutable payload/before-images to that identity. The same
authored reference ordinal in another package is a different object.

## Exact declaring property schemas

Individual existing read-only inspector calls confirmed:

| Qualified original property | Descriptor | Dim | Flags | Struct reference |
| --- | --- | --- | --- | --- |
| Engine.Mover.OldPos | Core.StructProperty | 1 | `0` | Core.Object.Vector |
| Engine.Mover.OldRot | Core.StructProperty | 1 | `0` | Core.Object.Rotator |
| Engine.Actor.bAssimilated | Core.BoolProperty | 1 | `2002` | none |
| Engine.Actor.bSpecialLit | Core.BoolProperty | 1 | `21` | none |

Native setup cache offsets/packed bits and write order remain the contract
in `ORIGINAL-MOVING-BRUSH-TRACKER.md`: OldPos+4AC, OldRot+4D0,
bAssimilated+28/mask100, bSpecialLit+1EC/mask1. Use authenticated qualified
fields/ordered canonical Vector/Rotator schema and ordinary shared VM undo
where appropriate, not leaf-name writes or raw portable-memory offsets.

## Actual25-mover first-Training material cohort

The map table/property/native-Model/FPoly read found25 actual
DeusEx.DeusExMover exports with bStatic not set. Their selected Brush Models
each refer to local Engine.Polys exports; every one has6 polygons, for150
polygons total, agreeing with the existing independent port-owned source
fixture's cohort size. Every decoded native polygon body ended exactly at
its export boundary.

Representative exact local source references (one-based package refs):

| Actor in00_Training | Model name/ref | Polys name/ref |
| --- | --- | --- |
| DeusExMover16 | Model1032 /1894 | Polys208 /1899 |
| DeusExMover17 | Model1033 /1896 | Polys210 /1901 |
| DeusExMover30 | Model1059 /1349 | Polys1020 /1344 |

All150 polygon Texture references are **nonnull**, and only these two actual
map imports occur:

| Map reference | Fully qualified imported texture | Polygon count |
| --- | --- | --- |
| `-52` | UNATCO.Metal.un_BlastDoor_A | 50 |
| `-4` | CoreTexMetal.Metal.Heli_LiftMetl_A | 100 |

Each of the25 mover sources uses both references. Actual map import chains:

- `-52`: ClassPackage Engine, ClassName Texture, Name un_BlastDoor_A,
  Outer`-16`; `-16`: Core.Package Metal, Outer`-5`; `-5`: Core.Package
  UNATCO, Outer0.
- `-4`: ClassPackage Engine, ClassName Texture, Name Heli_LiftMetl_A,
  Outer`-2`; `-2`: Core.Package Metal, Outer`-1`; `-1`: Core.Package
  CoreTexMetal, Outer0.

The actual owned texture packages and target exports were also read:

| Source under owned installation | File bytes | Export index / ref | Actual source class | Outer ref | Payload offset / bytes |
| --- | --- | --- | --- | --- | --- |
| Textures/UNATCO.utx | 4,406,928 | 213 /214 | Engine.Texture | 115 | 3,848,221 /43,899 |
| Textures/CoreTexMetal.utx | 5,586,282 | 58 /59 | Engine.Texture | 108 | 637,120 /22,036 |

Neither package is among the38 System `.u` packages listed by the original
startup fixture at `desktop/portable_actor_script_test.cpp`. Thus the
script-prefix baseline is not evidence that these actual texture objects
already exist in the VM index. A real SetupActor binder needs a genuine
source-asset admission/ownership path for these selected objects, their actual
source packages and outer chain. Mip/image decoding is not required merely
to give native Surf.Texture a real identity and GC edge; conversely a renderer
cache/image handle is not a substitute for that native identity.

Original SetupActor writes the actual polygon.Texture pointer into the new
world Surf and actual current mover into Surf.Actor. Keep source reference
ordinals as provenance, but do not place them in pointer-shaped runtime slots,
hardcode Texture=None, borrow another package's same ordinal, or manufacture
the texture object from a string without validating its source export/class.
The typed runtime GC ownership and journal must retain/release those edges
at the original mutation/rollback/teardown boundaries. No source files should
be rewritten as a persistence substitute.

Final read-only Training SHA256 remained
`BB66D95AAFC6AAE4179F8444E98661F3D5D27DAB06D13D7DDE7F4B5F4E24068D`.
The first import-summary attempt had a PowerShell return-token typo, failed
read-only, and was stopped; corrected table parsing produced the entries
above. No result from that failed attempt is used as evidence.

This discovery is first-Training scope only. It supplies concrete material
targets for genuine asset admission, not completed SetupActor, pools,
Update/BSP insertion, queries, later startup, crate gameplay or a new build.
