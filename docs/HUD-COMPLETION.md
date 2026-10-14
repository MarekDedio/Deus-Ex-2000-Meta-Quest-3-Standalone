# Working HUD release gate

The user requested that the complete working HUD precede another game build/APK.
Android packaging and installation stay paused until the source implementation
passes the offline integration checks below. Host unit/integration compilation,
owned-data inspection, source commits and checking the already installed APK are
still development work; none counts as a completed live HUD.

## Required before the next APK

- Accepted original player creation and native viewport pairing. The displayed
  values come from that actual player's state, not a dormant test pawn, health
  fixture, inventory-list cursor or artwork-only keyring.
- Starting NanoKeyRing produced by the original fresh-player initialization,
  including Login's ResetPlayerToDefaults/reinitialization. It owns the actual
  collected keys and occupies the special last-position slot labelled0.
- Ten original quick slots, original icons/labels and selected/pending state.
  Pickup, assignment, removal, replacement and activation act on actual owned
  items. Quick-slot controls must work without opening the inventory page.
- Independent six-limb health, energy and underwater oxygen; actual damage,
  healing and low-value transitions; original animation/threshold behavior.
- Weapon ammunition/clip/reload and item-copy/grenade counts sourced from the
  actual equipped inventory. Melee weapons keep the original N/A behavior.
- Original compass, target brackets/names/states, speech portrait/text, HUD
  messages and situational/augmentation indicators remain usable together.
- Original theme and HUD visibility/translucency settings, bounded predecoded
  artwork and no pickup-time synchronous package-loading regression.
- Inventory/health/augmentations/log/menu access remains functional. Panels
  stay comfortably inside the Quest view and their artwork, text and controller
  targeting use the same eye-facing placement.
- Warm/cold saves, restart, map replacement, viewport revocation/recenter and
  repeated initialization do not invent or lose ownership, duplicate windows,
  retain stale actors or leave a half-created HUD after a failure.

Once these pass offline, create the next signed APK, automatically install it
when the authorized Quest is connected, and verify its exact installed hash.
Physical comfort and end-to-end HUD/controller acceptance still require that
build on the headset; offline checks are not claimed as on-device acceptance.

## Current honest boundary

Training music/Jaime and the inset map-relative compass have human acceptance.
Original health/belt artwork, fonts and icons are decoded, but production
health/belt remain hidden until genuine player possession works. The isolated
original CreateKeyRing/GiveTo/AddInventory controls are not a live starter ring.

The native root bridge currently executes the original InitWindow prefix only.
Font/cursor setup and native object ownership now pass the actual original
callback-prefix checks. ActorDisplayWindow also completes its actual NewChild,
script/sine/font/clipping/hidden-show/alignment path. Actual verified Color
metadata now permits the original theme-null four-byte Context clearing; no
ThemeManager or theme color is fabricated. Ammo initialization, its HUD parent
visibility/layout callback, requested size and assignment now complete. The
first health body-part window gets its original head texture, translucent GC
style and requested geometry; initialization stops at SetTileColorRGB PC51/
native1475, whose uninitialized argument-buffer contract still needs support.
Native Root modal self-list,
constructor flags, preserved CDO fields and255 key/accelerator slots are checked
directly before rollback. This is still a pending construction graph. Full native
window construction, layout, lifecycle/input/rendering, player Login/Possess and
the remaining HUD behavior still need implementation and integration tests.
Unsupported callbacks refuse and roll back instead of publishing a partial HUD.
The original-data control passes1459 checks of the pending graph and complete
rollback; the ordinary host suite
passes79 tests/92 registered with13 explicit owned-data skips and zero failures.
These results do not satisfy the full-HUD functional acceptance checklist above.

The six-limb presentation clock now follows the original .4s damage flash,
1s healing and .5s refresh statement order, including retained applied tint when
the script would early-return. The bound-player rendering path ticks it per
visible frame over bounded10Hz real-state reads, uploads changed images only,
and resets presentation on player/map/unbind.357 offline checks pass. This is
not evidence that live possession, underwater oxygen or the full HUD is ready.

Startup fixes now include the real PlayerNone ConsoleCommand guard, FMax and
integer Clamp primitives. Independent qualified instance storage also preserves
Actor.Region's PointRegion separately from GameReplicationInfo.Region's Int,
including save/load. Hidden inherited class-default writes remain unsupported;
instance support must not be presented as completed mutable-CDO shadow support.

NPC poses/animation, general gravity, breakable crates and lamp illumination are
the following tutorial priorities, not substitutes for finishing this HUD gate.
