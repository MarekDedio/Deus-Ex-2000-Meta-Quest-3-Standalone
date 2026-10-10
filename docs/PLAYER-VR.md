# Player visuals, controls and recentering

This is a restricted first-person visual increment, not a complete animated
player pawn or a finished full-body rig.

## Original assets

CPU preparation reads the user's original `DeusEx.u`, `DeusExCharacters.u`
and `DeusExItems.u` on the existing startup worker. It verifies
`JCDentonMale.Mesh=DeusExCharacters.GM_Trench` and the original JC skin defaults.
The self-body comprises 251 original Still-pose surfaces: 127 trousers/shoes,
64 coat torso/back/shoulder, 27 chest/neck and 33 lower-coat/collar triangles.
The original coat UV atlas cleanly separates torso faces (all V<=112/255) from
static sleeves/arms (all V>=182/255). Whole original faces are selected; no
geometry is clipped or invented. Static hands/head/glasses and arms are omitted.
The neck/shoulder sockets remain open, so this is not a closed full-body mesh.
Unlike the earlier 147-face cut-off waist, this torso honors original material
back-face culling instead of exposing every garment interior.

Hands now use the more complete original `NanoKeyRingPOV` Still-pose hand and
connected sleeve, not the visibly open Glock weapon grasp. Only its 152 original
`WeaponHandsTex` faces are retained; no key-ring surface is included. Topology
validation requires 80 source vertices, one simple audited six-vertex rear cuff
and no extra holes, disconnected parts, inconsistent winding or nonmanifold
edges/vertices. Four explicitly derived convex-ear triangles close that cuff,
using exact source rim vertices and an original rear-cuff texture coordinate.
All original face UVs/materials remain unchanged. This is a bounded VR
derivative, not a claim that the original asset contained a cap.

The original source supplies the right-hand derivative without reflection
(`derivedMirrored=false`). Reflecting the prepared right hand across grip-local
X supplies the left variant (`derivedMirrored=true`), including normals, source
corner identities and winding. Both are explicitly adapted derivatives, not
separately authored assets. The earlier source-handedness classification was
wrong: its extra reflection reversed the wrist direction on the headset.

An audited palm/back landmark midpoint supplies the grip origin, independent
of the long sleeve. Original source landmarks define right grip +X into the palm and -Z
along the curled little-finger-to-index direction, with an orthogonal
right-handed +Y. The cuff extends along +Y, not -Y: for a neutral upright
controller the curled little-to-index direction points upward, making grip +Y
point back toward the wrist. The 21:38:09 capture confirms head-local grip +Y
Z=+0.9033/+0.9402 for right/left while the aim rays point forward. Preparation
converts vertices/normals into this grip-local
basis once. Rendering uses the actual controller grip pose directly, not its
aim ray or the obsolete Glock-specific -90-degree correction. Both remain rigid poses, not finger tracking,
grip animation or arm IK. Host geometry checks cannot establish physical grip
comfort; that requires controller-in-hand acceptance on the Quest. The latest
physical test rejected the earlier extra-reflection alignment: the fingertips
pointed toward the wearer. The corrected source mapping passed a subsequent
controller-in-hand direction test; see the latest device outcome below. Grip-axis
definitions follow the [Khronos OpenXR standard pose identifiers](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html#semantic-path-standard-pose-identifiers).

The body follows tracked horizontal head yaw on the active virtual floor, with
a render-only 0.16 m rearward clearance along that yaw's +Z axis. Multi-angle
original-texture CPU comparisons at 0/12/16 cm found that 16 cm clears the old
coat tunnel and reveals boots when looking down. The tracked eyes, calibrated
floor, collision and saved map position are not shifted. Near-vertical gaze
retains the last render-only heading to avoid a 180-degree projection flip;
reference changes rebase that heading. The body is hidden
below a 1.05 m relative head height to avoid a fixed standing waist intersecting
a low/crouched camera. Walking/crouching animation,
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

The four original images are 128x128 (trousers), 128x256 (coat), 64x128 (chest)
and 256x256 (hands), not equally sized layers. A bounded native-size upload plan is
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
body selection/import and, with an explicit original game root, verifies the actual
251 self-body faces, 152 original plus four derived faces per hand and four
decoded textures. `quest_vr_hand_geometry_test` separately tests closed topology,
source-face/UV retention, grip basis, mirror winding and bounded rejection.
`vr_world_transform_test` also checks horizontal body clearance at off-origin,
pitched, standing and seated poses. The posture
test exercises 1,856 bounded synthetic calibration/recenter/settling checks;
transform tests also cover seated
save feet, floor-delta switching and recenter preservation. These are
host contracts; they do not prove physical Meta-button event delivery, controller
grip alignment, headset comfort or full body animation. Capture-time logs also
report actual submitted HUD-center/head-forward projections: the asymmetric
left-eye image's forward point is not necessarily its pixel midpoint. Actual
device captures and a physical recenter test are separate acceptance checks.

Headless original-data diagnostics (write only to an ignored artifact directory):

```powershell
$hostBuild = 'artifacts\prerequisites-20261009\build'
$ownedGame = 'D:\Steam\steamapps\common\Deus Ex'
& "$hostBuild\quest_player_visual_test.exe" --game-root $ownedGame
& "$hostBuild\quest_player_visual_test.exe" --preview-body $ownedGame artifacts\player-body-preview
& "$hostBuild\quest_vr_hand_geometry_test.exe" --game-root $ownedGame
```

### Latest device outcome: corrected hand direction accepted

The corrected archived APK
`artifacts/release-hand-direction-20261010/DeusExQuestVR-hand-direction-20261010.apk`
(SHA256 `D66572A71FE41751452908ED428B959CB215E65B4727F7CAD458770701899A82`)
was replace-installed; the actual on-device APK hash matches. The user tested
normal holding and palm-up/palm-down rotation and confirmed "Yes, directions
match". This accepts the corrected hand direction in that physical test, not
finger animation, IK, every grip or a complete original player pawn.

The tracked 21:43:27 screenshot shows sleeves extending back toward the wearer,
not away. Both grips are tracked, eye height is1.650m, floor is-0.198m and four
body surfaces are submitted. Body geometry and height calibration were unchanged
from the accepted body test. Evidence:
`artifacts/player-geometry-20261010/quest-hands-direction-corrected.bmp` and its log.

### Previous geometry build: body accepted, hand alignment rejected

The archived geometry APK
`artifacts/release-player-geometry-20261010/DeusExQuestVR-player-geometry-20261010.apk`
(SHA256 `BA769234B1CF818F5CDFA26DB0CFD7D4C14B7568AC4C3B8A4A0A0D13F591D924`)
was installed and subsequently tested with valid head and controller tracking.
The user reported "body is fine, but hands are in a completely wrong direction"
and clarified that the fingertips point toward them. Thus the body is accepted
for this reported test case, while that build's hand alignment was **rejected**.
The corrected build above supersedes that rejected mapping. This is not
overall acceptance of the new geometry, every body/view angle, or a finished
animated player.

The 21:30:46 capture records both tracked grips and 1.680 m actual eye height
after real movement; the session's seated calibration had targeted 1.650 m.
Feet match the actual Training floor at -0.198 m, with four submitted body surfaces.
The recorded frame and metadata are
`artifacts/player-geometry-20261010/quest-hands-orientation-rejected.bmp` and its
matching log; successful rendering/grounding does not override the physical
hand-alignment rejection.

The same APK also survived one Home-to-return lifecycle cycle in process 15719.
At 21:29:37 the old session reached STOPPING, completed `xrEndSession`, and
entered IDLE. Returning at 21:29:43 created a new activity/native thread 16470
in the same process, and its new session reached READY and FOCUSED without an
observed assertion or process crash. However, `APP_CMD_RESUME` at 21:29:43.253
preceded READY at 21:29:43.267: this cycle did **not** reproduce the narrow
READY-before-Android-resume queue ordering that triggered the earlier SDK
assertion. It is one successful lifecycle regression check, not proof that all
focus races are fixed. Evidence: `artifacts/player-geometry-20261010/quest-focus-cycle.log`.

A subsequent quaternion-logging diagnostic APK
`artifacts/player-geometry-20261010/DeusExQuestVR-hand-pose-diagnostic.apk`
(SHA256 `5F87839DD178FF1AF7166289DB7C58A03174D65FBCAD059DE259D39ED50F0935`)
was installed to investigate the hand direction. Installation is not a new
physical acceptance or evidence that hand alignment has been corrected.

### Earlier seated-height acceptances

The following device acceptances describe the earlier cut-off-waist/Glock
builds. They prove their recorded calibration/grounding cases, not acceptance
of the new torso/closed-hand geometry above.

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
