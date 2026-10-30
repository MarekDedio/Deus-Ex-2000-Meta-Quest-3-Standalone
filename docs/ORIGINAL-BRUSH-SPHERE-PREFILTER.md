# Original moving-brush sphere prefilter and live BSP consumers

Static audit, 2026-10-30, for the first `00_Training` tracker constructor.
Only this document was added; no compiler inputs, builds, tests, original
modules, or live runtime state were changed. The read-only owned authority is
`D:/Steam/steamapps/common/Deus Ex/System/Engine.dll`, SHA256
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
All addresses below are original PE32 virtual addresses and byte offsets,
not portable layouts or authorization to execute the original library.

This completes the selected prefilter source contract from
`ORIGINAL-MOVING-BRUSH-TRACKER.md`. `ORIGINAL-BRUSH-TRANSFORM.md` supplies the
FSphere constructor and polygon coordinate inputs. The enclosing genuine
tracker/Model/UPolys ownership and journal are still separate integration.

## Exact entrypoints and parameters

Engine export1350 is `UModel::PrecomputeSphereFilter(FPlane const&)`, thunk
`10301BEF -> 103F6460..103F651A`. The original tracker supplies the FSphere
packet as this FPlane-shaped value: Float center XYZ at0/4/8, radius atC.
The recursive cdecl helper is thunk`103012A3 -> 103F63A0..103F6425` with
`(actualModel, nodeIndex, sameSpherePointer)`.

The root reads current Model.Nodes.Num at native+5C. If it is zero, it
returns without reading sphere fields, nodes, or flags. Otherwise it starts
at node index0, using Model.Nodes.Data+58 and native node stride64. The
helper starts at its supplied node index; only callers' child -1 checks
prevent a -1 entry. Neither routine scans all allocated Nodes.Num slots.

## Per-visited-node mutation and arithmetic order

On **each actual visit**, both root and helper:

1. Load node Flags byte+37.
2. Clear only masks`0x40|0x80`, immediately writing `Flags & 0x3F` back.
3. Compute retained plane distance with no Float or Double result spill.
4. Classify against the original stored Float radius, set at most one
   prefilter flag and choose child traversal as described below.

The two bodies have different literal product order:

| Body | Retained original distance |
| --- | --- |
| root `103F64A1..103F64BC` | `((node.nz*sphere.z + node.ny*sphere.y) + sphere.x*node.nx) - node.w` |
| helper `103F63BA..103F63D5` | `((node.ny*sphere.y + node.nz*sphere.z) + node.nx*sphere.x) - node.w` |

Each arithmetic instruction rounds under the original PC64 profile. Products,
the first two-term sum, completed dot and W subtraction remain retained;
there is no Float/Double distance, epsilon, normalized plane, radius scaling,
absolute-value test, or precomputed `distance+radius` replacement. The root
performs its own arithmetic for every node reached by its tail loop, not
only the first root node. Recursive helper tail visits use the helper order.

First load radius, FCHS it **without a store**, duplicate retained distance,
then FCOMPP distance against that retained negative radius. If the comparison
is less (`AH & 1`), select the back-only branch. Otherwise compare retained
distance directly against a freshly loaded positive radius with FCOMP;
strict greater (`AH & 0x41` equal zero) selects front-only. For finite inputs:

| Condition, in source priority order | Flag after low-six-bit preservation | Traversal |
| --- | --- | --- |
| `distance < -radius` | OR`0x80` | tail Back, node+20 |
| else `distance > radius` | OR`0x40` | tail Front, node+24 |
| else | neither bit | recurse Back first if not-1, then tail Front |

Thus equality at either sphere plane is overlapping, not one-sided. Either
distance zero sign compares equal to radius zero; source does not inspect a
Float sign bit here. A negative finite radius is not independently rejected
by the native routine; the first test's priority remains significant. Actual
FSphere input is nonnegative, but a detached prefilter must not silently
normalize/clamp an arbitrary radius without declaring that narrower policy.

For unordered native operands the first comparison's C0 is set, so it takes
the back-only branch. A bounded portable finite-active-input contract should
explicitly refuse that numeric case rather than claim this unchecked native
behavior is safe. Empty root Nodes still must not validate inactive sphere
fields. Mutating prefilter code belongs inside the caller journal: a later
refusal cannot leave the earlier flag clear behind as a successful effect.

## Exact traversal order and write scope

The root's overlap path `103F6507..103F651A` reads Back+20, recursively calls
the helper only when Back!=-1, then reads/tail-visits Front+24. The helper's
overlap path `103F6407..103F6420` does the same. A one-sided branch only reads
the chosen child and tails until -1. There is no fallback to the opposite
child when the chosen child is -1.

Node Flags are not consulted as CSG/vertex-count gates by this prefilter.
Existing prefilter bits do not suppress a visit; they are cleared/recomputed.
All low six bits survive. No Points, Vectors, Surfs, Verts, node plane,
child links, zones, leaves or tracker ownership words are written here.
There is no coplanar PlaneChild+28 or per-mover root-chain+30 traversal.
Unvisited siblings and unreachable/free pool slots keep their previous flag
bytes. There is no all-model reset at the start or completion of this call.

The source is recursive/tail-recursive without a safety visited set. A shared
child may be revisited and rewritten; do not deduplicate such visits and
claim identical native work/order. Cyclic/malformed links and exceeded
depth/work require an explicit bounded adapter refusal, not a synthetic
always-clear result or a reset allowance for each recursive invocation.

## FilterPoly consumes exactly this prepared live node state

Tracker Update computes the transformed mover sphere, invokes this prefilter,
then calls FilterPoly `103030B2 -> 1037C0C0` for each transformed polygon.
At `1037C1DE..1037C1E7`, FilterPoly reads the current node's Flags+37:
mask40 directly selects front; mask80 directly selects back; otherwise it
performs the actual polygon-plane split. Other CSG/outside tests later use
the same node values. This is a selected BSP mutation prerequisite, not a
renderer frustum optimization or metadata-only diagnostic.

## Actual consumer link evidence: same index space, active paths

The following original consumers begin at node0 after their native empty
Nodes test and follow Back/Front. They do **not** iterate all Nodes.Num free
slots or use tracker owners as substitute geometry:

| Consumer | Original bounded evidence | Selected node links |
| --- | --- | --- |
| UModel.FastLineCheck | export1026 thunk`10301A37 -> 103F3280`; root node0 call`103F32DA..103F32F0`; recursive body`103F33A0..103F34FE` | split reads`[node+side*4+20]` at`103F34A6`; same-side/tail reads it at`103F34D1`; side0=Back20, side1=Front24 |
| UModel.LineCheck zero extent | export1271 thunk`10303E5E -> 103F3C20`; actual recursion thunk`1030207C -> 103F3560..103F3AB9` | same-side Front24 at`103F3750`, Back20 at`103F37AC`; split child20+side*4 at`103F3924` and opposite24-side*4 at`103F39B6` |
| UModel.LineCheck extent | wrapper`103F3F6E..103F4184`; recursion`10302838 -> 103F42F0..103F568C` | first eligible child20+side*4 at`103F4545`; opposite24-side*4 at`103F4575`; later solid leaf reads last-parent CollisionBound |
| UModel.PointRegion | export1318 thunk`1030391D -> 103F5DB0..103F5F38`; node0 initialization`103F5DD1`; per-plane traversal`103F5E15..103F5ED0` | child20+side*4 at`103F5E99`, `103F5EBA`, `103F5ECA`; terminal parent supplies leaf/zone words |

FastLine specifically saves current Model.Nodes.Data as its traversal pointer
at`103F329B..103F329F`; it is not a separate source-file BSP. It tests only
Flags bit0 for outside transitions and uses its own Float-stored distance
sign-bit classification. Point/extent LineCheck have different CSG/flag and
distance rules; these are already recorded in `ORIGINAL-FAST-TRACE.md`,
`ORIGINAL-MODEL-POINT-OWNER.md` and `ORIGINAL-MODEL-EXTENT-TRACE.md`.
PointRegion has its retained distance and terminal leaf/zone behavior in
`ORIGINAL-MODEL-POINT-REGION.md`. This audit does not merge those algorithms.

For these traversal edges, neither PlaneChild+28 nor tracker root-chain+30
is a fallback child. Their distinct use is proven in tracker AddPolyToBsp,
FilterPoly and FlushActor. Other services such as rendering/decal coplanar
selection need their own link access and bounds; this narrow collision/
region evidence does not authorize omitting them from the actual Model.

The original AddPolyToBsp writes a new active node index directly into a
selected parent's Back/Front/Plane field and the mover's separate root chain.
Once an active Back/Front link changes, the consumers above observe it from
the same world Nodes array. Reloading authored source geometry, leaving a
stale immutable snapshot selected, or compacting/remapping dynamic indices
does not preserve that contract.

## Consequence for bounded mutable queries: no validation shortcut

Existing raw serialized-model kernels validate every node, including pruned
components; immutable hull proofs are tied to actual const allocations.
Those contracts and malformed-source tests must remain unchanged.

The native tracker expands actual Nodes.Num and zeroes unused dynamic slots.
Such a slot's zero Back/Front/CollisionBound fields are not an authored active
subtree. This audit proves that the native consumers only reach slots through
their actual traversal links, not through an all-Num loop. It does **not**
prove that arbitrary caller-supplied mutable data may bypass validation.

A separate live-Model capability/view must authenticate source baseline,
actual tracker ownership, generation/revision and all observable mutated
links/index bounds under the same transaction. Preserve the native index
space and actual pool bytes. A traversal reaching an unowned/free dynamic
slot is an invariant/refusal condition, not permission to cull the link or
replace it with -1/outside. Source baseline malformed components and newly
active dynamic components still require their appropriate admitted checks;
do not turn a public boolean or const-qualified mutable pointer into a proof.

Design and independent tests for that capability are not implemented here.
The prefilter itself also needs work/byte/journal ownership and root/recursive
numeric tests. The genuine constructor, insertion, query integration, later
startup services and first-Training crate gameplay remain explicit boundaries.

## Static reproduction

Use only the external objdump against read-only source, never DLL loading:

```powershell
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x103f6460 --stop-address=0x103f651c 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x103f63a0 --stop-address=0x103f6426 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x103f33a0 --stop-address=0x103f34ff 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x103f3560 --stop-address=0x103f3aba 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x103f42f0 --stop-address=0x103f457e 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x103f5db0 --stop-address=0x103f5f39 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
```
