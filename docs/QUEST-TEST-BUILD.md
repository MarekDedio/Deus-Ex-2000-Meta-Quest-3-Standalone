# Current Quest test-build handoff (2026-10-11)

Host-only decal geometry/state work dated 2026-10-12 is documented in
[DECAL-GEOMETRY-PLAN.md](DECAL-GEOMETRY-PLAN.md). It has not created a replacement
APK or changed the installed renderer; the signed APK recorded below remains
the latest build. Connected-Quest default auto-install remains enabled for
every actual new APK, preserving data/saves.

## Latest: Spawn tag fidelity and shadow prerequisites

`artifacts/decal-prerequisites-20261011/DeusExQuestVR-decal-prerequisites-20261011.apk`
is the newest signed ARM64 build: 18,919,145 bytes, SHA256
`F0E78EA681C70E0DF509375D0DC2B746CCCBAE7C337A01295A508604887EF70B`.
The default build helper automatically replace-installed it on connected Quest 3
`2G0YC5ZG620985`, without clearing app data/saves. On-device `base.apk` SHA256
matches; device lastUpdateTime is `2026-10-11 21:32:51`. One waited launch
confirmed process28821. APK signature v2 passes; its 16 ZIP entries contain SDK
assets/libraries and no original commercial packages.

The shared runtime worker now writes native Class.Name before Spawn callbacks,
then captured optional SpawnTag after native return, including None and
notification replacements. VM inputs are captured before later argument
expressions can mutate their sources. The updated data-probe library is
5,834,328 bytes, SHA256
`BEC3A9C6793F1D7CE66410B869BFE3F1E6203ED9D6290395D016F07C17E775EE`.
The Quest renderer library is byte-identical to the prior build: 5,360,240 bytes,
SHA256 `7461DF739F8E661097B675A024C8EF6DE9B54EB6DB2E3059104D864D270A331C`.
Accepted hand/body geometry, controller mapping and recentering are unchanged.

New host-tested original BSP point-trace code and its static original-decal
audit are prerequisites, not enabled NPC shadows. Native AttachDecal remains
explicitly unsupported; the rollback-only original Training startup probe still
reports the same actual PreBeginPlay slot756/JaimeReyes0 first failure and exact
save/actor/revision/GC rollback. See [MODEL-POINT-TRACE.md](MODEL-POINT-TRACE.md)
and [DECAL-NATIVE-CONTRACT.md](DECAL-NATIVE-CONTRACT.md). No original world/player
startup, possession or Tick is accepted by this batch.

Fresh installed-build captures are `quest-final-frame.bmp` and
`quest-final-inventory.bmp` under `artifacts/decal-prerequisites-20261011/`.
They show textured Training, differentiated lighting, readable debug HUD and
Persona inventory artwork/text. Both grips and head pose are reported tracked;
actual eye height is 1.650m, with four body surfaces submitted. The tilted
forward/upward pose has overlapping hands near the face; there was no controlled
physical grip/leg test or new user alignment acceptance. The inventory was
opened/closed by the diagnostic bridge and left closed, not physically tested.
Uncaptured samples reach 72fps; captures still cause 291.64ms worst-frame stalls.
These are smoke captures, not full visual or performance acceptance.

Ordinary CTest: 52 passed, zero failures, four owned-data skips (56 registered).
Point-trace controls pass 943/50 expected refusals, including all 256 flag bytes,
threshold neighbors, ordered first-node hits, native float boundaries, graph
validation/budgets and an 8,192-node iterative chain. Spawn/world controls
pass 5,598/116. The later-argument Tag alias control was first reproduced failing
before Native278 input snapshotting, then passed with the same control. Original
Training rays verify node1848/surface1837, near-floor clamping and a raised
half-unit backoff without modifying runtime/save state. Auto-install controls
pass 21 without hardware; the real connected-headset installation is separately
verified above. Remaining original-data suite results are recorded in STATUS.

## Previous backend development APK

`artifacts/world-startup-probe-20261011/DeusExQuestVR-world-startup-probe-final-20261011.apk`
is the newest signed ARM64 build: 18,919,145 bytes, SHA256
`C0085B58B56F86AF24D647384CB64981E0CE361D671841323C1220A8C63E7927`.
It adds shared-budget VM transactions, a rollback-only original-world startup
diagnostic, native Level ownership assertions and corrected BSP Region side,
leaf and zone fallback behavior. No accepted hand/body
geometry or controller/recenter mapping was changed. Signature v2 passes and
the16 ZIP entries contain SDK assets/libraries, not original game packages.

Data-probe: 5,833,752 bytes, SHA256
`D7EFDC0F635B4106C28222132A524F11327F4804C9824B6B40BEAF8685EEB20E`.
Quest library: 5,360,240 bytes, SHA256
`7461DF739F8E661097B675A024C8EF6DE9B54EB6DB2E3059104D864D270A331C`.

Earlier APK builds explicitly skipped default deployment while the Quest was
absent. During the final build Quest3 `2G0YC5ZG620985` reconnected: the default
build helper automatically replace-installed this exact APK, preserving app
data/saves. Device `base.apk` SHA256 matches the archive; device-reported
lastUpdateTime is `2026-10-11 21:00:43`. A single waited launch confirms
process25160. Installation/process startup alone is not visual acceptance.

Fresh in-app eye captures from this installed APK are
`artifacts/world-startup-probe-20261011/quest-release-frame.bmp` (21:03:02)
and `quest-release-inventory.bmp` (21:04:02). They show textured Training
architecture, differentiated lighting, readable debug text and the original
Persona inventory artwork/text. The inventory was opened/closed with the
diagnostic bridge, not tested with physical controller input. Tracking is
valid, seated actual eye height1.650m and the scene capture submitted four body
surfaces; neither controller grip is tracked, and neither view looks down at
the body. No renewed hand/leg alignment or comfort acceptance is claimed.
Uncaptured samples reached72fps; each capture caused a291.64ms worst-frame
stall. Menu was closed again afterwards; no save/load, pickup or map change
was requested. These two frames are smoke evidence, not complete visual QA.

All51 ordinary host tests pass (55 registered; four owned-data suites skip
without arguments). The four suites were separately run against the read-only
owned GOTY installation: original defaults1,002, original PawnList67, runtime
save/restore and the full Training/UNATCOIsland/Intro actor suite pass. Generated
Spawn/world integration passes5,398 checks/114 expected refusals; shared VM
transaction controls pass125/31 and total VM677,809/302. Automatic deployment
controls pass21/0 without hardware. The event-location regression was first
reproduced failing before its fix, then passed with the same generated control.

The separate unmodified Training startup probe creates GameInfo/Mutator and
reaches PreBeginPlay slot756, JaimeReyes0, failing in Engine.Decal.AttachDecal
native0. Exact repeated failure and byte-identical world/save/GC rollback pass
again after the final VM change. This diagnostic intentionally never publishes
a partial world; it is not full world startup, player ownership or Tick. See
[WORLD-STARTUP-PROBE.md](WORLD-STARTUP-PROBE.md).

Earlier hardware evidence belongs to the older
`original-state-control-20261011` APK, SHA256
`A17B145B1D4C29D8E6D0F27FB1A46D721904738F09ED2F14B980456A91943112`:
Training architecture, textures, differentiated lighting, readable debug HUD
and accepted hands were captured. A second seated frame reported eye1.694m
and four body surfaces; a forward frame does not prove leg/pose alignment.
Uncaptured Training samples reached72fps; screenshots themselves caused stalls.
Neither those frames nor this installable build prove campaign startup,
player ownership, live NPC scheduling or original player/root-window UI.

## Previous user-accepted hand-direction build

The previous **user-accepted hand-direction APK** corrected the backwards hands,
retains the accepted self-body geometry and includes the narrow SDK
session-lifecycle queue-order patch:
`artifacts/release-hand-direction-20261010/DeusExQuestVR-hand-direction-20261010.apk`.
It is 18,919,145 bytes, SHA256
`D66572A71FE41751452908ED428B959CB215E65B4727F7CAD458770701899A82`.
Android ARM64 compilation and v2 signature verification pass; its 16 ZIP entries
contain no original commercial packages. Data-probe is 5,635,696 bytes, SHA256
`5935D2827575D455568B647A2DA3AA4AB20C899A73016C9C93FA1383C24B0B5D`;
Quest library is 5,360,240 bytes, SHA256
`397DE1651B720E4B68E145E8E503918576EC0B70D3922BB5C68B4DEA585662D4`.

This exact archive was installed with `adb install -r` on authorized Quest3
`2G0YC5ZG620985`; on-device `base.apk` SHA256 matches. LastUpdateTime is
`2026-10-10 21:42:30`. One waited launch confirmed process18165. Replace-install
preserves app data; no manual save/load or data reset was requested. After a
fresh tracked capture, the user confirmed "Yes, directions match" while checking
normal holding and palm-up/palm-down rotation. This accepts hand direction for
that test case, not finger tracking, IK or all possible controller grips.

The new self-view has 251 original torso/coat/chest/leg faces and 152 original
plus four explicitly derived closed rear-cuff faces per hand. The original right
source is prepared directly in grip space; only the left hand is mirrored. The
previous extra source reflection put cuffs along -Y, away from the wearer.
Actual neutral-controller telemetry demonstrates +Y toward the wearer, and
the corrected mapping retains the proper palm/index axes with cuff +Y. A 16 cm
render-only rear body offset clears the camera; near-vertical body yaw is stable,
without changing eyes, floor, movement or saves. Native dimensions/UVs for four
original textures are retained. These remain rigid visuals, not finger/arm IK
or a possessed original player. See [PLAYER-VR.md](PLAYER-VR.md).

All50 ordinary host tests pass; three optional original-data integrations skip
without a game-root argument and were not rerun in this visual-only batch.
Separate original player-asset proof passes1724 checks/25 expected rejections;
hand proof passes147/30 synthetic and1705/34 original-data+preview controls,
including both captured controller rotations, rejection of the previous wrist
reversal, closed cuff depth/culling and all six views per hand. Shared transform
tests pass17 groups. Offline launch/install30 and fresh-patch4 tests pass on both
PowerShell versions. CPU checks do not establish Quest stereo comfort/input.
The Persona outer rails were independently matched to original border texels;
they are authored decoration, not cropped padding garbage.

Full campaign startup, original player ownership and automatic actor simulation
remain unfinished. The landing/probe backend below is retained unchanged in
scope; save codec8/envelope11 is unchanged. Device capture/physical acceptance of
this corrected hand mapping is recorded in the 21:43:27 frame and the user's
physical acceptance. Both grips were tracked; actual eye height was1.650m,
Training floor -0.198m and four body surfaces were submitted. Evidence is under
`artifacts/player-geometry-20261010/` and the release archive directory.

The previous geometry APK (SHA256
`BA769234B1CF818F5CDFA26DB0CFD7D4C14B7568AC4C3B8A4A0A0D13F591D924`)
received body acceptance but failed hand alignment: fingertips pointed toward
the wearer. A logging-only diagnostic (SHA256
`5F87839DD178FF1AF7166289DB7C58A03174D65FBCAD059DE259D39ED50F0935`)
measured neutral grip +Y at head-local Z=+0.9033/+0.9402 while aim rays pointed
forward. One earlier Home-return cycle survived without assertion; it did not
reproduce READY-before-Android-resume ordering. See [PLAYER-VR.md](PLAYER-VR.md)
for exact rejected-build and lifecycle evidence.

## Previous installed landing/probe build

The preceding **installed development APK** adds original landing waits,
nonprobe event eligibility and the corrected positive authored probe masks:
`artifacts/release-landing-probes-20261010/DeusExQuestVR-landing-probes-20261010.apk`.
It is 18,681,297 bytes, SHA256
`649B0B5AF1C18287F0A7D2A09AD389AE1D019DF7C675F99EAF9CF4AF3FFF317A`.
Android ARM64 compilation and v2 signature verification pass. Its 16 ZIP entries
contain no original commercial packages. Packaged data-probe is 5,635,696 bytes,
SHA256 `3DCA4B6CA765A93887A0B51C9AC24D941DD8858FB1A46A3B6F1968B77434C5A8`;
quest library is 5,122,392 bytes, SHA256
`1B864FB1D9B097228D9165A0AF9F1A8AC2C8FE80B8830EDDE5C6BD865CD8DEED`.
Original StartUp now reaches Standing Sleep304; real LongFall reaches
FallingState Sleep20. This is not full startup, actor ticking or physical landing.
See [SCRIPT-LANDING.md](SCRIPT-LANDING.md).
The existing save codec8/envelope11 remains unchanged.

At the user's request this exact APK was installed with `adb install -r` on
Quest3 serial `2G0YC5ZG620985`, preserving app data. On-device `sha256sum` of
`base.apk` matches the release hash above. Device-reported lastUpdateTime is
`2026-10-10 20:31:32`; the local receipt is
`artifacts/release-landing-probes-20261010/headset-install-receipt.log`.
The app launched and a fresh in-app eye capture confirms Training rendering
with valid head tracking. Automatic seated calibration reached 1.650 m; after
head movement the capture records 1.751 m and feet at the actual -0.198 m floor.
This upward view has no tracked controller grips and does not accept hand/body
alignment. The final-build frame is
`artifacts/landing-state-20261010/quest-landing-probes-final.bmp`.
Idle windows report 72 fps / 13.89 ms worst; screenshot readback causes a
291.65 ms worst-frame spike. These are scoped checks, not performance acceptance.
All 49 ordinary host
tests and all three separate original-data integrations pass on the corrected
source, including the full actor suite. State controls pass 473 / 58 rejections,
save-codec 6477 / 5731, dispatch 63562 / 20, generated lifecycle 1353 / 14 and
original defaults/lifecycle 998 checks. The actor run is
`artifacts/landing-state-20261010/original-actor-probes-verified.log`;
the other final logs have `-probes-final` suffixes.

Earlier this turn, the first landing APK
`artifacts/release-landing-state-20261010/DeusExQuestVR-landing-state-20261010.apk`
(SHA256 `A7D071DD238D806D655CB4F0D10BE1C9138ECF22A21A578A3256D05220BFD3FD`)
was installed and Training rendered with valid head/controller tracking.
The following visual measurements are from that earlier APK, not acceptance of
the final corrected positive-probe APK:
Automatic seated calibration reached1.650m; the downward capture records1.619m
after real head movement and feet on the actual -0.198m Training floor.
The inspected inventory capture has original page artwork and readable text,
but edge fragments need comparison with the original authored page decoration.
The downward capture shows textured
hands and lower body, including hand/finger spikes and an open waist viewed from
above; this is not complete character-model acceptance. Idle Training windows
report72fps/13.89ms worst, while screenshot readback causes277–292ms spikes.
No campaign, live-actor simulation or full performance acceptance is claimed.
Captures and logs are under `artifacts/landing-state-20261010/`.

The **previously installed** original-Sleep APK (source commit `f8abf2a`) is
`artifacts/release-sleep-state-20261010/DeusExQuestVR-sleep-state-20261010.apk`:
18,681,297 bytes, SHA-256
`587EB515CF7C51D1DC458BEE04D6E42925FCA6CE1BEBC0FECC76E8432B31A941`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass. Its
16 ZIP entries contain no commercial game packages. Packaged data-probe:
5,622,808 bytes, SHA-256
`A4D4BA670CCEDD6CD3C9270C2A7FF003299A98CF38D37277506CD7470801A548`;
quest library:5,122,392 bytes, SHA-256
`DB88F4D811D3D7B288D8B4EDD44A84F71D5838E587C3B67F0DEB9EDACC0D8789`.
It adds original Sleep256/StopWaiting0, explicit native wait polling, signed
timer retention and codec8/checkpoint11 saved continuation. StartUp now yields
at Sleep instead of refusing it; waking reaches unsupported WaitForLanding527
at PC18. This is not complete startup, automatic actor ticking, live AI or
new device acceptance. At the user's request this APK was installed with
`adb install -r` on Quest3 serial `2G0YC5ZG620985`, preserving app data.
On-device `sha256sum` of `base.apk` matches the release hash above;
`dumpsys package` reports `lastUpdateTime=2026-10-10 07:56:10`.
This replaces the older physically accepted seated-player APK `586cb78`;
no new headset tracking, visual or performance acceptance is claimed for `f8abf2a`.
See [SCRIPT-SLEEP.md](SCRIPT-SLEEP.md).
All49 ordinary host controls pass; state controls pass296 /40 rejections and
save-codec controls6467 /5731. All three separate original-data integrations
pass, including the full actor suite's Sleep/RNG cold restart and actual
StartUp inventory/wake rollback. This coverage uses isolated generated inventory
slots where needed, not a claim that Doctor1 has authored weapon inventory.

The preceding **backend-only, not installed** original-RNG APK is
`artifacts/release-random-state-20261010/DeusExQuestVR-random-state-20261010.apk`:
18,681,297 bytes, SHA-256
`3DA281F523885D6AE25D9BCF14682527B78080EE573EF461E9381816D49293E1`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. Packaged data-probe:
5,595,032 bytes, SHA-256
`5039FE7EEA0DF45ED8376F816234410E1DC63ECC21566E564CA30AAF00785BDB`;
quest library:5,122,392 bytes, SHA-256
`F15E9D185D91E79C47C81E82031D4E4C69DF709CD85DBAB0239BA221AF43748B`.
It adds original Rand/FRand, actual scripted RandRange/Bird branches and codec7 /
checkpoint10 continuation, including pre-first-draw saves and global map carry.
In that batch StartUp refused unsupported Sleep256 at PC6 with full rollback;
the subsequent original-Sleep increment above implements it.
This is not automatic startup, live NPC AI or new device acceptance. The installed,
physically accepted seated-player APK remains unchanged. See
[SCRIPT-RANDOM.md](SCRIPT-RANDOM.md).
All 49 ordinary host tests and all three separate original-data integrations pass.
RNG controls pass 58,942 / 37 rejections; codec controls pass 6,337 / 5,634.

The preceding **backend-only, not installed** Switch/Case APK is
`artifacts/release-switch-case-20261010/DeusExQuestVR-switch-case-20261010.apk`:
18,681,209 bytes, SHA-256
`AEB64340ED1C88BFD549D2363615B555D8ACE8B35DC2B385E813799CD0AB1465`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. The data-probe
library is 5,588,496 bytes, SHA-256
`3F6A3CDC6E8E151D78F71227E2855BA0A5807A92F81FBF19F29683D38AB5E36E`.
It adds original Switch/Case control, live state callback/alias safety and
transactional fallthrough. All 8,208 original functions pass structural
inspection; the original DeusEx head-turn override executes all directions.
In that increment StartUp reached unsupported FRand195 at PC9 and rolled back fully;
the later original-RNG increment above implements it.
This is not automatic world startup, working NPC AI or a new device acceptance.
The installed, physically accepted seated-player APK remains unchanged.
All 48 ordinary host tests and all three separate original-data integrations
pass on the final production source. State controls pass 208 / 32 rejections;
Switch-specific synchronous controls pass 420 / 58.
See [SWITCH-CASE.md](SWITCH-CASE.md) for scoped tests and evidence.

The preceding **backend-only, not installed** blend-animation APK is
`artifacts/release-blend-animation-20261010/DeusExQuestVR-blend-animation-20261010.apk`:
18,681,209 bytes, SHA-256
`E9500D83B97621DF0AE55D1BF02DAD1F3D4DA28FF34EFBFD585E66952FA9E8EA`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. The data-probe
library is 5,579,312 bytes, SHA-256
`683A63DA9264AF546A912300608EEF998745A385E8EFF4E3F554032C38D59B96`.
It adds original PlayBlendAnim1010/TweenBlendAnim1012 commands, corrected tween
frames/Plane packing, bounded older-clock restoration and complete cold mesh
material caching. Offline native-command Doctor/Jaime captures were inspected
with zero missing material selections; this is not new Quest GPU evidence.
It does not enable live animation ticking or finish original world startup. The installed,
physically accepted seated-player APK remains unchanged; this increment has
no new headset performance/visual acceptance. See [BLEND-ANIMATION.md](BLEND-ANIMATION.md).
All 48 ordinary host tests and all three separate original-data integrations
pass; 200 pure clock controls and the exact legacy import budget boundary pass.

The preceding **backend-only, not installed** native AI event-state APK is
`artifacts/release-ai-event-state-20261010/DeusExQuestVR-ai-event-state-20261010.apk`:
18,681,209 bytes, SHA-256
`2419A01F160BDEBB1A518862FBD3ABED5EF4EC0A61942E26A900E9A72132916B`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass; its
16 ZIP entries contain no original commercial game packages. The packaged
native libraries differ from the lookup build (data-probe 5,569,672 bytes,
SHA-256 `34004A11B3CC0C0D49EA8E207447346B275BBC3B0FCDF0402DC7E38CEF7AFF78`).
It adds manager initialization/registration/emission state, sensory histories,
transactional GC ownership and codec6/checkpoint9 snapshots, not AI processing
or a finished campaign. The 48 ordinary host tests pass (three optional original
data tests are verified separately). This batch is not device-tested or installed
over the accepted seated-player APK. See [AI-EVENT-STATE.md](AI-EVENT-STATE.md).

The preceding **backend-only, not installed** actor-lookup APK is
`artifacts/release-actor-lookup-20261009/DeusExQuestVR-actor-lookup-20261009.apk`:
18,681,209 bytes, SHA-256
`F812793EA19300A794EE8901623068C9BA69370C6FAC1302A7EED1EA92637DD7`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass;
its 16 ZIP entries contain no original commercial game packages. Packaged port
native-library hashes differ from the preceding spawn build. It adds bounded
synchronous foreach/AllActors304, linked-PlayerPawn fallback720, vector221/225
and valid inactive-reference assignments. All 47 ordinary host tests and all
three separate original-data integrations pass. Persistent iterator/AI/player
possession/world-startup work remains unfinished. This backend batch has not
been device-verified or installed over the accepted seated-player APK below.
See [ACTOR-LOOKUP.md](ACTOR-LOOKUP.md).

The preceding **backend-only, not installed** actor-spawn APK is
`artifacts/release-actor-spawn-20261009/DeusExQuestVR-actor-spawn-20261009.apk`:
18,681,209 bytes, SHA-256
`7B24F692B5F8D10FC33D10DF38944898A2D0ADAB0B36FA1CD48701599252ED67`.
Android ARM64 compilation and APK Signature Scheme v2 verification pass;
its 16 ZIP entries contain no original commercial game packages. It adds real
transactional actor births, checkpoint-v8 cold graphs, collision registration
and staged replacement actor materials. The material path and performance
are not yet device-verified; automatic authored player/world startup remains
unfinished. See [ACTOR-SPAWN.md](ACTOR-SPAWN.md). The accepted installed
player-controls APK below remains unchanged.

The preceding **backend-only, not installed** lifecycle APK is
`artifacts/release-actor-lifecycle-20261009/DeusExQuestVR-actor-lifecycle-20261009.apk`:
18,621,265 bytes, SHA-256
`AFA77AAFD9CAA1AE3C24562F2BF95CF48C7C2FDA2FBF5D96BD5A60D323742E4F`.
Android ARM64 compilation and signature scheme v2 verification pass; no original
commercial game packages are embedded. It adds native ownership/attachment/
deletion callbacks, checkpoint-v7 topology and committed geometry/ambient
publication. These backend operations are host/original-data tested, not newly
accepted on-device gameplay. Full Spawn/startup/campaign remain unfinished.
See [ACTOR-LIFECYCLE.md](ACTOR-LIFECYCLE.md). The installed player-controls APK
below remains unchanged and has a second user-confirmed automatic seated-height
acceptance recorded in [PLAYER-VR.md](PLAYER-VR.md).

The preceding **backend-only, not installed** class-default build is available at
`artifacts/release-class-defaults-20261009/DeusExQuestVR-class-defaults-20261009.apk`.
Its SHA-256 is `DE69AC761B7D6A6D01FDEE9C024348E244191D59D5213F1498BCF7BBAE5F9BB2`
and size is 18,600,785 bytes. Android compilation and APK Signature Scheme v2
verification pass; no original commercial packages are embedded. It adds
transactional concrete-class defaults/native283/checkpoint-v6 backend support,
not new visible player controls or complete campaign startup. See
[CLASS-DEFAULTS.md](CLASS-DEFAULTS.md). The installed, physically accepted
player-controls handoff remains the build described below.

This player-controls APK replaces the frozen asynchronous-startup build
`25e38b3`. It is installed on the user's Quest 3, not a finished full-campaign
release. Original commercial game data remains in the existing
private app directory; updating did not uninstall or clear game data or saves.

Package: `dev.deusex.questvr.smoketest` (Android ARM64 debug build).
APK size: 18,600,785 bytes.
SHA-256: `5B1B253765F3F31B80EA5575AD3865472232DBA57AD2837F47F429D0C4AFF403`.
APK Signature Scheme v2 verifies. The APK contains port/SDK libraries and SDK
UI assets, not original `.dx`, `.u`, `.utx`, `.uax` or `.umx` game packages.

## What was verified

- Android/OpenXR initialization returns before expensive runtime preparation.
  Original CPU lighting and staged Training world/actor uploads then complete
  while the process stays running with valid tracking.
- Actual Quest eye captures show lit Training geometry, HUD, and original-art
  Inventory, Health, Goals/Notes and Logs pages with readable bitmap text.
- Idle measurement windows reach 72 fps. Initial staged uploads and diagnostic
  screenshot readbacks still have frame spikes; this is not a general
  performance certification or a full-map visual audit.
- 43 ordinary host tests pass. Two optional original-data integrations skip
  unless supplied the game root; their separate full original-data checks passed
  separately with the final runtime source. The startup ownership path was source
  reviewed, ARM-compiled and exercised on the headset, not host-unit-tested.
- The player increment loads actual JC lower-body and original weapon-viewmodel
  hands, with corrected grip-local orientation and native-size original textures.
  Quest captures from the preceding grip-only build show the textured hands.
  User-confirmed Meta-button recenter works. Captured submitted HUD center is
  exactly head-local `(0,0,-1.05)` with matching head-forward eye projection.
  Latest seated-height acceptance is recorded in [PLAYER-VR.md](PLAYER-VR.md).
- The corrective body/floor APK disables back-face culling only for the wearer's
  lower body, preserves map feet through vertical standing recenter changes,
  and blocks collision probing during partial map upload. Original-texture CPU
  comparisons reproduce/fix the self-view culling defect; 15 transform groups
  and 103 ground-probe controls pass. The user confirmed height/legs correct
  after explicit seated calibration. The final build additionally delays
  automatic calibration until focused tracking/collision/reference state settles
  (1,856 posture controls). Final physical cold start calibrated at 1.650 m
  without a manual command; a downward capture shows both hands/lower body,
  two submitted body surfaces and feet on the actual Training floor. Subsequent
  real head lowering produced 1.501 m eye height, as intended.

## Test controls

| Input | Action |
| --- | --- |
| Left stick | Move |
| Left stick click | Switch seated/standing height; sit upright when calibrating |
| Left grip | Show/hide developer HUD outside Persona |
| Right stick left/right | 30-degree snap turn |
| A | Use pointed actor; equip/use selected item when menu is open |
| Right trigger | Fire/use damage action when selected weapon and ammo permit |
| Right grip | Cycle selected inventory item |
| Left Menu | Open/close Persona inventory |
| Right stick in Persona | Up/down selects item; left/right changes enabled page |
| B in Persona | Close Persona |
| Y / X | Quick-save / quick-load |
| B outside Persona | Developer next-catalog-map request, not authored progression |

Wear the Quest in a lit room. A cold start currently prepares data for roughly
40 seconds before the world appears; a startup message is shown while tracking
is valid. Check movement/turn direction, nearby materials and lighting, menu
pages, pickups and an intentional save/load. Existing saves were preserved;
pressing Y deliberately replaces the quick-save, so avoid overwriting a save
you want to keep. Report the map/actor and whether a bug follows launch, menu
use, pickup, save/load or travel.

Seated mode keeps a 1.65 m calibrated virtual eye height while retaining real
head/controller motion, ground-aligned avatar feet and the same floor in collision
and saved-position mathematics. The mode is remembered separately from saves
and recalibrates after each session's Training load and a short stable-tracking
window. Sit upright while loading; later leaning/crouching remains real motion.
Standing mode keeps physical floor height.
Long-press right Meta for system recenter; the app no longer cancels its forward
reset. The Meta button is not intercepted by game input.

## Known boundaries

Inventory starts empty until pickups or a quick-load. Goals/Notes and Logs
start empty until the corresponding supported runtime actions occur. Health
shows aggregate health; original per-body-part damage is not implemented.
Augs, Skills, Conversations and Images tabs are intentionally disabled, not
working pages. UI interaction, item population and the new v4 script-state
save format were not newly exercised through physical buttons in this handoff.

Full authored script startup, AI/state/latent execution, timers/spawns,
campaign archiving/progression, live GPU animation and remaining dynamic
lighting types/effects are unfinished. Worker teardown joins before global
runtime destruction and can wait if preparation is still running. See
[STATUS.md](STATUS.md) and [SCRIPT_STATE_SAVE.md](SCRIPT_STATE_SAVE.md).

Player visuals are static legs/lower coat and rigid controller-attached hands,
not full-body IK, arm/finger animation, walking/crouching animation or held
weapons. Black/missing-looking Training column sides remain visible in captures;
this player-controls batch does not fix or certify all map rendering.

The local `artifacts/release-player-controls-20261009/` folder holds the frozen APK, checksum,
device captures and handoff notes. These generated files are intentionally
ignored by Git. Source and build instructions are committed; no new feature
batch should replace this build before the user's headset test.
