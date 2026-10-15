# HUD test milestone and remaining integration

## Revised user-approved test milestone (2026-10-15)

The latest instruction supersedes the full-HUD APK gate below: show the
original health artwork even while incomplete, finish the playable quick
inventory bar with its starter keyring (held keyring mesh excluded), then build
and install that test APK on the connected Quest. Do not wait for unrelated
original-window/augmentation or full player-possession work for this milestone.

The current playable port uses native inventory ownership and collected NanoKey
actors. Its functional starter keyring aggregates those keys; it is not claimed
to be an original-script Pawn-owned UObject. Quick slots/equipped identity and
saves use that live playable state. The temporary health display mirrors the
aggregate health across all six segments and shows placeholder full energy;
independent limb damage, energy and original possession remain unfinished.

## Previous full-HUD gate / remaining integration checklist

The earlier instruction required the complete original HUD before packaging.
That restriction was superseded by the revised milestone above. The following
list remains the broader integration target, not a gate for the installed
health-preview/quickbar test APK.

## Remaining full original-HUD integration

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

Physical comfort and end-to-end HUD/controller acceptance still require testing
the installed build on the headset; offline checks are not on-device acceptance.

## Current honest boundary

Training music/Jaime and the inset map-relative compass have human acceptance.
The installed milestone shows original health/belt artwork and fonts using the
live native playable equipment and aggregate-health presentation described above.
An inspected software preview confirms actual Training pickups, selected slots
and collected-key count render correctly. This is not headset-view acceptance.
The isolated original CreateKeyRing/GiveTo/AddInventory controls still do not
establish original-script live player possession.

The native root bridge currently executes the original InitWindow prefix only.
Font/cursor setup and native object ownership now pass the actual original
callback-prefix checks. ActorDisplayWindow also completes its actual NewChild,
script/sine/font/clipping/hidden-show/alignment path. Actual verified Color
metadata now permits the original theme-null four-byte Context clearing; no
ThemeManager or theme color is fabricated. Ammo initialization, its HUD parent
visibility/layout callback, requested size and assignment now complete. The
six health body-part windows now receive their original textures, translucent
GC styles, requested geometry and colors. The body overlay is lowered beneath
them with its original armor color. Both energy/breath ProgressBarWindows and
the crosshair initialize and are assigned to the actual pending HUD. Audited
IntToString0x53 now allows all ten augmentation key labels to initialize. The next
original boundary is DamageHUDDisplay.InitWindow PC39: the authored struct-array
String member sIconInfo.DamageType is unsupported. ConfigureChild1461 now executes the audited Window geometry,
callback, aligned-child resize, clipping and selectable-resort path; native
Radio/Tile/Root configuration overrides still refuse. QueryPreferredSize executes its original
cache/callback/texture-fallback path. All ten HUDObjectSlot Text/Button/Toggle
constructors now execute, including temporary selectable-table insertion/removal,
original FontTiny, requested size, theme-null colors and sibling lowering.
The Radio retains Toggle creation order; the Tile sibling order is0,9,8,...,1.
RadioBox/TileWindow construction, Modal tab registration, hidden visibility and
Tile margin/spacing/order setters execute. The bounded diagnostic captures132
objects/8783 writes with no inspection error (512 objects/4MiB capture limits);
this remains an unpublished, rolled-back original initialization prefix.
The belt has completed its script InitWindow, locally shown itself, returned
from its parent's layout callback and been assigned to HUD.belt. Root.hud remains None;
this does not establish working equipment or possession. Native Root modal
self-list, constructor flags, preserved CDO fields and255
key/accelerator slots are checked
directly before rollback. This is still a pending construction graph. Full native
window construction, layout, lifecycle/input/rendering, player Login/Possess and
the remaining HUD behavior still need implementation and integration tests.
Unsupported callbacks refuse and roll back instead of publishing a partial HUD.
The coherent host suite passes87 ordinary tests/101 registered,14 explicit
owned-data skips, zero failures (16.76s). The separate owned-data control passes6095 checks covering
all ten actual slots, Radio/Tile hierarchy, native table indices/display records, original
properties, oxygen sources, repeated initialization, forced-budget rollback
and warm/cold ring/session ownership. It also checks all ten F3–F12 augmentation
labels, completed container initialization, actual crosshair geometry, the
later configured belt clip offsets, drawing-GC propagation and unchanged
constructor/general GCs.
Runtime/VM ARM64 syntax-only checks pass.
The85 non-preview original-artwork controls also pass with unchanged source files;
inspected software previews are explicit
fixtures, not live Quest health/equipment acceptance. The revised native playable
milestone has since been built and installed; see QUEST-TEST-BUILD.md.
The separate console control passes65 checks/one expected refusal. The focused
original world probe retains1308 actors and the same Move266 startup boundary,
with complete rollback. These results do not satisfy the full-HUD functional
acceptance checklist above; the current APK implements the narrower approved milestone.

The Text constructor owns an empty String instead of Win32 FString heap bytes.
Button's six original display records, timing/pressed state and Toggle sounds
are initialized with audited declaration identities. Retail constructor colors
only establish RGB; defined A0 is an explicit portable policy, not original
alpha evidence. SetSelectability keeps actual Radio/TabGroup tables and indices,
focus negotiation still explicitly refuses nonnull focus, and unsupported paths
roll back the whole unpublished graph. A PC64 integer-dyadic location helper
preserves X's extended comparison versus Y's earlier Float store;20641 controls,
including20468 independent host x87 tuples, pass without executing original code.

Additional audited Tile child-alignment/equal-width/equal-height setters are
implemented with runtime-owned Byte/Bool argument seeds. The current original
prefix stops before their later information-panel use; unit argument coverage
is not presented as live information-panel acceptance.

The implemented1465 declaration is Extension.Window.QueryPreferredSize, flags0x401,
with two Float OUT parameters (0x180), not ordinary copied inputs. Retail thunk
10053160 captures each actual output reference, falling back to independent
initialized Float temporaries; do not add1465 to ordinary-input snapshots.
QueryPreferredSize1004e710 delegates to1004e790, which has requested-size,
dirty-query/cache, dynamic ParentRequestedPreferredSize and actual background-size
fallback branches. Its callback is NOT named QueryDesiredSize. Window's native
virtual10050390 forwards an initialized Bool/Float-OUT/Bool/Float-OUT packet to
the most-derived script function. Original-data construction and rollback
controls verify this query path. Negative cached results retain pre-fallback values;
each call obtains fallback dimensions from the actual Texture's authored USize/
VSize, or current Width/Height when Background is None. Callback references own
their float storage so rollback cannot retain dangling stack pointers.
Bitmap dimensions are captured from original tagged properties during package/
map preparation, not by opening packages or decoding images in the query.
Missing/malformed metadata stays unavailable; current typed runtime overrides
remain authoritative. The sizing helper passes147 branch/cache/reentrancy,
signed-zero and nonfinite-refusal checks. Native output aliases are guarded
exactly once and are kept distinct from copied Float expression temporaries.

A corrected native offset audit also distinguishes Window.X/Y/Width/Height
(+78/+7c/+80/+84) from the six-Float winClipRect (+d4..+e8). Selectable-table
sorting uses clip originX/Y; location eligibility uses positive clipWidth/
clipHeight, and the first visible location adds clip offsets to its origin.
The previous local-geometry interpretation was incorrect for clipped windows
and has been replaced; unchanged hidden constructor fixtures alone did not
prove those visible selection semantics.

The following layout dependency1461 is Extension.Window.ConfigureChild,
flags0x401 and four mandatory Float inputs (0x80). Its wrapper10052f60 seeds
each Float0 independently and copies X,Y,width,height in order; no clamp is
proved. Body1004ec50 locks Self and sets bConfigured before the local visibility
test. A locally hidden Self skips geometry; a locally visible Self clears
bNeedsReconfigure and stores current X/Y/Width/Height even under a hidden root.
Size change or prior bNeedsReconfigure invokes ChangeConfiguration10050f90;
outermost ConfigureChild (or a TabGroup) clips the actual subtree and resorts
selection tables. It must not be substituted with SetConfiguration, which
stores requested geometry only. Original ChangeConfiguration performs dynamic
script ConfigurationChanged and aligned-child ResizeChild propagation. These
contracts now drive the runtime with exception-scoped configuration globals,
actual lock counts and journaled fields. Window/Text/Button/Toggle use their
verified shared optional script callback; other native configuration overrides
and deferred-unlock destruction refuse and roll back. The original crosshair
now reaches16x16 at(-2.5,-2.5) under the pending10x10 HUD and copies the clipped
rectangle into its actual drawing GC. Constructor clip initialization remains
separate: it copies raw parent origins and size without intersection or a GC
clip write. No native callback is bypassed to publish this pending graph.

The new configure/clipping/resize helpers pass130745 controls, including82368
independent host x87 PC64 arithmetic/clip/resize comparisons. Production uses
portable integer dyadics, not host long-double assumptions. Clipping retains
negative intersections, live PC64 bottom-edge arithmetic and unconditional
descendant traversal; sorting does not append duplicate selection entries.
Resize captures parent size before the query but rereads child alignment and
margins afterwards. Centered positions preserve the native truncation order;
out-of-signed32 conversions and Float storage overflow explicitly refuse.

The six-limb presentation clock now follows the original .4s damage flash,
1s healing and .5s refresh statement order, including retained applied tint when
the script would early-return. The bound-player rendering path ticks it per
visible frame over bounded10Hz real-state reads, uploads changed images only,
and resets presentation on player/map/unbind.357 offline checks pass. This is
not evidence that live possession or the full HUD is ready. A separate oxygen
presentation helper now preserves the original entry-tick100%, later actual
swimTimer/swimDuration arithmetic, exit/hide behavior and gate/reset rules;
1067 tests pass. The bound-player reader supplies actual HeadRegion.Zone water
state and swim fields; the Quest compositor consumes them without advancing
gameplay timers. Missing actual head zone stays unknown. Live underwater
gameplay/possession and device acceptance remain unproved. Original progress
bars now truncate through SetCurrentValue's Int conversion before height/color,
and low-O2 label flashing follows signed Float modulo and the strict.25 threshold.

Startup fixes now include the real PlayerNone ConsoleCommand guard, FMax,
sign-preserving Abs and signed integer Max/Clamp primitives. Audited native
argument buffers distinguish initialized defaults from SetTileColor's
uninitialized four-byte Color destination; absent or partial Color writes are
refused rather than filled from invented defaults. Tile setters copy each
initialized Float/Byte input separately; no global default seed is inferred
from an opcode. The VM passes694494 checks and778 refusal controls, including
1619 ConfigureChild input-copy/default controls,203 preferred-size OUT-copy/reference controls, SetSelectability/SetWidth and
5126 Tile input-copy/default checks. Byte alignment
inputs retain all256 values; partial packed-Bool clearing still refuses rather
than inventing a native memory layout.
Independent qualified instance storage also preserves
Actor.Region's PointRegion separately from GameReplicationInfo.Region's Int,
including save/load. Hidden inherited class-default writes remain unsupported;
instance support must not be presented as completed mutable-CDO shadow support.

NPC poses/animation, general gravity, breakable crates and lamp illumination
remain tutorial priorities. Further development waits for the user's next direction.
