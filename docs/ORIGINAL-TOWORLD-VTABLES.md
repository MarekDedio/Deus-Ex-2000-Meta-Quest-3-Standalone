# Original native Actor ToWorld virtual authority

Static, read-only audit of the owned GOTY installation, 2026-10-16. No original
DLL/EXE was loaded or executed. Only this document was created; no runtime or
build was changed. Addresses below are preferred PE32 virtual addresses, not
portable pointers or reflected-property offsets.

## Scope and binary identities

Files are under `D:/Steam/steamapps/common/Deus Ex/System`:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| DeusEx.dll | `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04` |
| Extension.dll | `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| DeusEx.u | `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480` |
| Extension.u | `9984FAEDA55FB2100B1A15A37BFEB8CDB4878248B39AFF832002A799F1D787F7` |

PE headers, section mappings, named exports, export ordinals, import lookup and
address tables were decoded directly from these bytes. Each exported primary
Actor-family vtable's DWORD at byte offset `+0x88` was read, then its E9 or FF25
thunk resolved. This is not an assumption based on C++ inheritance or an
`Engine.*` class-name prefix.

For completeness, the four original v68/licensee0 packages Engine, DeusEx,
Extension and Core were independently read as package tables: compact indices,
names, export Class/Base/Outer/Name/Flags and imports. Actual UClass exports
(`ObjClass==0`) were traversed by `ObjBase` using fully qualified import/export
paths. All native Actor-descendant UClass exports in the three audited packages
have actual object flags `0x040F0004`, including **RF_Native `0x04000000`**.
There are exactly **47 Engine, 8 DeusEx and 1 Extension** such classes, and each
has exactly the matching exported primary vtable below. No native Actor class
in those package tables is missing from this set. In particular this result
does not use CLASS flag `0x80` (RuntimeStatic) to identify native classes.

Other modules, changed binaries, unrecognized native classes and arbitrary
same-leaf class names are outside this audit, not implied additions to it.

## Two implementations, no further override in the audited class set

| Label | Virtual slot target and resolution |
| --- | --- |
| Actor | Engine thunk `10303AC6` (E9) -> `1030DAB0..1030DE70`, exported `?ToWorld@AActor@@UBE?AVFCoords@@XZ` |
| Brush | Engine thunk `103023AB` (E9) -> `1030F310..1030F4F6`, exported `?ToWorld@ABrush@@UBE?AVFCoords@@XZ` |

Only Engine.Brush and Engine.Mover use Brush. Every other audited class uses
Actor, either directly or through the import thunk stated below. There is no
additional native Actor-subclass ToWorld override among these 56 tables.
This distinguishes an Actor receiver with a nonnull Brush primitive from an
actual native Brush/Mover receiver: the former's class does not thereby acquire
the latter's ToWorld implementation.

### Engine.dll: all 47 actual native Actor classes

The export column is the one-based original Engine.u UClass export, not an
actor instance index. Actor/Brush refer to the exact resolved implementation
above. The vtable export symbol is `??_7A<ClassLeaf>@@6B@`; the authored spelling
`locationid` really is lowercase in its native export.

| Actual class path | UClass export | Immediate authored base | Vtable VA | +88 target | Implementation |
| --- | ---: | --- | --- | --- | --- |
| Engine.Actor | 2 | Core.Object | 104268DC | 10303AC6 | Actor |
| Engine.Brush | 816 | Engine.Actor | 10426C9C | 103023AB | Brush |
| Engine.ButtonMarker | 5607 | Engine.NavigationPoint | 10427EE8 | 10303AC6 | Actor |
| Engine.Camera | 5595 | Engine.PlayerPawn | 10426B18 | 10303AC6 | Actor |
| Engine.CameraPoint | 62 | Engine.Keypoint | 10427178 | 10303AC6 | Actor |
| Engine.Carcass | 196 | Engine.Decoration | 1042839C | 10303AC6 | Actor |
| Engine.Decal | 809 | Engine.Actor | 10426F74 | 10303AC6 | Actor |
| Engine.Decoration | 45 | Engine.Actor | 104282F0 | 10303AC6 | Actor |
| Engine.GameInfo | 13 | Engine.Info | 10427AE0 | 10303AC6 | Actor |
| Engine.GameReplicationInfo | 108 | Engine.ReplicationInfo | 10427428 | 10303AC6 | Actor |
| Engine.HUD | 54 | Engine.Actor | 104284F4 | 10303AC6 | Actor |
| Engine.Info | 91 | Engine.Actor | 104272D0 | 10303AC6 | Actor |
| Engine.InternetInfo | 5610 | Engine.Info | 104276D8 | 10303AC6 | Actor |
| Engine.InterpolationPoint | 194 | Engine.Keypoint | 10427224 | 10303AC6 | Actor |
| Engine.Inventory | 6 | Engine.Actor | 104286F8 | 10303AC6 | Actor |
| Engine.InventorySpot | 1239 | Engine.NavigationPoint | 10428040 | 10303AC6 | Actor |
| Engine.Keypoint | 148 | Engine.Actor | 10427020 | 10303AC6 | Actor |
| Engine.LevelInfo | 3 | Engine.ZoneInfo | 10427A34 | 10303AC6 | Actor |
| Engine.LiftCenter | 5609 | Engine.NavigationPoint | 10427D90 | 10303AC6 | Actor |
| Engine.LiftExit | 132 | Engine.NavigationPoint | 10427CE4 | 10303AC6 | Actor |
| Engine.Light | 631 | Engine.Actor | 104288FC | 10303AC6 | Actor |
| Engine.locationid | 188 | Engine.Keypoint | 104270CC | 10303AC6 | Actor |
| Engine.Menu | 113 | Engine.Actor | 10428448 | 10303AC6 | Actor |
| Engine.Mover | 37 | Engine.Brush | 10426E08 | 103023AB | Brush |
| Engine.Mutator | 46 | Engine.Info | 10427B8C | 10303AC6 | Actor |
| Engine.NavigationPoint | 29 | Engine.Actor | 10427C38 | 10303AC6 | Actor |
| Engine.PathNode | 5598 | Engine.NavigationPoint | 10428244 | 10303AC6 | Actor |
| Engine.Pawn | 1 | Engine.Actor | 10426994 | 10303AC6 | Actor |
| Engine.PlayerPawn | 5 | Engine.Pawn | 10426A40 | 10303AC6 | Actor |
| Engine.PlayerReplicationInfo | 7 | Engine.ReplicationInfo | 104274D4 | 10303AC6 | Actor |
| Engine.PlayerStart | 603 | Engine.NavigationPoint | 104280EC | 10303AC6 | Actor |
| Engine.Projectile | 193 | Engine.Actor | 10428850 | 10303AC6 | Actor |
| Engine.ReplicationInfo | 805 | Engine.Info | 1042737C | 10303AC6 | Actor |
| Engine.SavedMove | 19 | Engine.Info | 10427784 | 10303AC6 | Actor |
| Engine.Scout | 5604 | Engine.Pawn | 10426BF0 | 10303AC6 | Actor |
| Engine.SkyZoneInfo | 592 | Engine.ZoneInfo | 10427988 | 10303AC6 | Actor |
| Engine.SpawnNotify | 247 | Engine.Actor | 10426EC8 | 10303AC6 | Actor |
| Engine.StatLog | 20 | Engine.Info | 10427580 | 10303AC6 | Actor |
| Engine.StatLogFile | 2383 | Engine.StatLog | 1042762C | 10303AC6 | Actor |
| Engine.Teleporter | 245 | Engine.NavigationPoint | 10428198 | 10303AC6 | Actor |
| Engine.Trigger | 162 | Engine.Triggers | 1042864C | 10303AC6 | Actor |
| Engine.TriggerMarker | 5606 | Engine.NavigationPoint | 10427F94 | 10303AC6 | Actor |
| Engine.Triggers | 215 | Engine.Actor | 104285A0 | 10303AC6 | Actor |
| Engine.WarpZoneInfo | 376 | Engine.ZoneInfo | 104278DC | 10303AC6 | Actor |
| Engine.WarpZoneMarker | 5608 | Engine.NavigationPoint | 10427E3C | 10303AC6 | Actor |
| Engine.Weapon | 9 | Engine.Inventory | 104287A4 | 10303AC6 | Actor |
| Engine.ZoneInfo | 16 | Engine.Info | 10427830 | 10303AC6 | Actor |

### DeusEx.dll: all 8 actual native Actor classes

Every +88 slot contains `1001AF1A`. Its bytes are an indirect import jump
`FF 25 14 72 02 10`, using IAT `10027214`, whose ordered import name is
Engine.dll `?ToWorld@AActor@@UBE?AVFCoords@@XZ`. Resolving that actual export
therefore reaches Actor thunk `10303AC6` and body `1030DAB0`, not a local
DeusEx override or merely the name of an expected base class.

| Actual class path | UClass export | Immediate authored base | Vtable VA | +88 target |
| --- | ---: | --- | --- | --- |
| DeusEx.Augmentation | 7 | Engine.Actor | 10027984 | 1001AF1A |
| DeusEx.AugmentationManager | 46 | Engine.Actor | 100278F4 | 1001AF1A |
| DeusEx.DeusExDecoration | 14 | Engine.Decoration | 10027744 | 1001AF1A |
| DeusEx.DeusExLevelInfo | 71 | Engine.Info | 100276B4 | 1001AF1A |
| DeusEx.DeusExPlayer | 1 | Extension.PlayerPawnExt | 10027568 | 1001AF1A |
| DeusEx.ScriptedPawn | 5 | Engine.Pawn | 10027624 | 1001AF1A |
| DeusEx.Skill | 21 | Engine.Actor | 10027864 | 1001AF1A |
| DeusEx.SkillManager | 131 | Engine.Actor | 100277D4 | 1001AF1A |

### Extension.dll: its sole actual native Actor class

| Actual class path | UClass export | Immediate authored base | Vtable VA | +88 target |
| --- | ---: | --- | --- | --- |
| Extension.PlayerPawnExt | 70 | Engine.PlayerPawn | 1005C32C | 1005459A |

Thunk `1005459A` is `FF 25 54 92 05 10`, using IAT `10059254`. Its import is
again Engine.dll `?ToWorld@AActor@@UBE?AVFCoords@@XZ`, resolving to the same
Actor implementation. No Brush import or local override occurs in this slot.

## Runtime admission contract, not an integration claim

A future provider can select Actor versus Brush from the **nearest actual
RF_Native UClass** in a bounded, cycle-checked real Actor ancestry, using an
explicit fully qualified allowlist derived above. Script-only subclasses
inherit that nearest native implementation. It must refuse unknown nearest
native classes, malformed ancestry, UClass-as-instance confusion, foreign
same-leaf classes and unavailable actual Actor storage; an arbitrary native
class is not admitted just because an earlier ancestor was recognized.

When UModel is empty, retail returns RootOutside before reading Owner or
calling ToWorld. Do not introduce a native-receiver/coordinate refusal on that
inactive branch. A nonempty owner extent query captures the Owner virtual
+88 result once in FBoxCheck and retains its full 48-byte value for the query.
This audit establishes which original implementation supplies that value;
the field/schema reads, both Brush scale-block mappings, sine-profile
provenance, current primitive selection, actual Model/owner identity, bounds
versus narrow-phase distinction and shared VM transaction still require their
own authority. Refer to ORIGINAL-BRUSH-COORDINATES.md and
ORIGINAL-MOVER-LINE-TRACE.md for the separate numeric/owner-sweep contracts.

No live native provider, Move266, FindBase, world startup, crate destruction,
falling physics, APK or on-headset acceptance is established by this document.
