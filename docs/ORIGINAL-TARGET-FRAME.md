# Original object-focus brackets

The focus frame uses the original PC game's `DeusEx.FrobDisplayWindow`
style. This is procedural UI, not a missing picture: the compiled original
`DrawWindow` draws one-pixel `Extension.Solid` strips at the four corners,
first a black shadow at `(1,1)`, then the HUD header color. The default
`ColorThemeHUD_Default` header color is white (255,255,255). The supplied PC
references use a golden presentation, so this port explicitly selects the
original `ColorThemeHUD_Amber` palette: header (255,233,177), border
(185,177,140), background (139,105,35). These values are decoded from the
user's package, not substituted pixel estimates. This is a chosen original
presentation palette, not evidence that the missing original player/theme
bootstrap has selected that theme. A missing Amber export uses decoded Default.

The shared `quest_targeting_frame.h` preserves the original corner formula
`clamp((width + height) * 0.1, 4, 40)` and moving inset
`int(24 * (time % 0.3))` on the 640×480 reference canvas, including the
70-pixel edge margin and small-target expansion. No commercial texture has
been copied into the repository or APK for the frame.

The original `DrawWindow` intentionally does **not** outline a whole door.
For movers it obtains the current brush center, then replaces all three
half-extents with 16 Unreal units (bytecode PC302..349). For other actors it
projects authored CollisionRadius/CollisionHeight and then insets each screen
edge by the corresponding Unreal-unit value divided by four, in canvas pixels
(PC858..969). The former full-brush outline and missing screen inset were bugs.
Quest now applies these source rules before the original clamp/expansion.
The resulting strips form one
world-space billboard at the object's depth, so both eyes receive stereo
parallax. They are separate from the blue debug panel. Rendering batches all
16 strips in one immutable mesh and changes only small uniforms while aiming.
All strips are packed into four matrix uniforms: the pinned SDK honors matrix
array counts but uploads only one vector for a vec4-array parameter. The host
regression simulates the SDK transpose and checks all 64 packed components.

The frame and the A-button share one 3-metre controller-ray selection.
Collision sizes, `bHighlight`, and mover `bFrobbable` are cached during actor
publication. Hidden actors and non-frobbable doors are excluded. Nearby BSP
and live moving brushes occlude targets behind walls. Non-mover selection
uses a collision AABB with a small minimum controller-aim extent, not the PC
engine's exact cylinder trace. A ray starting inside that bound remains usable;
it must not lose its frame merely because its entry distance is zero.
The focus label now uses the original `FontMenuSmall_DS` atlas and
`ConWindowBackground`, native glyph metrics, four-pixel text padding and
`infoX/infoY=boxTL+10`. Assets/messages/palette are decoded on the startup
worker. Name/lock fields use fully qualified original property identities,
including the original FamiliarName/UnfamiliarName/LastConEndTime rule and
Inventory/Decoration ItemName. Missing fields never become a technical actor
name or a guessed Unlocked state. Original lock/door-strength text is available;
strength bars, tool counts and HackableDevices information remain unfinished.
The detached name/state adapter does not execute the missing player/UI scripts.
Descriptions are cached and mutable changes sampled at most ten times a second;
the small retained label texture is uploaded only when displayed text changes.

## Verification

`targeting_frame_test` covers projection, original strip positions and
shadow offset, inset timing, margins, tiny targets, behind-eye and malformed
bounds refusals, original mover proxy/non-mover screen insets, declared-name
selection, missing-state refusal, authored RGB glyph tint and bounded label
canvas, and the door ray-bounds broad phase. The read-only
`targeting_frame_audit GAME_ROOT` prints original class-default evidence;
it does not execute the original Windows game.

Physical headset acceptance is separate from those host checks:

- Aim the right controller at a closed Training door; the golden brackets
  should form the original small focus box at its center, with its actual
  Unlocked/Locked text, and A should operate that same panel.
- Keep aiming while a door moves; the frame must follow its current brush,
  not remain at the original pivot.
- Look and move sideways while targeting an object: the frame must remain
  centered on the focus object in both eyes, without a debug-panel offset.
- Aim at the desk lamp and items: authored display names, not export/class
  identifiers, should appear in the original small bordered information panel.
- Aim at an item, NPC, hidden trigger and an NPC behind a solid wall. Only
  the visible selected highlighted target should receive a frame.
- Open inventory or switch maps: no stale world-target frame may remain.

These on-device visual/performance checks remain required before treating
this implementation as accepted VR presentation.
