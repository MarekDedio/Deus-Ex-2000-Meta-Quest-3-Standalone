# Original HUD artwork compositor

`native/quest_hud_ui.h` is shared, bounded CPU presentation code for the original
object belt, body health and compass. It accepts detached typed snapshots. It
does not spawn a player/ring, invent ownership or equipment, apply damage,
allocate belt positions, collect keys, or alter saves. Unknown limb/energy
values remain absent, and empty belt positions remain empty.

## Original geometry and palette

- Object belt: 541x69; positions 1..9 followed by special keyring position0.
  Ordinary child slots are 51x54 at (10+51*(position-1),6); special slot0 is
  44x54 at (469,6). Every icon crops its original source42x37 at child (1,3).
  `FontTiny` renders numbers in the right-aligned six-pixel area at (37,3),
  centered descriptions at (1,42,42,7), and item counts/ammo at (1,32,42,8).
  Real selection uses the original PersonaItemHighlight nine-slice at
  (0,2,44,50); pointer/drag/equipment state is supplied, never inferred.
- Health: 84x106; background crops60x76 at (11,11), border crops84x106.
  Neutral body crops34x68 at (24,15), lowered behind independent head/torso/
  left-arm/right-arm/left-leg/right-leg children. Their actual rectangles are
  (39,17,4,7), (36,25,10,23), (46,27,10,23), (26,27,10,23),
  (41,44,8,36), (33,44,8,36). Body/parts use original UE Translucent screen
  blending (source + destination*(1-source)), so their black pixels do not
  make black rectangles. Generic masked draws preserve legitimate black.
  Independent scaled colors match `Extension.Window.GetColorScaled` at
  thresholds0/0.1/0.4/0.8; an explicit damage-flash fraction approaches white.
  Energy/breath bars are5x55 at (15,20)/(61,20), with original `BE`/`O2`
  labels and low-breath blink. The snapshot carries original presentation's
  displayed limb values; original healing interpolation/timer advancement
  belongs to the runtime/presentation state owner, not the rasterizer.
- Compass: 73x40; background/tick-box crop60x19 at (11,6). The55x22 child clip
  starts at (13,7); each scrolling strip is logical240x15 despite the actual
  256x32 texture padding. The two windows follow original binary32 arithmetic
  and float-to-int truncation: `int(27 - ((yaw-mapNorth)&65535)/(65536.0f/240))`.
  For example yaw8192 yields-2, not an exact-real/floor result of-3. Subtraction
  wraps safely, including signed host extremes. TrueNorth and yaw are inputs;
  the compositor does not fabricate a Pawn.Rotation or map north.

The explicit Amber reference palette is source-derived: Background139,105,35;
Borders185,177,140; NormalText202,158,73. HeaderText255,233,177 is a different
theme entry and must not replace NormalText. Compass tick color is the original
HUDCompassDisplay default200,200,200. Original serialized Color.A=0 is palette
metadata, not transparent pixel coverage. This palette is a presentation
choice matching the human's PC references, not a claim that a live
ThemeManager selected Amber. The canvas's background/border use masked
coverage; live configurable world-background/border translucency requires the
renderer/window integration to preserve the relevant draw styles, rather than
claiming a flattened RGBA image represents every original graphics option.

## Evidence and tests

Read-only original package/script inspection uses `System/DeusEx.u`,
`System/Extension.u` and `System/DeusExUI.u`; no original Windows EXE/DLL is run.
`quest_hud_ui_original_test GAME_ROOT [artifacts/.../preview.bmp]` validates
authored art identities, slot dimensions, theme name-to-color mappings and
the NanoKeyRing's actual Icon/beltDescription. It decodes FontTiny and the
original UI textures from an external owned game installation, verifies the
source files are unchanged, and optionally writes full640x480 and per-panel
BMP previews only under ignored `artifacts`. Healthy/damaged/unknown limb
values and the starting ring are explicitly **fixtures, not live gameplay**.

`quest_hud_ui_test` uses generated noncommercial sources to cover wrap/clip/
padding, ten-slot mapping, special-slot/identity/duplicate refusals, icons,
selection, text, six independent limb colors, original blend styles, energy/
breath bounds and invalid input. The header also exposes compass-only
`BuildHudCompassCanvas`, allowing genuine map-relative rendered heading to be
integrated without drawing a pretend healthy player or starting inventory.

Remaining runtime acceptance is recorded separately in
`docs/ORIGINAL-HUD-CONTRACT.md`: actual player startup, real owned NanoKeyRing,
inventory chain/activation/key collection, six-limb gameplay, HUD animation,
save restoration and live VR controller/recenter/turn tests are not proved by
these software-artwork tests.
