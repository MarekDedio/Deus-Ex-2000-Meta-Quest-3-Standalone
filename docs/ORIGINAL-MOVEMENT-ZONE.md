# Movement-time ULevel.SetActorZone contract (2026-10-19)

Static audit for the native movement path required by first `00_Training`.
This static audit does **not** itself implement Move266/299, world startup,
support/falling or playable crate destruction/loot. The subsequent complete
ordinary SetActorZone kernel and actual typed rollback-only providers are
documented in TRAINING-ACTOR-ZONES.md; they are not falsely published through
the still-unsupported movement call. The implemented
PointRegion query is documented separately in ORIGINAL-MODEL-POINT-REGION.md.
The existing world/birth `(actor,1,1)` helper must not be substituted for the
movement call `(actor,bTest,0)`.

## Source authority

Owned GOTY files, read-only static inspection, never loaded/executed:

- Engine.dll SHA256
  `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- Core.dll SHA256
  `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

ULevel vtable `10428AFC+AC` reaches thunk `10302A77`, complete ordinary body
`1039B940..1039BE94` and catch tail `1039BE97..1039BEC6`. Source dump:
`artifacts/world-native-startup-20261011/original-setactorzone-body.asm`.
Additional complete helper bodies and identity/hash/EH records are in
`artifacts/training-movement-zone-20261019/`:

- GetLevelInfo thunk `10303DDC` to `10346110..1034618F`;
- eventHeadZoneChange thunk `103032E7` to `10326290..103262BE`;
- Core FVector constructor `10117690..101176A8`;
- Core FVector addition `10117740..10117762`;
- original EH stub at `10420560`, metadata `104477D8`.

The disassembler's nearby AInventory label does not name SetActorZone.
IAT entries identify UObject.IsA `10599A74`, FindFunctionChecked `10599A88`,
FVector(Float,Float,Float) `10599B28`, FVector addition `1059A038`.
Original event FName globals are ActorLeaving `105836E8`, ActorEntered
`105836EC`, ZoneChange `10583708`, FootZoneChange `105836CC` and
HeadZoneChange `105836C8`; see WORLD-NATIVE-STARTUP.md.

## Complete ordinary control flow

The receiver is the supplied native Level, not a mutable Actor.Level field.
The Actor argument is required nonnull. Treat both integers as any-nonzero
booleans, not exactly1 or a selected-bit test.

1. Inspect the Actor's initial bDeleteMe bit. If set, return before slot0,
   Pawn ancestry, force reset, coordinates, BSP or callbacks. There are **no
   additional deletion checks** after callbacks in this body.
2. Validate native Actors data slot0 nonnull and IsA(LevelInfo), then reread
   slot0 for comparison with the Actor (`1039B9A0..1039B9EB`). This is not a
   LevelInfo class scan or reflected Actor.Level override.
3. If the Actor equals slot0, validate the LevelInfo again and write exactly
   Region `(slot0,-1,0)` using the final slot0 reread, then return
   (`1039B9ED..1039BA5A`). No Pawn classification/coordinate/BSP/event access.
4. Otherwise cache whether the Actor has native Pawn ancestry
   (`1039BA5D..1039BA8A`). Do not reclassify it after callbacks.
5. If force-refresh is nonzero, reset main Region to a freshly validated
   slot0 fallback tuple. For a Pawn, validate slot0 again and reset HeadRegion
   and FootRegion to the same tuple (`1039BA8C..1039BB76`). The raw stores
   interleave: Head.Zone, Foot.Zone, Head.iLeaf, Foot.iLeaf, Head.ZoneNumber,
   Foot.ZoneNumber. These are native stores, not zone-change callbacks.
6. Select the main PointRegion fallback by the Level's live Actor-array count
   (`Level+30`, `1039BB79`). Nonzero count performs another GetLevelInfo-style
   slot0 validation/reread. Zero count passes the Actor itself as fallback,
   despite the earlier slot0 assertion path. Do not silently replace that
   count-dependent selection with a saved slot0 pointer.
7. Copy the current Actor.Location words X/Y/Z by value and query this native
   Level.Model (`1039BBCE..1039BBF7`). Only after the query, read the Actor's
   current main Region.Zone (`1039BBFC`).
8. If the new Zone differs and bTest is zero: dispatch
   `oldZone.ActorLeaving(actor)`, then `actor.ZoneChange(newZone)`, then write
   **all three** computed main Region members, then reread stored Region.Zone
   as the receiver of `ActorEntered(actor)` (`1039BC1E..1039BCA1`). The old
   Zone is the single postquery snapshot used for ActorLeaving; the new tuple
   remains the computed value across the callbacks. Do not prepublish it,
   reorder the events, or replace the final live receiver read with an
   unrelated lookup. Native has no null-old-zone guard here; portable safety
   must explicitly refuse an unsafe active event receiver.
9. Otherwise publish all three computed main Region members without events
   (`1039BCA6..1039BCC1`). Same Zone identity does not mean skip the leaf or
   ZoneNumber refresh. A nonzero bTest suppresses callbacks, not these stores.
10. Non-Pawns return. For the cached Pawn identity, construct a foot point
    using **current**, post-main-callback state. CollisionHeight is loaded
    before Location.Z; Z is Float-spilled `Location.Z-CollisionHeight`; Y/X
    are copied raw to the FVector constructor (`1039BCCD..1039BCF1`). Validate
    this Level's current slot0 as fallback and query its current native Model.
    Compare the resulting Zone with the live FootRegion.Zone. If different and
    bTest zero, dispatch `pawn.FootZoneChange(newFootZone)` **before** publishing
    the computed foot tuple (`1039BCF7..1039BDB4`).
11. After that optional callback, load the Pawn's current EyeHeight
    (`1039BDB7`), then publish the computed FootRegion fields (Zone/leaf/number)
    across `1039BDC0..1039BDD5`, regardless of identity equality or bTest.
    EyeHeight's read is before those raw publication stores, not after them.
12. Construct `(0,0,EyeHeight)` and use the original Core FVector **addition**
    with the Pawn's live Location (`1039BDD8..1039BDE6`). This samples Location
    after FootZoneChange and computes/stores X/Y/Z individually. The Core body
    loads the offset channel, adds the Location channel and Float-stores the
    result. Thus head X/Y are Float `+0+Location.X/Y`, **not raw copies**;
    signed-zero handling distinguishes them. Z is Float `EyeHeight+Location.Z`.
    Do not take head/foot/height snapshots together at function entry.
13. GetLevelInfo on the original supplied Level, query that Level's current
    Model, then compare the result with live HeadRegion.Zone
    (`1039BDEC..1039BE30`). If different and bTest zero, dispatch the genuine
    `pawn.HeadZoneChange(newHeadZone)` wrapper. It uses FindFunctionChecked and
    this Pawn's ProcessEvent, not a hand-written water/physics substitute.
    Publish all three computed head members after the callback regardless of
    identity equality/test (`1039BE32..1039BE52`).
14. GetLevelInfo again. If its current NetMode is3, return without reading PRI.
    Otherwise read live Pawn.PlayerReplicationInfo; if nonnull, read the Pawn's
    **current main Region.Zone** and write PRI.PlayerZone
    (`1039BE55..1039BE94`). A HeadZoneChange callback can therefore affect this
    final assignment. It is not the initial point-query Zone, FootZone or HeadZone.

Full FPointRegion copies include only the declared Byte's meaningful low byte;
native stack padding/uninitialized upper Byte-storage bits are not a portable
value contract. Defined leaf and actor identities remain exact.

## Provider contract and remaining integration

Use exact declared Region/FootRegion/HeadRegion storage, not the convenient
read-only Region fallback as a substitute for old native tuples. Providers must
share the same outer movement VM execution, event gating, work/memory budget,
Level/Model authority and rollback journal. Main/foot/head callbacks must be
able to mutate the later sampled values. Do not add postcallback bDeleteMe
early exits, force a begun world/role, or publish dormant test roots as gameplay.
Unsafe stale/null/type identities and bounds may be explicitly refused with
complete root rollback; that stronger safety is not retail EH behavior.

There is no native SetBase, direct bWaterZone-to-Physics assignment, support
search or falling step in this body. Existing SpawnZone's additional behavior
does not establish this movement contract. The subsequent kernel has independent
event/store-order, callback-mutation, shared-budget and reentrant controls; its
typed diagnostic providers are tested on the actual first map. Those dormant
world probes do not execute live zone-change bytecode or prove MoveActor,
startup, support/falling or surviving contents. Other campaign-map integration
remains deferred.
