# Original cylinder PointCheck prerequisite

Read-only original GOTY audit,2026-10-18. Content integration remains first
`Maps/00_Training.dx` only. No commercial EXE/DLL is loaded, executed or changed.
This is a collision primitive prerequisite, not Move/encroachment, successful
world startup, live crate damage, surviving loot, falling or an APK.

Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Ignored evidence `artifacts/training-actor-point-original-20261018.log` contains
the complete normal primitive body, exported thunk, selected vtable words,
constants and the complete imported Core.SafeNormal body. Named native thunk
`10301848` targets `103D87E0..103D8A44`; vtable slot+54 of UPrimitive, UMesh and
ULodMesh all selects that same thunk. There is no Mesh-specific wrapper here.

## Native contract

Inputs are caller FCheckResult, optional actual Owner, query Location, Extent,
and Flags. True means clear; false means overlap. Null Owner returns clear
without reading query/extent or writing any packet field. Flags and Extent.y
are inactive throughout this body. Unlike Mesh.LineCheck, there is no
zero-or-unordered extent canonicalization before this raw operation.

The original strict overlap predicates are:

```
(Owner.Z-Location.Z)^2 < (Extent.Z+Owner.CollisionHeight)^2
(Owner.Y-Location.Y)^2 + (Owner.X-Location.X)^2
    < (Extent.X+Owner.CollisionRadius)^2
```

Differences, sums and products retain the audited PC64 arithmetic; the summed
dimensions do not Float-spill before squaring. Exact radial/cap contact is clear
and leaves the complete seeded packet unchanged. The height test comes first,
so a clear cap branch does not read inactive X/Y/radius values.

An overlap writes Actor=Owner and Normal=SafeNormal of the Float-spilled
three-dimensional query-minus-owner vector. That full normal stays in the
packet; the later radial normal is temporary. The strict Normal.Z branches
below-Double-.5 and above-Double+.5 both write
`(Location.X,Location.Y,Owner.Z-Extent.Z)`. The positive branch really subtracts
Extent.Z too; the port must not silently repair this into symmetric geometry.

Otherwise form SafeNormal(Normal.X,Normal.Y,0). For output Location, X's
Extent.X product stays retained until Owner.X subtraction; Y and Z products
Float-spill before subtraction. Store Float(Owner.Z-zProduct), then perform
a separate Float addition of the original query Location.Z. Thus a centered
zero-extent overlap can produce Owner.Z+Location.Z; this is source behavior,
not a renderer position to replace with a plausible contact point.

Only Actor, Normal and Location are written. Next, Primitive, Time and Item
remain caller-owned, even on a hit. They are not input bounds, validated query
data or permission to claim a particular surface. The portable kernel stages
all packet writes and refuses active nonfinite values, negative active sizes
and overflowing required Float stores atomically. Those finite-domain refusals
are explicit portability policy, not invented original clear/blocked returns.

## Runtime scope

`quest_actor_point_check.h` implements the detached cylinder primitive.
`NativeActorPrimitivePointCheck` selects the actual authenticated primitive and
qualified current Owner Location/CollisionRadius/CollisionHeight. Constructed
Engine.Cylinder, actual Mesh and actual LodMesh use the same recovered point
body; no renderer dimensions or controller position is its authority.

The diagnostic in `portable_actor_point_check.h` owns an explicitly documented
seed and exports detached paths/values only. Its seed is not claimed to be the
original ActorEncroachmentCheck caller's packet initialization. The production
provider preserves its actual caller's complete packet instead.

UModel.PointCheck is a different service with zero/nonzero-extent branches,
owner transforms and recursive brush/leaf/hull checks. It currently refuses
explicitly in this adapter; a zero-length LineCheck is not a substitute.
Hash ActorEncroachmentCheck, complete Level.CheckEncroachment, movement, native
touch/base callbacks and original startup publication remain unfinished.
These boundaries must be completed before claiming playable crate physics.

The standalone cylinder test passes212583 checks/23 safety refusals. Independent
hardware-x87 controls cover32000 generated finite queries plus strict-contact,
adjacent Float, signed-zero, subnormal, cap/radial/zero-normal and inactive-NaN
cases; all complete packets and portable refusal behavior are compared. A
separate exact-integer oracle checks46656 overlap classifications without any
production arithmetic helper. Strict ARM64 kernel/test syntax and Android native
compilation/link pass. These are CPU/provider prerequisites, not headset physics
acceptance. Evidence uses `artifacts/training-actor-point-*20261018.log`.

Actual first-map provider controls cover227 constructed Engine.Cylinder and64
LodMesh owners (no plain Mesh instances in this map), with582 overlap and582
clear packets. They compare declared source dimensions/location and complete
defined packet outputs against the separately verified cylinder kernel. This
is adapter/authority evidence, not a second independent arithmetic oracle.
Active-invalid inputs explicitly refuse; source save/revision/lookup/client/GC/
RNG checks guard against diagnostic mutation. The original cylinder checkpoint
also refused Model owners. The subsequent full separate Model occupancy service
and actual selected-brush provider now supersede that refusal; see
ORIGINAL-MODEL-POINT-CHECK.md. It does not change this raw cylinder contract.

The complete owned exact38-source first-map collision/provider suite passes
611498 checks/39 refusals. Same-runtime Engine42647/31 and isolated crate347/two
regressions pass; the latter still does not retain usable loot in its dormant
Game=None fixture. Coherent host rebuild/CTest passes134 executed/158 registered,
24 explicit owned-data skips and zero failures in75.83s. No other real-map
integration or APK is included, and the known Move266 startup boundary remains.
