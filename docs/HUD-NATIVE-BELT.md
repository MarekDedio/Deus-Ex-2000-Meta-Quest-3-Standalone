# Original belt container initialization

Source work only, 2026-10-15. The full working HUD remains an APK release gate.
Original Windows DLLs were inspected statically, never executed.

HUDObjectBelt.CreateSlots calls ordinary Window.NewChild1410 for both native
RadioBoxWindow and TileWindow, with omitted bShow=true. There is no separate
NewChildTileWindow native. Both original constructors now run before the later
script InitWindow/Show/unlock sequence.

## Radio and native ownership

Radio.Init10038660 calls TabGroup.Init10044d90, then Window.Init1004bb30.
Owned native pointer lists are represented as GC-traced/journaled vectors,
not serialized raw DynamicArray structs. TabGroup clears row/column lists,
sets windowType1, dirty tab order, sizing flags, index-1 and coordinates+0.
Non-Modal descendants register in their nearest actual Modal before Radio sets
bOneCheck=true, clears toggleButtons and currentSelection. The original belt
script subsequently sets bOneCheck=false.

Modal registration10037000 appends without duplicate suppression or a
visibility filter. Resort10036ed0 orders by Y, X, then process-local native
UObject identity, writing every group index. Portable pointer ordering uses
std::less, not a saved raw pointer or fabricated cross-run semantic key.
Finite-coordinate and4096-entry limits are explicit portable safety policies.
Constructor and list growth share aggregate retained-state budgets/rollback.

Radio.DescendantAdded10038b90 first invokes optional original script, then
appends Toggle descendants only when this is their nearest actual Radio.
Tile is not a Toggle, so its notification does not populate the toggle list.
Radio child visibility bypasses script and reconfigures on hide only.

## Tile and setters

Tile.Init10048360 calls Window.Init directly, not TabGroup.Init. Row storage
starts empty; its unaudited element layout is not represented as fake pointer
bytes. Actual row-layout behavior still needs implementation. Defaults are
orientation/directions0, margins3, spacings1, child alignments3, wrap/fill/equal
height true and equal width false. These fields have original Const flags2;
native stores are distinct from permission for arbitrary script writes.

Native1535 margins,1539 minor spacing and1540 major spacing use independent
initialized Float+0 input temporaries.1538 order uses a Byte0 temporary. Inputs
are copied at expression time; every setter always requests reconfiguration
on Self1004e060, with no clamps/equality shortcut. Hidden ancestors defer layout.
Byte order7 produces horizontal orientation, both directions1 and wrapping.
Out-of-enum Byte values12..255 follow retail case4, not an invented rejection.
Tile child visibility reconfigures on both show and hide, unlike Radio.

## Honest next boundary

The original pending graph now reaches CreateSlots PC206 after54 objects and
3739 writes, refusing native ToggleWindow construction. HUDObjectSlot inherits
Toggle→Button→Text→Window. Its real constructor must handle selectability tables,
accelerators and Button/Text defaults, not pretend to be a pure Window. Root.hud
and HUD.belt remain unassigned; the entire probe rolls back. Quick-slot input,
real starter ring, live ownership/activation, remaining windows and accepted
world/player startup are still required before another APK.

Verification: coherent host compilation and80 ordinary CTest passes out of93
registered/13 explicit owned-data skips, zero failures,30.08s. Separate owned
player/root/container/oxygen-source/ring/session controls pass2308 checks;
VM684788/684, oxygen presentation1067, shared HUD compositor21582 and original
artwork87 controls pass. Runtime/VM/Quest ARM64 syntax-only checks pass. The
untouched1308-actor world probe still refuses Move266 and fully rolls back.
Software art previews use explicitly detached fixture states. No Android
binary/APK was generated or installed, and no commercial assets were pushed.

## Audited next slot constructor, not yet implemented

Toggle.Init10049820 calls Button.Init10007640→Text.Init10045af0→Window.Init.
Window parent/descendant notifications happen before derived initialization,
so Radio appends the actual Toggle while its Text/Button fields are still
uninitialized. Toggle later clears enableSound/disableSound only.

Text sets alignments1/1, margins3/3, word wrap true, MinWidth0, min/max lines-1
and an empty private FString. EnableTextAsAccelerator100464e0(true) preserves an
already-true flag; otherwise sets it then always writes acceleratorKey0 through
1004f3b0. Only a changed old nonzero accelerator dirties the nearest Modal.
Text fields are original Const flags2; private Text is StrProperty flags1000.

Button clears pressed/mouse/auto-repeat bits while preserving right-click
enablement; delays are.3/.5/.1, key/timer/repeat time0, texture/sounds None.
Its six ButtonDisplayInfo records contain tX Texture and canonical Color
tlColor/txColor; tile RGB white, text RGB green. Current colors have white RGB.
Original Color alpha bytes are undefined stack storage: support needs an
explicit defined portable policy, not a claim that retail initializes RGBA.

Button calls SetSelectability(true), and original HUDObjectSlot.InitWindow
later calls native1420 SetSelectability(false). The temporary true phase must
really insert/sort row/column tables and indices; skipping it is not equivalent
to the original callbacks.1420 has one copied Bool parameter with native+0
default. Changed selectability checks root focus, updates nearest TabGroup lists,
and dirties accelerators only for an actual nonzero accelerator key.

Table sorting uses row Y/X/address and column X/Y/address. ComputeTabGroupLocation
10045000 scans positive-size traversable windows and uses clip origin+X/Y.
IsTraversable(false) still checks all ancestors' visibility/sensitivity; false
only bypasses the active-Modal constraint. The hidden HUD/root prefix has no
eligible entries and keeps firstAbsX/Y0. Nonnull focus, visible layout, Button
press/timers, Toggle selection and native virtual-event overrides still need
their own implementation/audit. Constructor evidence does not prove interaction.
