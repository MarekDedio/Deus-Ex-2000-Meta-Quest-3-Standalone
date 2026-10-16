# Current Quest test-build handoff (2026-10-16)

The subsequent crate sound-script, original BSP FastTrace, sound-routing, VRand,
fragment-texture, native-clock and MakeNoise/hearing checkpoints are
source/tests only: no replacement APK was built or installed. The NPC APK below
remains the recorded accepted device build. Final host CTest106/121 passes with15 explicit
original-data skips; separate actual Training traces, lamp activation, native
Destroy279 and damage probes pass too. Both isolated dormant-world fatal hits
now complete original Frag/nativeDestroy with six actual WoodFragment births,
selected textures, exact velocity/scale/RNG and whole-checkpoint save/rollback.
Native ULevel Double clock ownership, exact explicit phase and codec11/envelope16
continuation now pass; this is not a whole-world Tick. MakeNoise512 and original
Pawn.CanHear now execute with native caches, live probe/list/callback routing,
team checks and directed BSP. Both crates also complete isolated true nonnull
Pawn-instigated fatal hits with six fragments, exact cache/animation preservation
and rollback; owned626/4 passes. This does not establish a possessed VR player,
live melee or a started original world. Direct begun destruction
creates then relevance-deletes a real Lockpick in its isolated Game=None fixture;
it is not surviving loot or complete startup. Native bound-player audio and destructible crates
are not ready for headset acceptance. See
ORIGINAL-LEVEL-CLOCK.md, ORIGINAL-MAKE-NOISE.md, ORIGINAL-MESH-TEXTURES.md, ORIGINAL-FAST-TRACE.md, SOUND-RUNTIME-CONTRACT.md and STATUS.md for explicit limits.

## Original Training NPC waiting animations (2026-10-15)

The user accepts the hand models/quiet trigger, then prioritizes NPC T-poses
before crates. All18 Training mesh NPCs now have original-script-selected
waiting presentations: humans breathe in their original arms-down pose;
both robots use their authored Idle. No hardcoded idle-name table or fake
started AI/state is used. Valid authored map/script poses (sitting/lying/tweens)
are preserved. See NPC-WAITING-PRESENTATION.md for the renderer-only scope,
rollback contract, supported cases and outstanding AI/animation work.

Strict coherent host build succeeds. CTest93 passes/107 registered,14 explicit
owned-data skips, zero failures in22.77s. Synthetic NPC20019/8 refusal controls
and separate actual Training280531/26 pass, with complete checkpoint/RNG/revision
rollback, existing original command preservation and original source/GC checks.
Six software previews were produced; inspected Jaime/doctor/trooper views have
normal arms-down poses and original materials.

Final signed ARM64 APK:
`artifacts/npc-waiting-20261015/DeusExQuestVR-training-npc-waiting-final-20261015.apk`
(34,033,382 bytes), SHA256
`A56F67A040445E2EEB771D6928E8C32AC781A852FBF4622229C6002C73CCD9CA`.
Final Android build succeeds in1m22s, v2 signature verifies with one signer.
Automatically replace-installed without clearing saves on Quest3
`2G0YC5ZG620985`, update2026-10-15 13:50:27; launched process11881.
Installed base.apk checksum matches exactly. The final process prepared all18
NPC presentations, but head tracking is unavailable (flags0), so no final-APK
render capture is claimed. The accepted first-APK evidence is distinguished below.

The first NPC APK (34,032,926 bytes, SHA256
`7A67CB1D80A8D175C4A9619A63CCC9E225B17F4BD61D80BC77E05B0B20131130`)
was installed with matching checksum, launched process9675, and captured at
`artifacts/npc-waiting-20261015/quest-training-npc-window-20261015.bmp`.
The inspected1680x1760 frame shows an arms-down NPC through the desk window,
original speech beside health/compass and tracked keyring. Actual GPU clip
uploads resolve BreatheLight for GM_Trench_F, GM_Trench and GM_Suit. The user
confirmed "all npcs are fine" and requested destructible crates next.
The final APK additionally preserves already-valid original animation poses;
the same18 Training results pass. The first APK's capture is not mislabeled as
an exact-final-APK screenshot. Steady windows reach72fps/13.89ms, but streaming
and diagnostic readback still produce slower windows; no full performance,
AI, full tutorial or campaign acceptance is claimed.

At the NPC APK checkpoint, the next crate probe passed138 original-data controls and reproduced both
crates' true surviving damage6 (HitPoints10 to4/ScaleGlow0.46). Fatal damage
then rolled back at DeusExDecoration.Active.TakeDamage PC464, unsupported
PlaySound264. The source-only2026-10-16 checkpoint above supersedes that boundary;
it does not change this APK or complete crate gameplay.

## Tracked keyring, knife and crowbar / quiet trigger (2026-10-15)

The accepted HUD placement remains. Right trigger now uses the equipped item
without revealing the quickbar or extending its display window. Right grip
still cycles/reveals and can hold the bar open; right-stick click selects the
keyring. A-pickups and closing Inventory retain their brief selection feedback.

Original full right-hand-plus-item assemblies replace the generic right hand
for the native starter keyring and actual equipped CombatKnife/Crowbar actors.
They track RightRemotePose in canonical grip-local meters. No duplicate hand,
pickup-mesh substitution, model reload at equip or original Pawn claim is made.
All textures/geometry are prepared before gameplay and retained across maps.
Unsupported/mismatched/removed items fall back explicitly to the generic hand.

Melee uses original typed base damage/range; trigger/controller-ray targeting
remains, not original Fire/skill modifiers, attack animation or physical swings.
Other item variants, firearms/ammo fidelity and breakable crates remain open.
See HELD-ITEM-VISUALS.md for source contracts and bounds.

Full coherent host build/CTest:92 passes/106 registered,14 explicit owned-data
skips, zero failures in17.78s. Expanded actual collected/equipped metadata
contract2016/16 refusal controls passes; cache1007448/42 also passes.

Signed ARM64 APK:
`artifacts/held-items-20261015/DeusExQuestVR-tracked-held-items-20261015.apk`
(33,909,870 bytes), SHA256
`155DF306966C77E523A0F326CC82D5892B18DDDA009FC0ACF28539016A2B4168`.
Final Android build succeeds in1m1s; APK v2 signature verifies. Automatically
replace-installed without clearing data on Quest3 `2G0YC5ZG620985`, update
2026-10-15 11:45:24, launched process8708. Installed base.apk SHA256 matches.

Exact-build1680x1760 Quest captures were inspected:
`artifacts/held-items-20261015/quest-held-live-20261015.bmp` shows the original
keyring and right hand, and `quest-held-followup-20261015.bmp` in the same folder
shows the original crowbar and right hand. The generic left hand remains;
there is no duplicate generic right hand. HELDSTATE confirms the corresponding
native ring/actual Crowbar actor, cacheReady1/drawReady1/rightTracked1, with
Crowbar base damage6/range80 original units. Live selection also resolves the
actual CombatKnife actor with base damage5/range80. Knife capture and physical
alignment/comfort acceptance are not inferred from those selection logs.

Idle HUDSTATE records health1/compass1/belt0/alpha0 and grip0/trigger0. The human
trigger-only test and palm-up/down acceptance are still pending. Steady timing
windows reach72fps/13.89ms; screenshot readback produces slow frames, so this is
not a complete performance pass. HELDSTATE is a read-only presentation diagnostic;
HUDSTATE now includes grip/trigger values for the quiet-use check.

## Previous installed build: contextual quickbar and separated speech HUD (2026-10-15)

User-priority changes: Jaime/NPC portrait/text no longer covers health or compass.
The original431x114 speech canvas is placed immediately to the column's right,
with a shared top edge and0.04m gap. Scale is reduced15%, retaining original
artwork, fonts, portrait and text layout; both-eye horizontal bounds stay inset.

The low quickbar now faces the eyes instead of remaining forward-parallel.
Its center, width and height are unchanged; top/bottom edges are equally far
from either eye under tracked yaw/pitch/roll. Center-row left alignment with
health/compass remains. Actual tilted vertical corners occupy about35–41 degrees
below the eye line, replacing the previous parallel-plane corner approximation.

The bar starts hidden and appears on deliberate equipment actions. It remains
visible while right grip/trigger is held; after release/action it shows for
three seconds, fading during the final0.25s. This is presentation-only: hiding
never unequips or consumes the selected item. Inventory and tracking/loading
guards still suppress gameplay overlays; health and compass do not auto-hide.

- Right grip cycles occupied shortcuts and reveals the bar; holding keeps it visible.
- Right stick click selects/reveals the starter keyring.
- Inventory: browse an item, press A to equip, then close with B/Menu.
- Right trigger outside Inventory uses the equipped item, even while the bar is hidden.
- Pickups and closing Inventory briefly reveal the selected shortcut.

Host regression checks: layout57930, visibility405884/24 expected invalid-time
refusals. Full CTest91 passes/105 registered,14 explicit owned-data skips,
zero failures in19.04s. The independent held-item preparation is also checked
but deliberately not activated by this UI-only APK; see HELD-ITEM-VISUALS.md.

Signed ARM64 APK:
`artifacts/hud-reposition-20261015/DeusExQuestVR-contextual-eye-facing-hud-20261015.apk`
(33,810,966 bytes), SHA256
`E65186D5177C54018DEA8847C4E140A1A0D8DD46FE181C4837AB2F56AD0B4356`.
Final Android build succeeds in1m3s, APK v2 signature verifies, automatically
replace-installed without clearing data on Quest3 `2G0YC5ZG620985`, update
2026-10-15 11:25:07. Installed base.apk checksum matches exactly; launched
process12666. Headset visual/comfort acceptance remains separate from these
compile, signature and installation checks.

A tracked1680x1760 frame from this exact build was captured and inspected:
`artifacts/hud-reposition-20261015/quest-contextual-hud-idle-20261015.bmp`.
It shows the actual Jaime portrait/typewriter speech beside unobscured health
and compass, with the quickbar hidden. The same frame's HUDSTATE reports
health1/compass1/speech1/belt0/alpha0. This proves that rendered combination,
not human comfort or a complete held-item renderer.

The revealed state was also captured and inspected at
`artifacts/hud-reposition-20261015/quest-contextual-hud-revealed-20261015.bmp`:
actual Jaime speech, health and compass remain separate while the eye-facing
bar is visible. HUDSTATE records health1/compass1/speech1/belt1/alpha1. A later
idle query records health1/compass1/belt0/alpha0, proving on-device hide/show
without hiding the other HUD panels. Holding right grip/trigger intentionally
extends the visible window. Steady windows reach72fps/13.89ms; streaming and
debug screenshot readback still produce slower windows, not a full performance
acceptance pass. A normal session reinitialization interrupted the first reveal
request; it was left intact, consumed after startup, and not overwritten.

Diagnostics: BELT reveals presentation without changing inventory/equipment;
HUDSTATE reports final per-frame overlay visibility/alpha and layout. Neither
creates or equips items. These use the existing tracked-app diagnostic mailbox.

## Previous installed build: much-lower quickbar correction (2026-10-15)

The user rejected the27-degree quickbar position as still too high. Move only
the bar center fromY=-0.84m to-1.28m at the same1.65m depth: about38 degrees
below eye level, top about35.5 degrees and bottom just below40 degrees. The
health/compass column, shared left edge, artwork scales and controls are
unchanged. This replaces the first repositioning APK below.

Signed ARM64 APK:
`artifacts/hud-reposition-20261015/DeusExQuestVR-bottom-quickbar-20261015.apk`
(33,806,942 bytes), SHA256
`4BB211985F145241C8E87BCEFDB8E9EF92CD3AAF780DC4F4CD9E756F5EE1378F`.
Android build succeeds in1m3s; APK v2 signature verifies. Automatically
replace-installed without clearing data on Quest3 `2G0YC5ZG620985`, update
2026-10-15 11:05:33, then launched (process9786). Installed base.apk SHA256
matches exactly. Head tracking became valid (flags0xf), Training loaded and an
in-app1680x1760 frame from this exact process was captured and inspected:
`artifacts/hud-reposition-20261015/quest-bottom-quickbar-20261015.bmp`.
The bar is fully visible in its lower position, the health/compass left edge
stays aligned and the center view is clearer. This is device rendering evidence,
not physical comfort acceptance; that still needs the user's test.

Playable presentation/placement passes240 controls, including unchanged
health/compass and both-eye alignment; compass/view607. All affected host targets
rebuild. Full CTest:87 passes/101 registered,14 explicit owned-data skips,
zero failures in11.81s. The new bounds encode the requested much-lower position,
not an assertion that an angular threshold alone proves headset comfort.

## Previous installed build: lowered, left-aligned HUD (2026-10-15)

The user accepts the current artwork but requests less eye strain and less
central obstruction. This layout-only APK keeps all icons, scales, equipment
controls and gameplay unchanged. Hand-held item development is temporarily
deferred in favor of this explicit placement request.

- Health and compass move0.18m down and0.1655m left in the shared1.65m HUD
  plane. Their left edges now exactly match the quickbar's left edge(-0.8115m).
- The original four-pixel health/compass gap and artwork aspect ratios remain.
  Health's top is about19.7 degrees above the eye line, down from25 degrees.
- The quickbar center moves fromY=-0.49m to-0.84m (about17 to27 degrees down).
  Its top clears the central24 degrees; its bottom stays just below30 degrees.
  The user subsequently rejected this bar position as still too high.

Signed ARM64 APK:
`artifacts/hud-reposition-20261015/DeusExQuestVR-lowered-aligned-hud-20261015.apk`
(33,806,942 bytes), SHA256
`37E77DDAE46750672F4161A354D3BFF57DFE4202C314E69BC5A7FBF22A39D89F`.
Android build succeeds in1m24s and the APK v2 signature verifies. The Quest was
initially absent from ADB, then reconnected during verification. The build was
replace-installed on Quest3 `2G0YC5ZG620985` without clearing data, update
2026-10-15 11:00:02. The installed base.apk checksum matches exactly and the
game was launched (process8660). Physical comfort acceptance is still pending.

The affected host targets rebuild successfully. Playable presentation/placement
passes238 controls (including both-eye alignment for52–74mm IPD), compass/view
passes607, and the focused quickbar/HUD quartet passes. The full CTest run has
87 passes/101 registered,14 explicit owned-data skips, zero failures (16.58s).
Only placement headers/tests changed; no original asset or package was modified.
An inspected original-artwork software rendering uses actual Training equipment
and these shared projection constants. It confirms the shared left edge and
clear central view; it is not a headset screenshot or physical comfort pass.

## Previous installed build: health preview and playable quickbar (2026-10-15)

Signed ARM64 APK:
`artifacts/quickbar-health-20261015/DeusExQuestVR-health-quickbar-20261015.apk`
(33,806,910 bytes), SHA256
`2BA5D4D788EF789A1A9899FF1F70D582A62F92CC3C6524C00E917EBDE6737610`.
Build succeeded in1m21s; APK v2 signature verifies. Replace-installed on Quest3
`2G0YC5ZG620985` without clearing data; device update2026-10-15 09:53:23 and
installed base.apk SHA256 match exactly. Launched process10934. Head tracking
was not active during the initial verification; physical HUD/controls acceptance
is the user's next test, not an inferred on-device pass.

The pure quickbar model passes205129 controls, health/comfort141, and the actual
Training integration314: real pickups, matching locked Train-key door, selected
equipment, key counting, consumption, warm/cold saves, legacy migration and
atomic rejection of malformed v15 envelopes. Production quickbar queries use
indexed original metadata with no package or icon decoding on the render thread.
The final coherent host build passes; CTest reports87 passes/101 registered,
14 explicit owned-data skips and zero failures (16.76s). Separately invoked
original player/HUD controls pass6095 checks; the VM passes694494 checks with
778 rejection controls. Skipped owned-data tests are not counted as passes.

The original body-health artwork is now visible above the accepted compass.
Until independent body damage is integrated, all six segments mirror the same
aggregate port health; energy is a full-scale placeholder, not completed energy
gameplay. The original541x69 belt is centered and raised into comfortable view.

The starter keyring occupies slot0 and aggregates actual collected NanoKey
actors. Picked-up usable items fill stable slots1–9; ammunition and loose keys
do not consume shortcuts. The selected border is actual playable equipment,
independent of merely browsing the Persona inventory. The native key component
is not claimed to be an original-script Pawn-owned ring UObject.

- Right grip: cycle through occupied shortcuts, including the keyring.
- Right stick click: select the keyring directly.
- Inventory A: equip the browsed item; close Inventory before using it.
- Right trigger: use the equipped weapon/consumable, or use the keyring on a door.
- A outside Inventory: interact/pick up/open doors as before.
- Y/X: save/load, now preserving quick-slot assignments and equipped selection.

No keyring model in the player's hand is required or added by this milestone.
The new runtime v15 equipment envelope retains its complete original v1–v14
payload; older saves still load and initialize the functional starter ring.

## Scope change and remaining original-HUD work

Latest override (2026-10-15): the user explicitly requests a visible unfinished
health display plus the functional quick inventory bar/starter keyring, followed
by compilation and Quest installation. Held keyring geometry is not required.
This replaces the previous full-HUD packaging restriction below. The complete
original player/window/augmentation integration remains future work.

The earlier full-HUD restriction is retained as historical context only.
The eye-facing Persona adjustment was built/installed before that restriction;
the current health-preview/quickbar APK follows the latest explicit override.
See HUD-COMPLETION.md for the remaining full original-HUD acceptance checklist.

Source-only HUD layout progress (2026-10-15): original crosshair preferred-size,
ConfigureChild, clipping/drawing-GC propagation and belt assignment now execute.
The pending graph now completes augmentation-key label setup through audited
IntToString0x53, then refuses the unsupported DamageHUDDisplay.sIconInfo.DamageType
struct-array String access (132 objects/8783 writes). Native geometry helpers and
argument-copy tests pass; this original graph is not the installed native
playable quickbar and does not establish completed original player possession.

## Previous installed APK: eye-facing Persona placement

`artifacts/crate-runtime-20261013/DeusExQuestVR-persona-eye-facing-20261014.apk`
is signed ARM64,33,217,654 bytes, SHA256
`3516B8D72AF8A4B6575A38EB2D25E7F1A2465B521E8DA2BDFAA52D570D57BBD4`.
Built/replace-installed before the HUD gate, preserving data on Quest
`2G0YC5ZG620985`, update `2026-10-14 05:45:53`. The installed base.apk SHA256
matches exactly. Android compilation passed in1m46s and APK v2 verified.

The user's first lowering test was too low and looked tilted. The correction
uses head-space centerY=-0.16m, shared1.20x0.90m geometry and a pitch that faces
the eyes. Artwork, text and controller targeting share that placement;207
generated placement checks pass. Exact corrected-build headset comfort has
not yet been accepted. The later approved milestone is installed as documented above.

## Previous installed APK: original compass, inward placement and player foundation

`artifacts/crate-runtime-20261013/DeusExQuestVR-original-hud-compass-inset-20261014.apk`
is signed ARM64,33,139,566 bytes, SHA256
`9653002F6CDE58C463EE0EF72D7C59B2532C5F19AA80BF723C26EDFE2733D026`.
Android build succeeds in1m9s; APK v2 signature verifies. Automatically
replace-installed without clearing data on Quest `2G0YC5ZG620985`, update
`2026-10-14 01:15:37`; pulled installed base.apk matches exactly, then launched.

Original map-relative compass is implemented and captured on the preceding
same-art build. The user confirms it works, but PC-edge attachment was too far
outside comfortable view. This build moves its center to(-0.50,0.25,-1.65)m,
preserving source scale; the health column follows its original alignment/gap.
Process11070 and a frame captured from this exact installed APK confirm the
inward shift. The user accepts the revised placement as comfortable, after
confirming compass direction works. Broader recenter/map-route testing remains
separate. Music/Jaime remain the previously accepted baseline.

The full original health/belt artwork and typed owned-player readers are in
place, but those panels intentionally remain hidden until real possession.
Original keyring/session controls pass169 checks in isolated dormant worlds;
they are not live equipment. InitRootWindow1052 and world ConsoleCommand are
the next explicit startup dependencies. NPC animation, gravity, crate fatal
damage, held items, lamp light and full tutorial completion remain unfinished.

Final CTest74 ordinary/86 registered/12 owned-data skips passes,13.10s. Separate
original HUD87, generated HUD20571, rendered heading/placement606, original
player/keyring169, defaults1767/71 and VM678235/369 controls pass, as do
diagnostic mocks26/0 and the focused original startup/rollback probe. No
original Windows executable/DLL was run; commercial assets stay outside APK/Git.

## Previously installed APK: original Training music

`artifacts/crate-runtime-20261013/DeusExQuestVR-training-music-20261013.apk`
is signed ARM64, 32,918,766 bytes, SHA256
`8673B16C3F37B5771DC0470299786D4834BF2DCD6062B426C66388416FC536C1`.
Android build succeeds in 2m13s and APK v2 signature verifies. Its 20 entries
contain port/SDK code and licensed decoder notices, not original game packages.
Initially installation was skipped while USB was absent. On reconnect it was
replace-installed without clearing data on Quest `2G0YC5ZG620985`, update
`2026-10-13 14:23:21`. Pulled installed base.apk matches the full SHA256 above
exactly. One launch confirms process 8558; Training CPU preparation and AAudio
startup (22050 Hz) pass. Tracking subsequently becomes valid, Training actor/
collision banks publish, and logs confirm the original IT section0 playing.
The user confirms background music and Jaime's speech are both audible. A
consumed MUSIC diagnostic reports stream present and zero underrun frames;
the focus gate is inactive at that later check. Long-duration/focus-resume
acceptance remains separate from this successful initial listening test.

The actual Training map selects Training Room from its original UMX. Decoder
work runs outside the callback into a fixed PCM ring; music is mixed at 0.35
with existing ambience and speech and pauses with app focus/map loading. Final
CTest passes 72 ordinary / 82 registered / 10 explicit owned-data skips, zero
failures, 11.37s; separate original music decoding/fingerprint test and 25
diagnostic-tool mock tests pass. This does not yet complete dynamic music
events/fades, player settings, full HUD/keyring, NPC animation, crate/gravity/
held-item gameplay or lamp illumination.

Initial headset audibility is accepted. Further checks are pause/resume focus,
long-duration playback and accumulated missing PCM frames. General gameplay
performance and the remaining user-prioritized features are still unfinished.

## Previously installed APK: original focus labels and runtime prerequisites

`artifacts/crate-runtime-20261013/DeusExQuestVR-focus-runtime-20261013.apk`
is signed ARM64,20,094,921 bytes, SHA256
`B07B838EA6BE257D3E8533ECE35C570C3371DA42DF86655E14FB25D7F46F8D76`.
Android build succeeds in1m51s. Replace-install preserves data on authorized
Quest3`2G0YC5ZG620985`, update13:16:25; pulled base.apk matches exactly,
launch8979. Golden original-font object labels and Unlocked door status were
captured on the preceding same-UI build; the user confirms the lamp label too.
The final rebuild additionally fixes qualified class-name case sensitivity.

Original mover focus proxies/non-mover insets and original Amber palette follow
the supplied PC references. Declared-field ownership and bounded class ancestry
protect the typed name/status queries. Full CTest passes70 ordinary tests out
of79 registered/nine skips,25.37s; separate fresh-process case-order test passes
(now80 registered). Original focus audit finds Desk Lamp and14 unlocked movers;
held-item metadata and both crates' original DropThings/surviving-damage probes
pass. Fatal crate damage and lamp Frob explicitly roll back at native264.

This does not fix lamp illumination, held-item rendering/equip, gravity,
fatal crate destruction or NPC T-poses. The user's requested work order is now
Training music, original full HUD/starting keyring, NPC poses/animation, then
gravity/crate destruction/held items. See STATUS.md and the source contracts.

## Previous APK: streaming safety follow-up

`artifacts/tutorial-streaming-20261013/DeusExQuestVR-training-streaming-safety-20261013.apk`
is signed ARM64,20,049,433 bytes, SHA256
`DBE03808B1A46DCF5D4FD4144D35DFF03AC76D2D72C9269AF44C559C189C3BA2`.
APK v2 signature verifies; Gradle succeeds in1m46s. USB was absent at build
completion. On reconnect it was data-preservingly installed to authorized
Quest3 `2G0YC5ZG620985`, lastUpdateTime `2026-10-13 12:06:13` and launch
process8446. Pulled installed base.apk matches the SHA256 above exactly.
The later focus/runtime APK above supersedes this installed version.

This includes the retained-pose version plus nonblocking retirement when a
pickup removes the pending-pose actor, and one-shot publication of instant
mover command endpoints/collision correction. Final CTest passes68 ordinary
tests/76 registered/eight skips, zero failures,21.57s. Separate owned Training
route/generated instant-mover tests pass1,226 checks. The strict native check,
host and Android builds pass. Final headset timing/visual acceptance is pending.
It does not add visible held weapons, crate damage, gravity or NPC animation;
see TRAINING-WEAPON-CRATE-GAPS.md for the latest user's confirmed requirements.

## Previous built APK: streaming, inventory and retained mover poses

`artifacts/tutorial-streaming-20261013/DeusExQuestVR-training-streaming-retained-poses-20261013.apk`
is signed ARM64,20,049,273 bytes, SHA256
`12D465000B70844516AD7AF6B5F40133FA7A4DCBC4E7CDCDD8424A957D865D76`.
APK v2 signature verifies; all16 ZIP entries contain port/SDK code/assets, not
commercial packages. Gradle succeeds in1m2s. USB was absent at completion, so
automatic install was correctly skipped; install this file when Quest returns.

This adds visible-only identity-preserving inventory selection, held-repeat
stick navigation, controller-ray/trigger icon selection, original item names
and descriptions, retained spatial gameplay and actor removal without a bank
rebuild. Door poses are retained and rebased without re-reading every mover.
Full CTest passes68 ordinary tests/76 registered/eight owned-data skips,
zero failures,10.86s; new navigation75 and ownership/cache1,045 checks pass.
Strict Android syntax and separate original metadata/inventory integration pass.
Final cache-change headset timing is not yet verified.

## Previously installed/tested APK: first streaming/inventory replacement

`artifacts/tutorial-streaming-20261013/DeusExQuestVR-training-streaming-20261013.apk`
is20,049,273 bytes, SHA256
`ACC35F28EB61E8900A45391C3C94607EBEA7415809597AA7E030FD3BF605F0B5`.
Data-preserving replacement succeeded on Quest3`2G0YC5ZG620985`, lastUpdateTime
`2026-10-13 08:20:55`; pulled base.apk matches exactly, launch process22073.
The user confirms freezes are basically fixed and inventory browsing works.
Live scene publication/screenshot succeeds. Logs still caught109.273ms mover
snapshot rebase and116.01ms selection, which motivated the final cache change.

The user also confirms an invisible selected crowbar and rejected damage on
intentionally breakable crates, plus a floating stack. The current attack
adapter is NPC-only; selection is not original equipment state. Held weapons,
crate damage/gravity, NPC T-poses, starting NanoKeyRing, ten functional belt
slots, body-part health and compass remain unfinished. MENU opens inventory;
right-stick up/down or pointing+right trigger selects; A only has the existing
consumable action, not genuine weapon equip. See ORIGINAL-HUD-CONTRACT.md.

## Previous installed APK: receiver/context foundation

`artifacts/class-context-foundation-20261013/DeusExQuestVR-receiver-foundation-20261013.apk`
is signed ARM64, 20,044,129 bytes, SHA256
`5A382E5DEDA7771B4B3FF4036F0BF1FDACC5D75E0E763BE265F1DC9FD0D78746`.
APK v2 signature verifies; all16 ZIP entries contain port/SDK code/assets, not
commercial game packages. Data-preserving replace-install completed on Quest3
`2G0YC5ZG620985`, installed base.apk matches exactly, and lastUpdateTime is
`2026-10-13 07:21:56`. One launch confirms process10802 and tracked Training.

This includes the preceding visual fixes plus native280 timers, qualified-field
read/shadow isolation and audited opcode/frame receiver distinctions. It does
not enable original world startup, automatic NPC ticking or ClassContext.
Fresh CTest passes66 ordinary tests/74 registered/eight opt-in skips, zero
failures,29.58 seconds. Separate original actor integration and exact unchanged
startup rollback pass. The permanent original ClassContext structural census
passes1,184,728 checks; these are not full gameplay acceptance.

Inspected live eye captures show original Jaime artwork/portrait, rendered hands,
all four white focus corners on a moving door, and textured plant/lamp/drink at
the desk. Door publication125 frames is8.530ms mean/9.928ms worst; live retained
bank swaps complete. Stalls remain. The new user recording specifically reports
streaming/pickup freezes, NPC T-poses and equipment browsing problems; these are
under investigation, not accepted as fixed. Glass, full route and final performance
acceptance remain open. Captures/logs are ignored under
`artifacts/class-context-foundation-20261013/`.

## Previous installed APK: Training video visual fixes

`artifacts/tutorial-mover-20261012/DeusExQuestVR-training-visual-fixes-20261012.apk`
is signed ARM64, 19,983,745 bytes, SHA256
`A3FEA0B06DDFE6967215144D2EFAA2FFEEF87870DD98A3D947E78F781A77E83A`.
APK v2 signature verifies. Its 16 ZIP entries contain port/SDK libraries and
assets, not original commercial packages. Default replace-install preserved
app data/saves on Quest3 `2G0YC5ZG620985`; installed base.apk has the exact same
hash and lastUpdateTime `2026-10-12 12:40:03`. One launch confirms process25187.

This replacement includes the reviewed original speech artwork, four-matrix
focus-bracket upload, inside-object targeting and footprint stair sweeps below.
It also retains the visible actor/collision bank until its replacement is
complete, preserves atlas generations through renderer retirement, and recovers
original static BSP material flags for screen/modulated glass blending. Door
refreshes use authoritative mover state; speech/UI remains visible during a
retained refresh. The existing 25m/512-actor residency limits remain unchanged.

Fresh full host verification: 64 ordinary tests pass (70 registered, six
explicit owned-data skips), zero failures, 24.57 seconds. Separately, the owned
Training route passes 1,184 checks, six real stair routes pass 26 checks, the
actual window1942 audit passes, and original speech artwork passes 13 checks.
Pure speech49,158, targeting97, ground/publication141, retained-bank36 and
material15 checks pass. The Android build and strict native syntax checks pass.

Same-camera original-data software previews show the previously black window
now revealing the observation room/NPCs, with the same 60,766 triangles. Frame
hash changes from `2cda0a972317c51a` to `41dead0d704197df`; this is offline
evidence, not acceptance of the Quest glass shader.

The exact installed APK reports valid head/grip tracking and steady samples at
72fps. A paired-door opening publishes 130 frames, two moving leaves, 8.369ms
mean/8.946ms worst. Two retained actor-bank swaps complete atomically in the live
log. Larger stalls still occur around loading/refresh and screenshot readback;
these samples are not final performance acceptance. An inspected live eye frame
shows textured hands and the `00_TrainingCombat` corridor after a map change;
it does not accept the Training desk/window/brackets. The screenshot request
timed out during transition but was later consumed; its completed frame was
pulled without replacing the pending command. Physical stairs, visible focus
frames, transparent glass and uninterrupted objects still require the user's
replacement-build walk/recording. Full tutorial/campaign completion is not
claimed. See [VIDEO-REVIEW-20261012.md](VIDEO-REVIEW-20261012.md).

## Previous installed APK: reviewed Training UI and stairs

`artifacts/tutorial-mover-20261012/DeusExQuestVR-training-ui-stairs-reviewed-20261012.apk`
is signed ARM64, 19,983,745 bytes, SHA256
`07F883068A00393DFC50B9FA1C2B3AB4886D2E0EE51D8A072B482ECDC2E00941`.
APK v2 signature verifies. Default replace-install preserved app data/saves on
Quest3 `2G0YC5ZG620985`; installed base.apk has the exact same hash and
lastUpdateTime `2026-10-12 12:21:26`. One launch confirms process22572.

This checkpoint adds original remote speech art/fonts/portraits, a corrected
four-matrix upload for all focus-bracket strips, inside-object controller
targeting, footprint stair sweeps and changed-mover publication through the
world-actor registry. It also keeps speech typing/expiry coherent across
tracking loss and refuses grounding against incomplete actor colliders.
The last launch reports no tracked head pose; launch alone is not acceptance.

Fresh verification: 62 ordinary host tests pass (68 registered, six explicit
owned-data skips), zero failures, 28.87 seconds. Original route1,184 checks,
actual runtime cross-map/save rollback and original speech-art13 checks pass
separately. Speech composition/timing49,158, target-frame97, stair26 and
ground/publication141 checks pass. Original assets remain separate from APK/Git.

The user's earlier 53.33-second headset recording confirms original Jaime
speech artwork, but exposes refresh pop-out and opaque BSP glass; see
[VIDEO-REVIEW-20261012.md](VIDEO-REVIEW-20261012.md). Those two fixes are being
prepared and are **not included in this installed checkpoint**. Nor does this
checkpoint prove full tutorial/campaign or final Quest performance acceptance.

## Previous installed APK: introductory Training recording route

`artifacts/tutorial-mover-20261012/DeusExQuestVR-training-demo-20261012.apk`
is the signed ARM64 test build: 19,983,537 bytes, SHA256
`E2398B7A2D89C4C81AED1DD86FEF31B7CA450DCE12A3A4123107D72AE6DED189`.
APK signature v2 verifies; its 16 entries contain port/SDK libraries and assets,
not original commercial packages. The default data-preserving deployment helper
replace-installed it on Quest3 `2G0YC5ZG620985`. Installed `base.apk` has the same
hash; lastUpdateTime is `2026-10-12 11:28:17`. One waited launch confirms process
12864. Rendering/tracking and physical route acceptance are separate checks.

This build adds exact opening Mission00 contact speech/audio and independent
gold subtitles, a bounded pending-instruction queue, paired authored sliding
door transforms/collision, matching-key use and v14 motion saves. Door use no
longer rebuilds every actor. See [TRAINING-DEMO.md](TRAINING-DEMO.md) for the
recording route, controls, acceptance checklist and explicit full-tutorial gaps.
No player hand/body geometry or controller/recenter mapping changed.

Fresh host evidence: 59 ordinary CTests pass (64 registered, five owned-data
opt-in skips), 25.17 seconds; 358 owned Training route checks and separate
original runtime save/cross-map rollback tests pass. Defaults1,002/PawnList67
and generated Spawn/positive attachment8,268/143 controls pass. The inspected
software closed-door image is not headset motion/performance acceptance.
Positive native CPU Attach is integrated; native shadow rendering, original
world/player startup, automatic AI and the full tutorial remain unfinished.
Logs, desktop frame and archived APK are ignored under
`artifacts/tutorial-mover-20261012/`.

Fresh `quest-training-demo.bmp` and `quest-door-check.bmp` show the corridor and
post-door lobby, textured hands, original lighting and compact status/control
text, with valid head/grips and actual eye heights1.721/1.683m. The introduction
and door-instruction contacts resolve event79/audio79 and event41/audio41 on
device. The user confirms introduction and door use. Idle samples reach72fps,
but the opening interval drops to43.3fps/166.66ms worst; screenshot readback
stalls291.65ms. Stairs remain impassable by user report. PC-style speech artwork/
portraits and object interaction frames remain pending. This is an installed
test checkpoint, NOT full tutorial or recording/performance acceptance.

## Previous APK: native client and Quest target lifetime

`artifacts/native-client-20261012/DeusExQuestVR-native-client-20261012.apk`
is the newest signed ARM64 build:19,695,049 bytes, SHA256
`C090387B78BF173026E9B33030CBD83C6E50EF5B8174C29821B5D4116F6D53A3`.
APK Signature Scheme v2 verifies. Its16 ZIP entries contain only port/SDK
libraries/assets, not original commercial game packages.

Both actual builds used default data-preserving auto-install, but the configured
Quest3 `2G0YC5ZG620985` was absent at deployment time. No alternative device was
selected and no installation is claimed. The checkpoint APK recorded below is
still the last device-hash-verified installation. A new APK will continue to
replace-install automatically whenever that authorized Quest is connected.

Data-probe:6,136,144 bytes, SHA256
`E67F782D5BBC7641754DE522B75CAEDBB0D159F009BDAD5B8EFDF311F4457B21`.
Quest renderer:5,375,640 bytes, SHA256
`4FF16F3B1BA9822193A4312DFA0552E5053F09C9E3AE09963565891766B35572`.
This batch binds the actual stereo target's native-client lifetime after SDK
resource creation and revokes it before target teardown. Original Attach entry
guards and client/server default-class selection are implemented, but positive
Attach, rendered native shadows and original world/player startup remain pending.
No hand/body geometry or controller/recenter mapping was edited.

All58 ordinary host tests pass (62 registered, four owned-data skips),33.50s;
generated Spawn/world/client7072/135, registry116/39, VM677971/338 and SDK patch6/0
pass. Original-data results and limitations are in [STATUS.md](STATUS.md).
There is no new Quest capture, physical alignment, tracking or performance
acceptance without hardware. See [NATIVE-CLIENT-BINDING.md](NATIVE-CLIENT-BINDING.md).
Build/test logs and the archived APK remain ignored under
`artifacts/native-client-20261012/`.

## Previous installed: native decal ownership, Detach and checkpoints

`artifacts/decal-runtime-20261012/DeusExQuestVR-decal-runtime-20261012.apk`
was the last device-verified signed ARM64 build:18,919,193 bytes, SHA256
`8D6B115A3745F51819FF0C8D977A03618B9D48D69A4674D5A19938CA0AE48237`.
The default helper automatically replace-installed it on connected Quest3
`2G0YC5ZG620985`, preserving app data/saves. Installed `base.apk` has the exact
same hash; device lastUpdateTime is `2026-10-12 02:31:21`. APK Signature Scheme
v2 verification passes. Its16 ZIP entries contain port/SDK libraries/assets,
not the original commercial game packages. One waited launch confirms
process22672; launch alone does not establish map/tracking readiness.

Data-probe library:6,110,648 bytes, SHA256
`1573F9BA501955A3D15D09DD1C28048A57C1447D4B3AFE947CF3470BE1BAF7EF`.
Quest renderer library:5,360,288 bytes, SHA256
`6BA7314A44E05C6CA6CA55E10204126940BC6BDD999A1D009AD94E91B2352FB4`.
The shared VM type/loader changes cause a relink; no hand/body geometry,
controller mapping or recenter behavior was edited in this batch.

The new runtime owns/journals native decal records and roots, implements original
Detach and bounded INT-array bridging, and cold-restores codec10/envelope13
checkpoints after actual authored schema validation. See
[DECAL-CHECKPOINT.md](DECAL-CHECKPOINT.md). Native Attach and rendered NPC shadows,
original player/world startup, possession and live campaign Tick remain pending.
This is not a full playable-campaign release. Default connected-Quest
data-preserving auto-install stays enabled for every actual new APK.

The latest launch reports tracking unavailable; no new physical hand/body or
visual/performance acceptance is inferred from the running process or untracked
72fps timing samples. The screenshot request could not complete because the
app did not consume its pending diagnostic within20 seconds; that mailbox was
left intact, not overwritten or repeatedly queued. Host/original-data results are in
[STATUS.md](STATUS.md); local logs and the archived APK are ignored under
`artifacts/decal-runtime-20261012/`.

## Previous: Spawn tag fidelity and shadow prerequisites

`artifacts/decal-prerequisites-20261011/DeusExQuestVR-decal-prerequisites-20261011.apk`
was the preceding signed ARM64 build: 18,919,145 bytes, SHA256
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
