# Original object-focus brackets

The focus frame uses the original PC game's `DeusEx.FrobDisplayWindow`
style. This is procedural UI, not a missing picture: the compiled original
`DrawWindow` draws one-pixel `Extension.Solid` strips at the four corners,
first a black shadow at `(1,1)`, then the HUD header color. The default
`ColorThemeHUD_Default` header color is white (255,255,255).

The shared `quest_targeting_frame.h` preserves the original corner formula
`clamp((width + height) * 0.1, 4, 40)` and moving inset
`int(24 * (time % 0.3))` on the 640×480 reference canvas, including the
70-pixel edge margin and small-target expansion. No commercial texture has
been copied into the repository or APK for the frame.

Quest projects the selected object's authored collision bounds, or a mover's
current brush bounds, into a central-camera rectangle. The strips form one
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
The complete original lock-strength, door
strength, tool-count and object-information panel is not implemented here.

## Verification

`targeting_frame_test` covers projection, original strip positions and
shadow offset, inset timing, margins, tiny targets, behind-eye and malformed
bounds refusals, and the door ray-bounds broad phase. The read-only
`targeting_frame_audit GAME_ROOT` prints original class-default evidence;
it does not execute the original Windows game.

Physical headset acceptance is separate from those host checks:

- Aim the right controller at a closed Training door; the white brackets
  should surround that visible panel, and A should operate the same panel.
- Keep aiming while a door moves; the frame must follow its current brush,
  not remain at the original pivot.
- Look and move sideways while targeting an object: the frame must remain
  around the object in both eyes, without a debug-panel offset.
- Aim at an item, NPC, hidden trigger and an NPC behind a solid wall. Only
  the visible selected highlighted target should receive a frame.
- Open inventory or switch maps: no stale world-target frame may remain.

These on-device visual/performance checks remain required before treating
this implementation as accepted VR presentation.
