# Player visuals, controls and recentering

This is a restricted first-person visual increment, not a complete animated
player pawn or a finished full-body rig.

## Original assets

CPU preparation reads the user's original `DeusEx.u`, `DeusExCharacters.u`
and `DeusExItems.u` on the existing startup worker. It verifies
`JCDentonMale.Mesh=DeusExCharacters.GM_Trench` and the original JC skin defaults.
The initial lower body comprises 147 original Still-pose trousers/shoes and
lower-coat triangles. Head, torso and upper arms are omitted to keep the
first-person camera clear; there is no invented skeletal rig or clipping mesh.
The wearer's lower-body renderer deliberately disables back-face culling: an
overhead self-view looks into the original mesh's open waist, where authored
one-sided trousers otherwise disappear. This is a VR self-view exception, not
a change to NPC/hand flags, geometry, alpha masking or depth occlusion.

The right hand/sleeve comprises 142 original `Glock` Still-pose triangles whose
material resolves to `DeusExItems.Skins.WeaponHandsTex`. No gun surface is
included. Its local origin is the original grasp material's vertex centroid;
it follows the actual right controller grip pose, not its pointing ray. A fixed
hand-local X rotation of -90 degrees maps the original upright grasp's
little-finger-to-thumb +Y direction to OpenXR grip -Z. Original textured CPU
axis views confirm the anatomical sign; shared tests check that basis and
centroid under independently yawed/pitched/rolled, translated controllers. The
left hand is an explicitly mirrored derivative of those same original surfaces,
with normals and triangle winding corrected. It is not an authored left-hand
asset. Both are rigid poses, not finger tracking, grip animation or arm IK.

The lower body follows the tracked head's horizontal position and yaw on the
active virtual floor. It is hidden below a 1.05 m relative head height to avoid a fixed standing
waist intersecting a low/seated camera. Walking/crouching animation, full torso,
arm IK, held weapons and a real script-controlled player pawn remain unfinished.
The new visuals do not add actors or alter inventory/checkpoints/saves.

## Seated height

Clicking the left thumbstick outside Persona switches seated/standing mode.
Seated mode explicitly calibrates once from the wearer's current tracked head
height to a 1.65 m VR comfort eye-height target, not an authored engine default.
For example, a 0.92 m physical height selects a virtual floor at -0.73 m.
Head/controller/eye poses are not fabricated or lifted; only the world's floor
translation and the avatar's feet use that virtual floor. Subsequent leaning
and crouching remain real head-height deltas, not continuously re-zeroed motion.
Sit upright when calibrating. `SEATED` explicitly recalibrates; `STANDING`
restores the real floor, through the existing developer mailbox.

The chosen mode is retained in the app-only `quest-posture.cfg` setting and
recalibrated after the next session's map and tracking have settled. Automatic
calibration requires focused, fully tracked head poses with no pending reference
change and 0.75 seconds within an 0.08 m head-height range after collision loading
finishes. Reference events, tracking loss, loading, invalid samples and frame
hitches reset the window. This gate only runs while calibration is pending;
ordinary leaning/crouching cannot recalibrate it. Explicit `SEATED` is immediate.
Ground probes, capsule
sample heights, body origin, fresh map spawns and map-local saved feet share the
same virtual floor. Mode changes translate the world by the floor delta before
ground probing, so the old ground cannot disappear outside the step-up range.
Both map-local restore paths use the current floor. Pre-posture legacy raw-pose
saves receive the current floor offset exactly once; their bytes/format are
unchanged. No quick-save was triggered to test this change.

Gravity-valid reference changes transform seated calibration alongside the map;
unknown/tilted reference changes request one fresh calibration on valid tracking.
Ordinary tracking loss does not continually erase seated calibration. Invalid,
non-finite or out-of-budget heights leave calibration unchanged. Standing mode
retains the real floor. Head-origin LOCAL fallback currently refuses seated
calibration, rather than guessing a floor.

Valid reference changes preserve map-local feet using the change in virtual
floor height. Standing keeps tracking floor zero, so a nonzero reference-origin
Y must not be subtracted from world Y; doing that could bury the feet beyond
the normal 0.45 m step-up limit. Seated floors still transform with the origin.
Ground and wall probes are disabled while a map's collision mesh is incomplete.
Normal movement retains the existing 0.45 m step-up / 2 m drop search; there is
no unrestricted upward teleport or fabricated ground. Original cache front and
reverse triangles remain paired, so collision extraction still advances by six
vertices. This mesh probe is not the original engine's full pawn physics.

The three original images are 128x128 (trousers), 128x256 (coat), and 256x256
(hands), not three equally sized layers. A bounded native-size upload plan is
fully validated before GL calls; each unchanged image uses a separate one-layer
array with its original UVs and bilinear sampling. No resizing/padding/texel
replication is used. These textures are uploaded once per XR session, kept
separate from map/actor renderer lifetimes, and freed before their shared shader. Mesh,
texture, triangle, dimension and retained-byte checks reject partial preparation;
failure leaves no substitute cubes or synthetic hands. Asset preparation cannot
run concurrently with the portable-runtime startup owner.

## Debug HUD and recenter

TinyUI labels are children of individual menu roots. Both the debug menu pose
and its auto-layout parent object now have identity poses; its child is placed
once, directly in front of the tracked head
at 1.05 m. Previously the fixed root offset `(0,1.3,-1.5)` was added to the
head-relative pose, causing an off-center position that varied with the user's
room location/orientation. Simply removing that menu translation did not remove
the SDK's separate room-fixed auto-layout parent offset. Persona artwork
positioning is unchanged. Left controller grip toggles the developer HUD outside
Persona, so the panel need not obstruct the body/world; it uses the same
0.7/0.35 engage/rearm rule as navigation. The existing developer mailbox also
accepts `HUD` through `tools/Send-QuestDiagnostic.ps1`.

The app requests `XR_EXT_local_floor` and selects the recenterable `LOCAL_FLOOR`
space when available. Eye matrices, controller poses and projection submission
all use that same space through the pinned SDK. Timed reference-change events
preserve horizontal map position but honor the new physical forward; compensating
the scene yaw would cancel the recenter. The system Meta button is not intercepted
or rebound by the game. See the [Khronos LOCAL_FLOOR contract](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR.html).

If the runtime does not support LOCAL_FLOOR, the existing STAGE fallback retains
room-locked continuity and logs that recenterable tracking is unavailable. This
fallback is not a promise that the Meta-button recenter works. If only head-origin
LOCAL is available, the lower body is explicitly hidden because its origin is
not a floor; controller hands remain available. Invalid tracking is not replaced
with a fabricated head/controller pose. Both position and orientation must be
valid for a hand and its aim; stale controller rotations are not drawn.

## Stick input

Left-stick movement has a radial 0.18 deadzone, remapping the remaining range to
full speed while preserving diagonal direction. Snap turns and Persona navigation
activate at 0.7 and rearm only after returning inside 0.35. Holding or jittering
around the activation threshold cannot trigger repeated turns/pages. Non-finite
inputs cannot move or rearm the player.

## Validation

`quest_vr_input_test` exercises synthetic input boundaries, drift suppression and
hysteresis. `vr_world_transform_test` covers off-origin/pitched HUD positioning,
map-position-preserving user recenter and the existing room-space continuity,
turn/collision/save mathematics. `quest_player_visual_test` tests bounded CPU
geometry/mirroring and, with an explicit original game root, verifies the actual
147/142/142 original surface selections and three decoded textures. The posture
test exercises 1,856 bounded synthetic calibration/recenter/settling checks;
transform tests also cover seated
save feet, floor-delta switching and recenter preservation. These are
host contracts; they do not prove physical Meta-button event delivery, controller
grip alignment, headset comfort or full body animation. Capture-time logs also
report actual submitted HUD-center/head-forward projections: the asymmetric
left-eye image's forward point is not necessarily its pixel midpoint. Actual
device captures and a physical recenter test are separate acceptance checks.

The first seated device test was rejected by the user (view too low / legs
missing). A requested downward Quest capture showed only the lower coat, and
an independent original-texture CPU preview reproduced missing legs with
correctly grounded feet solely from one-sided culling. A separate reference
test reproduces standing feet about 0.689 m below the original Training floor
after a vertical origin shift. Neither result proves that every missing-body
case shares the same cause. New capture diagnostics report the actual map,
world/head coordinates, floor beneath the camera, eye height and body surfaces
to distinguish grounding, culling and camera placement on the device.

The corrective device build's feet match the actual Training floor at -0.198 m.
Its first automatic calibration still ran during a startup reference change and
left only 0.290 m of virtual eye height. An explicit calibration restored 1.65 m;
the user then confirmed "Height and legs look correct", with an actual downward
capture showing textured hands, lower coat and previously culled trousers/boots.
The final startup settling gate addresses that premature calibration separately;
host gate checks do not by themselves prove its on-device cold-start behavior.

Final cold-start device check: Training upload completed at 04:54:21.968 and
automatic calibration completed at 04:54:22.745 (physical head 1.103 m, virtual
floor -0.547 m, target eye height 1.650 m), with no manual `SEATED` command.
The 04:54:49.775 downward capture has valid grips, two actually submitted body
surfaces, feet exactly on the -0.198 m Training floor, and 1.501 m actual eye
height after real head lowering. Both original textured hands and the coat/
trousers/boots render. This is one successful physical cold-start acceptance,
not certification of every runtime/recenter case or an animated full body.

Second cold-start acceptance at 08:14:32: automatic seated calibration used
physical head height 0.928 m and virtual floor -0.722 m, reaching 1.650 m with
no manual calibration command. The user confirmed "Height and legs look
correct" without pressing the left stick. The 08:15:05 capture reports both
tracked grips, two submitted lower-body surfaces, feet and the actual floor
at -0.198 m. Its later physical head height is 1.088 m (actual eye height
1.810 m), consistent with real movement after the one-time calibration. This
frame faces forward and therefore does not independently show the boots.
Two idle timing windows reached 72 fps; screenshot readback caused a frame
spike and is not a performance certification.
