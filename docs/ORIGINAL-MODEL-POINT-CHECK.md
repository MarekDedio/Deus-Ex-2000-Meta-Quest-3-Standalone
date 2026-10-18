# Original UModel.PointCheck dependency

Read-only static GOTY audit, 2026-10-18, scoped to the collision dependency
needed by first `00_Training` mover/crate encroachment. This is **PointCheck**
(point/box occupancy), not the already implemented **LineCheck** point branch.
No original commercial EXE/DLL was loaded or executed. No Model occupancy
kernel, complete encroachment, Move266, gravity or playable crate acceptance is
established by this document.

Original SHA256 values, checked before and after the inspection:

- Engine.dll: `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`
- Core.dll: `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`

Addresses below are preferred image virtual addresses mapped through the PE
section table, never portable object offsets. Export names establish actual
thunk identities; objdump's nearest unrelated symbol headings do not name the
internal helper bodies.

## Selection and packet contract

The exported UModel.PointCheck thunk `10301F5F` jumps to normal body
`103F1570..103F17F2`. Signature: caller FCheckResult reference, optional Actor
Owner, query Location Float3, Extent Float3, unsigned flags. Virtual slot+54:

| Actual class | Exported vtable | Slot+54 address | Function word |
| --- | --- | --- | --- |
| UPrimitive | `1042655C` | `104265B0` | `10301848` |
| UModel | `104265D8` | `1042662C` | `10301F5F` |
| UMesh | `104290B8` | `1042910C` | `10301848` |
| ULodMesh | `1042942C` | `10429480` | `10301848` |

Thus Mesh/LodMesh inherit raw UPrimitive.PointCheck in this source; do not
invent a Mesh-specific zero/NaN-extent wrapper from its different LineCheck.
UPrimitive.PointCheck thunk `10301848` targets `103D87E0..103D8A46` and remains
a distinct cylinder operation. Model selection still requires real native
GetPrimitive authority, including actual Brush precedence and Owner.ToWorld.

At `103F1598..103F15D4`, Model.PointCheck **always** writes:

```
Hit.Primitive = this Model; Hit.Actor = Owner; Hit.Time = Float+0;
Hit.Normal = Float3(+0); Hit.Location = supplied Location;
```

It does not initially write Next or Item. These stores precede the node-count
test at `103F15E0`. An empty Model then returns its RootOutside value without
reading Extent, Owner coordinates or flags. Unlike Model.LineCheck's empty
case, its caller packet is not unchanged. A portable transaction can stage
these stores against throwing validation, but must publish the same defined
packet stores on a successful empty/clear return.

## Zero-or-unordered Extent: containment, not a line trace

For nonempty nodes, the successive x/y/z comparisons `103F15EB..103F1619`
test x87 equality-status bit40 against Float0 (`10426D54`). Unordered also
sets that bit. All three zero-or-unordered components enter `103F16C5`;
the first nonzero ordered component selects the nonzero-box path instead.

The zero branch captures Owner.ToWorld once through Actor virtual+88, or
copies identity coordinates for null Owner (`103F16CB..103F16F1`). Starting
at node0 and RootOutside, it iterates one child per node:

- Transform/store the normal components using captured axes, in Z/Y/X
  computation order, with each inlined sum `(Y+X)+Z`
  (`103F16F8..103F1752`). Imported TransformVectorBy maps
  coords.Origin to a Float3. Compute original W minus its dot with transformed
  normal; the dot has an FST Float at `103F1767` **without popping**, so its
  retained value remains the subtraction authority.
- Compute query-plane distance with `(Z+Y)+X` products, subtract the retained
  transformed W and compare against Double0 (`10429838`) at `103F1785`.
  There is **no intermediate Float distance store**. Equality selects back;
  strictly positive distance selects front.
- CSG uses vertexCount>0 and `(nodeFlags & 0x21)==0`. Caller flags are ignored
  in this branch. Front updates outside by OR CSG, back by AND NOT CSG.
- The original native node child offsets are back+20/front+24. At a terminal
  child−1 it writes `Item = 2 * terminalNodeIndex + side`, with back side0 and
  front side1 (`103F17E5..103F17EB`), then returns outside.

Location/Normal/Time remain the entry stores; no closest surface, line
backoff, owner-normal retransformation or hull sweep occurs. Existing
ModelPointLineDetail::Trace/Distance cannot simply supply this operation:
their traversal and intermediate Float-store contract differ.

## Nonzero Extent: separate box occupancy machinery

At `103F161F..103F168D`, caller flags are converted through an unsigned64
integer temporary and `_ftol` thunk `10413652` (IAT`1059A34C`), then passed to
constructor `103F18E0..103F197D`. That constructor captures Owner.ToWorld once
or identity, records original Extent/flags, clears helper bookkeeping, and
constructs64 local FPlanes. The wrapper records query Location and sets
best-distance Float100000 (`47C35000`) before calling recursive helper
`103F19B0..103F2BBD` with node0, parent0 and RootOutside.

The complete recursive normal body was inspected, including its return and
all three bevel-axis branches. It is not the nonzero LineCheck kernel:

1. For each visited BSP node, use the raw plane for null Owner, otherwise
   transform/store its normal and W using captured coordinates
   (`103F19E4..103F1A63`). Store Float3(Extent * Float1.1), where Float1.1 is
   `3F8CCCCD` at `10431C34`. Its absolute projected radius is Float-stored;
   node distance is separately Float-stored (`103F1A67..103F1B2C`).
2. If distance>−radius, recurse front first, passing outside OR CSG. Then
   continue back with outside AND NOT CSG if distance<=radius. CSG uses
   vertexCount and low-byte `(flags | 0x21)`. Any recursive false is retained;
   a later clear branch does not erase it (`103F1B30..103F1BDE`).
3. At a solid leaf, resolve its parent node collisionBound. A missing bound−1
   is clear. Collect up to64 hull planes, transform by the original imported
   TransformPlaneByOrtho for Owner, then apply the encoded40000000 flip.
   Record each normal's six-component sign mask. The loop ends at literal−1
   or the64 cap. Copy six following bound words regardless of Owner
   (`103F1BEF..103F1DBF`). A safe port must explicitly reject malformed
   over-cap/unterminated data rather than silently treat it as a valid hull.
4. Test primary hull planes in source order (`103F1DC5..103F1F1D`) using
   **original**, not1.1-inflated, Extent. Projection-radius helper
   `103F6590..103F65DB` computes retained absolute products in Z/Y/X addition
   order; callers Float-store radius. Imported PlaneDot computes `(Z+Y)+X-W`.
   Its first positive test uses the retained return after an FST Float;
   subsequent distance/best/radius comparisons use stored Float values.
5. A positive distance smaller than bestDistance can become the candidate
   pushout only when distance<radius. Save bestDistance, set Location to the
   query point plus the scaled normal, Normal to that plane, Actor/Primitive
   to Owner/Model, Time0 and Item to the unflipped source plane-node index.
   Scaling first stores `Float3(normal * Float1.02)` (`3F828F5C`), then uses
   Float(radius−distance) and imported Float-vector multiplication/addition.
   A separating distance>=radius returns clear even if earlier candidate
   packet stores occurred. Do not publish fields only when final clear=false.
6. For **null Owner only**, test six additional axis bounds in order −Z,+Z,
   −X,+X,−Y,+Y (`103F1F1F..103F2715`). Their plane W values, Float-stored
   after Double0.1 (`3FB999999999999A` at `1042BCB0`) arithmetic, are
   `0.1-minZ`, `maxZ+0.1`, `0.1-minX`, `maxX-0.1`, `0.1-minY`,
   `maxY-0.1` respectively. Preserve this asymmetric high-X/Y subtraction;
   do not replace it with a uniformly expanded box. Their accepted candidate
   Item is−1. Nonnull Owner skips numerical use of these bounds, not the
   preceding six-word copy.
7. Test hull-plane pairs in original outer/inner order and X/Y/Z axes
   (`103F2715..103F2BA3`). Union sign masks gate each axis. Projected normals
   are cross-products with that axis, UnsafeNormal-normalized where shown;
   an axis branch proceeds only when their retained dot is strictly greater
   than Double0.001 (`3F50624DD2F1A9FC` at `1042AC20`). Obtain a plane-plane
   intersection, orient its normalized line against the first plane, create
   FPlane(intersectionPoint,lineNormal), and pass Item−1 to the bevel test.
8. If all required primary/bounds/bevel planes overlap, return false for the
   solid leaf (`103F2BA9`). A clear/separating path returns the retained
   recursive accumulator. BestDistance and caller packet are shared across
   all recursion; do not reset or choose a new nearest hit per leaf.

Item can therefore remain caller-seeded if no candidate update occurs. Time,
normal and location nevertheless have defined entry stores. The unsigned
flags control nonzero traversal CSG; they are not guessed PolyFlags filters.

## Bevel helpers and remaining numerical verification

`103F3050..103F3205` tests a constructed bevel plane against original Extent
and query Location. It retains its radius and distance through comparisons,
unlike the primary plane's stored Float comparisons. Positive distance below
bestDistance updates the caller only if below radius. It writes Item from
its supplied integer (−1 here), Time0, Owner/Model and pushout Normal/Location;
it returns distance<radius. Its inlined Float1.02 vector schedule has different
retained/store boundaries from the primary helper call chain. Reusing one
generic rounded dot or pushout helper needs independent numerical proof.

Plane-intersection helper `103F6600..103F67E6` stores the normals' cross-product
Float3, computes its retained squared length and compares strictly against
Double1e−6 (`3EB0C6F7A0B5ED8D` at `10433610`). Below threshold it zeroes both
output vectors and returns false. Otherwise it computes the point with
explicit cross/vector Float spills, Float-stores the reciprocal length-square,
then calls Normalize on the line direction. The caller does **not** branch on
that return before later normal/plane work; a finite portable policy must
refuse undefined/nonfinite active results instead of inventing separation.

Named Core imports were resolved and their complete small normal bodies read:
FVector cross `101176C0..101176F8`, Normalize `10117CF0..10117D61`,
UnsafeNormal `10117DD0..10117E20`, FPlane(point,normal)
`10118360..1011839B`, and PlaneDot `10118520..1011853D`. Normalize has its
own small-length branch; UnsafeNormal does not. Existing audited owner
coordinate helpers cover TransformVectorBy/TransformPlaneByOrtho, but no
independent oracle yet proves the **combined occupancy** numeric schedule,
bevel degeneracies, retained comparisons or packet behavior. This remains
required implementation/verification, not a passing kernel claim.

## Proposed integration seam and retained evidence

Use a distinct caller-seeded `CheckOriginalModelPointOverlap` interface with
Model identity, Owner identity, query Location, Extent, flags, lazy ToWorld
capture, fixed validation/work limits and returned statistics. Stage packets
until supported-domain success; preserve Next and untouched Item. Model
occupancy needs its complete zero/box/owner/null behavior, not a zero-only
native shortcut or LineCheck substitution. Runtime capture must use the
actual temporarily posed source in ActorEncroachmentCheck, not candidate
coordinates or a render mesh. Shared VM/hash-root rollback and failure work
accounting remain mandatory. No other map integration is needed for this work.

Ignored evidence directory: `artifacts/training-model-pointcheck-20261018/`.
Bounded files contain the two thunks, complete Model wrapper, box constructor,
complete recursive body, bevel test, projection-radius helper, plane
intersection and scaled-normal helper; metadata records vtable mapped words,
constants, named import targets and unchanged source hashes. Core helper
body files are separately bounded. No original executable code is run.
