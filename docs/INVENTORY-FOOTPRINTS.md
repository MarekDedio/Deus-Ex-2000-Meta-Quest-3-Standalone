# Original Inventory footprints and display scope

The original single-player grid is **5 columns by 6 rows**, not a different
aspect-ratio layout. The previous port error was treating every item as one
cell and resizing an entire padded texture to fit it.

## Original-data evidence

This contract was inspected read-only from the user's `System/Engine.u` and
`System/DeusEx.u`, under `D:/Steam/steamapps/common/Deus Ex`. Commercial package
bytes and inspected ScriptText remain outside version control. Local ignored
evidence is in `artifacts/inventory-fidelity-20261009/`:

- `original-inventory-contracts.log:12` and `:13`: `DeusExPlayer` defaults
  `maxInvRows=6`, `maxInvCols=5`; its authored inventory array has 30 slots.
- `original-persona-inventory.log:2`: `PersonaScreenInventory` uses 53-pixel
  horizontal/vertical steps, and 40 by 35 logical pixels for the small-icon
  fallback. `CreateItemsWindow` starts at client-local `(9,19)` and measures
  `(266,319)`. With the original client origin `(33,43)`, that is `(42,62)`.
- `CreateInventoryButtons` creates one child per displayable item, uses
  `largeIcon` and its authored logical width/height when present, and sets the
  child's dimensions to `(53*invSlotsX+1, 53*invSlotsY+1)`. `SetItemButtonPos`
  multiplies each effective `invPos` coordinate by 53. The compiled original
  functions are also recorded in `original-compiled-inventory-contract.log`.
- `original-inherited-slot-defaults.log:1`: both slot dimensions default to
  1, both positions to -1, and displayability to true in `Engine.Inventory`.
  Its class hierarchy was actually traversed; a missing subclass override was
  not mistaken for zero or inferred from an icon's alpha bounds.

| Authored class | Slots | Child pixels | Large-icon logical source window |
| --- | --- | --- | --- |
| `DeusEx.WeaponRifle` | 4 by 1 | 213 by 54 | 159 by 47 |
| `DeusEx.WeaponAssaultGun` | 2 by 2 | 107 by 107 | 94 by 65 |
| `DeusEx.WeaponGEPGun` | 4 by 2 | 213 by 107 | 203 by 77 |
| `DeusEx.WeaponPistol` | 1 by 1 | 54 by 54 | 46 by 28 |
| `DeusEx.MedKit` | 1 by 1 | 54 by 54 | 39 by 46 |
| `DeusEx.BioelectricCell` | 1 by 1 | 54 by 54 | 44 by 43 |
| `DeusEx.Multitool` | 1 by 1 | 54 by 54 | 28 by 46 |
| `DeusEx.Lockpick` | 1 by 1 | 54 by 54 | 45 by 44 |
| `DeusEx.WeaponLAM` | 1 by 1 | 54 by 54 | 35 by 45 |

The rifle's Y size specifically comes from `Engine.Inventory`, not from its
own class default block. The tool and LAM facts have separate ignored
`original-tools-slot-defaults.log` and `original-lam-slot-defaults.log` evidence.

The original `PersonaInventoryItemButton.DrawWindow` centers a masked icon
using its logical dimensions and draws one selection border around the entire
item. Its nine `PersonaItemHighlight_*` pieces, in TL/TR/BL/BR/left/right/top/
bottom/center order, are actual `PersonaItemButton.texBorders` defaults
(`original-inventory-contracts.log:103`). Selection does not become a separate
highlight for each occupied cell. The inspected allocation, drag and draw
paths do not rotate an item or swap its width/height to fit.

Pinned `third_party/SurrealEngine/SurrealEngine/Packages/Extension/Windows/UGC.cpp`
`DrawTexture` at lines 280-291 uses a source window equal to the requested
destination width/height. Thus a 159 by 47 rifle icon is cropped from the
upper-left 159 by 47 pixels of its decoded padded texture; it is not a resized
256 by 64 image. `DrawBorders` at lines 73-208 supplies the nine-piece stretch
rules, including its asymmetric bottom-edge TL-origin rule. The CPU compositor
uses integer virtual-canvas centering; fractional GPU rasterization/blending
has not been established by this inspection.

## Implemented bounded correction

`native/persona_ui_canvas.h` now accepts detached slot/position/icon-window
metadata, reserves all valid assigned rectangles first, and packs only fully
unassigned `(-1,-1)` items row-first without rotation. It draws one full-size
child rectangle, a centered cropped icon, and original selection artwork.
Capacity is based on occupied cells, not the number of items: excess items are
reported as unplaced, not squeezed into one cell or paginated as a fake second
original inventory grid. Bad dimensions, partial positions, overlapping
assigned items and out-of-grid rectangles are rejected. Layout work/storage is
bounded to 4096 metadata rows and 30 occupied cells; icon-copy work is clipped
to the 266 by 319 grid. Sources are preflighted before destination painting.

Quest's refresh adapter consumes `ReadPortableRuntimeInventoryDescriptors` for
actual explicit inventory identities. It does not derive a class or icon from
a substring of an actor name. The reader includes indexed inactive pickups,
retains declaration/effective-owner/source provenance, executes no scripts and
allocates no game actors. A synthetic `Class@event` conversation token remains unindexed
and cannot acquire a guessed icon or fabricated actor. Non-displayable items
do not get grid space. Icon caching uses the actual UI source path plus exact
qualified object path, is cleared on each committed UI package load, and is
bounded to 256 entries / 64 MiB retained image bytes. Unsupported icon packages
are reported rather than resolved to a same-named UI asset.

The original indexed pickup may still have `invPosX=invPosY=-1` after the port's
pickup path. The resulting first-fit packing is **display-only fallback**:
it neither runs the original `FindInventorySlot`/`SetInvSlots` lifecycle nor
writes generated coordinates into actors, checkpoints or saves. Diagnostic
`displayOnlyPacked` counts distinguish that fallback from effective positions.
Original `CreateInventoryButtons` rebuilds the player's slots and may update
actor positions; this read-only adapter deliberately does neither.

Desktop default icon fixtures explicitly name the classes in the table and
read their inherited defaults from the user's original sibling packages. Each
declared large-icon reference must equal the requested asset. The preview JSON
records class/property/source provenance, full rectangles and generated
positions. It is not evidence of a live inventory or executed pickup.
The sample detail pane identifies the actual selected fixture class/asset;
it does not keep an unrelated Pistol description when another icon is selected.
Unknown asset-only fixture icons retain the prior safe one-cell preview fallback and
are labelled `one-cell asset fixture`; they do not claim original footprint
metadata. Original-class fixture input is bounded to 256 rows, with metadata
loaded once per distinct explicit class within the preview.

## Targeted tests and remaining fidelity gaps

`VerifySharedPersonaCanvas` exercises rifle/assault/GEP footprints, row-major
packing, assigned-position priority, preservation of input positions, capacity
overflow, hidden/unavailable metadata, malformed dimensions/positions,
cropped padded textures, alpha masking, whole-item selection, asymmetric
nine-slice stretch/ordering, bounded selection rectangles and destination
preservation on rejected sources. The preview wrapper expects 31 Inventory
artwork assets (the prior 22 plus nine original selection pieces) and checks
class-default/placement provenance. These tests are CPU contracts, not headset
or campaign-success evidence.

The generated descriptor controls also include a positive same-package Texture
class constraint, alongside imported/native and malformed/type-8 refusals. The
original three-weapon metadata integration initially rejected all icons because
an already qualified class path was qualified twice (`Engine.Engine.Texture`).
Removing that redundant qualification fixes the original-data integration
without weakening class/object/type checks. Full host CTest and both separate
buffered original-data integrations pass; Android ARM64 also compiles.

Remaining behavior is explicitly incomplete:

- Neutral opaque gray item fills and generic normal edges remain a port
  approximation; original theme-driven translucent fill/selection colors and
  blending have not been executed.
- The controller still enumerates the raw inventory list. A hidden,
  unavailable or capacity-unplaced selected entry can have no visible
  highlight; `selectedVisible=0` reports this. It is not silently shown as a
  placeholder, and this change does not claim faithful visible-item navigation.
- Belt-position numbers, ammunition/copy labels, equipment state and drag/drop
  interaction are not supplied by this footprint adapter.
- Original action chrome uses Change Ammo / Drop / Use / Equip (including
  conditional enablement and UnEquip); the existing working VR Use / Save /
  Load / Close labels/bindings remain unchanged. All eight authored navigation
  labels are retained, but only the existing four implemented pages are active.
- The authored side panes, status/credits, key-ring/ammo buttons and object
  belt are not made functional by correcting rectangles. Live inventory
  lifecycle, standalone Quest GPU presentation and controller behavior require
  separate verification.
