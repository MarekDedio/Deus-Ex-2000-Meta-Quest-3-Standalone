# Port status

## Original sound routing / crate callbacks (2026-10-16)

- Added one engine-owned RAII UObject-index allocation domain across every
  RuntimeObject constructor, independent of pointers/exports/world slots/saves.
  Actual native264 now selects recipients from genuine caller provenance,
  native slot-zero NetMode, source.Level.PawnList and ordered native viewports.
  Live callbacks can splice nextPawn/change source Location; all effects remain
  synchronous and share the enclosing transaction. Explicit standalone/no-driver
  service ownership is loader-created, not inferred from missing reflection.
- Sound hearing uses exact integer dyadic PC64 geometry, separate retained and
  Float distances, original directional BSP queries, per-listener occlusion and
  Instigator (not Owner) exemption. Genuine empty recipient sets return the
  original channel. Base ClientHearSound's real non-PlayerPawn/unbound exits work;
  a valid Viewport audio route still refuses pending commit-only prepared audio.
- Object indices262188/10 refusal controls, hearing111595/25 and production
  generated routing2356/11 pass. Routing covers State versus simulated Function,
  context receiver versus FFrame.Object, all256 Slot bytes/both override values,
  live recipient/source mutation, solid BSP attenuation, client lifetime and
  complete later-callback rollback. Coherent host build passes; final CTest99
  passes/114 registered/15 explicit owned-data skips/zero failures in61.82s.
  Strict ARM64 syntax passes runtime and all changed/new integration fixtures.
- Separately invoked owned Training: lamp Frob completes (one lamp, no native
  bound-player audio/dynamic-BSP-lighting claim); both fatal None-instigator
  decoration damage probes now pass264 and reach real Fragment.CalcVelocity:7,
  missing VRand252, with exact complete rollback. Legal nonnull Pawn input still
  stops earlier at MakeNoise512:428. Damage regression152 checks passes.
- New genuine Destroy279 diagnostic257/2 passes: explicit isolated begun/startup
  controls execute Containers.Destroyed/FastTrace/super cleanup, while unchanged
  Contents=Lockpick rolls back at Containers.Destroyed:681 VRand252. These saved
  preconditions do not start the original game or manufacture surviving loot.
  Owned source bytes/timestamps/GC are unchanged. Owned548250 and NPC280519/26
  retain actual BSP behavior and all18 accepted waiting presentations.
- Source/tests only: no new APK, no new on-device acceptance. Held-item/NPC APK
  remains the recorded accepted device build. Crate gameplay/gravity, actual
  player/world startup, MakeNoise, VRand/debris/contents, bound-player native
  audio and full tutorial/campaign remain unfinished. See SOUND-RUNTIME-CONTRACT.md
  and TRAINING-WEAPON-CRATE-GAPS.md for exact scope and next dependencies.

## Original BSP FastTrace checkpoint (2026-10-15)

- Implemented retail FastTrace548 using the actual native Level's serialized
  Model, not render triangles or actor/mover rays. Exact ordinary PC64 arithmetic,
  Float spill/sign-bit boundaries, NotCsg-only classification and End-first
  reversed traversal are retained. Query work is bounded/shared per transaction.
- Added canonical CoreVector12 destination authority, required uninitialized
  End/full-write handling, sequential Start defaults and immediate548 input
  copies. Original declaration identity is validated before arguments even for
  lazy reflected FunctionIdentity records; full authored schema remains required.
  Warm model-cache comparison correctly distinguishes local export paths from
  qualified runtime UObject identities. Reflected Level/XLevel cannot redirect it.
- Pure BSP72081/10 refusal controls, VM695581/838 (new548373/54), generated
  integration2190/47 and separately invoked owned Training250 pass. Actual map
  integration covers16 clear/32 blocked supplied paths and16 omitted Starts;
  save/revision/GC/source bytes/timestamps remain unchanged.
- Coherent host build and final CTest:96 passes/110 registered,14 explicit
  original-data skips, zero failures39.54s. Strict ARM64 syntax passes. Separate
  original NPC regression280519/26 retains all18 accepted waiting presentations;
  crate152 and lamp rollback checks retain their documented264/512 boundaries.
- Independent named-property evidence corrects the detached sound helper's
  range exception to source.Instigator, not Owner. No production recipient/audio
  adapter consumed the old owner-named Boolean. See SOUND-RUNTIME-CONTRACT.md.
- This is source/tests only; no new APK or device acceptance is claimed. The
  accepted NPC APK remains installed. Native sound/noise, original player/world
  startup, crate breakage/debris/contents and support gravity remain unfinished.
  See ORIGINAL-FAST-TRACE.md for primary evidence, exact scope and remaining work.

## Crate sound-script foundation checkpoint (2026-10-15)

- The user accepts all Training NPC poses and resumes destructible crates.
  Implemented original native264 sequential defaults/omission snapshots and
  Sound=None's no-write path, with exact original declaration/type validation.
  Native result dispositions and real pre-argument caller provenance now survive
  raw/reflected/nested VM calls. Nonnull sounds still explicitly refuse; there
  is no fake crate disappearance or claimed complete destruction.
- New scalar sound protocol passes1,859,556 checks/43 safety refusals; VM
  passes695,208/784, including new destination/default/caller controls. Strict
  ARM64 syntax checks pass. Coherent host build and final CTest:94 passes,
  108 registered,14 explicit owned-data skips, zero failures in23.32s.
- Separately invoked owned lamp probes pass actual PlaySound(None) with1..6
  inputs, complete checkpoint invariance and wrong-type refusal. Nonnull Frob
  still rolls back at264 PC43. Owned crate152 checks now distinguish None
  instigator's264 PC464 from a legal nonnull Pawn's earlier512 PC428; both
  restore the exact complete checkpoint/revision. The NPC test input is not
  substituted for a possessed player in gameplay.
- No new APK is produced for this backend-only checkpoint. The accepted NPC
  APK remains installed. Audible native effects, original damage-instigator
  binding, noise, debris/contents, support gravity and full tutorial/campaign
  remain unfinished. See SOUND-RUNTIME-CONTRACT.md for exact retail evidence,
  rollback requirements and remaining native clock/identity/audio ownership.

## Training NPC poses/breathing accepted (2026-10-15)

- User confirms tracked hand models and quiet trigger work, then prioritizes
  NPCs before breakable crates. Original virtual PlayWaiting is previewed in
  a complete rollback-only transaction; GPU vertex clips remove the unresolved
  Fighter/All reference pose without inventing a started original AI/world.
  Sixteen humans use actual breathing loops and two robots their original Idle.
  Valid authored poses and subsequent original animation commands are preserved.
- Strict coherent host build and CTest93 passes/107 registered,14 explicit
  owned-data skips, zero failures22.77s. NPC synthetic20019/8, actual original
  Training280531/26, source/GC/checkpoint rollback and pose preservation pass.
- First exact-APK Quest capture shows an arms-down NPC through the Training
  window; the user confirms "all npcs are fine." The final preservation-guard
  APK is automatically replace-installed with saves preserved and launched.
  See QUEST-TEST-BUILD.md and NPC-WAITING-PRESENTATION.md for exact artifacts,
  distinct initial/final verification evidence and remaining AI/scheduler work.
- Crate work resumes. The current original-data probe passes138 controls:
  damage6 survives with HP4; fatal damage rolls back at original PlaySound264
  (Active.TakeDamage PC464). Live breakage, contents/fragments and support
  gravity remain unfinished. Full tutorial/campaign completion is not claimed.

## Tracked held-item integration / quiet trigger (2026-10-15)

- User accepts the HUD and resumes items in hand. Trigger use no longer reveals
  or holds the quickbar; grip selection, keyring shortcut and Inventory remain.
- Original complete keyring/knife/crowbar hand-and-item assemblies are connected
  to actual equipped identity and tracked right grip. GPU resources are prepared
  once, generic right hand is replaced rather than doubled, and unsupported or
  stale selection safely falls back. Render performs no runtime/package access.
- Original typed melee base damage/range now feeds trigger-ray use; Fire states,
  skills, attack animation, physical swings, other held variants and crate
  destruction are not claimed complete. See HELD-ITEM-VISUALS.md.
- Coherent host build/CTest92 passes/106 registered,14 explicit owned-data skips,
  zero failures17.78s; actual equipped metadata2016/16 and cache1007448/42 pass.
  Signed APK built and automatically replace-installed with saves preserved;
  installed checksum matches. Exact-build Quest frames show keyring/crowbar in
  the tracked right hand; live selection resolves the knife too. Knife capture,
  physical alignment/comfort and trigger-only quiet-bar acceptance remain open.
  See QUEST-TEST-BUILD.md for artifact/hash, captures and verification bounds.

## Contextual quickbar, eye-facing tilt and separated speech (2026-10-15)

- New user priority: move original NPC portrait/text beside the health column
  instead of covering it,15% smaller with a shared top edge and positive gap.
- Quickbar stays at the requested lower center but now faces the eyes, with
  equal top/bottom edge distances. It starts hidden; equip/cycle/use/pickup and
  closing Inventory reveal it, then it fades after three idle seconds. Holding
  right grip/trigger keeps it visible. Hiding does not change equipment.
- Layout57930 and visibility405884/24 refusal checks pass; full CTest91 passes,
  14 explicit owned-data skips, zero failures,19.04s. Device acceptance is separate.
- Signed APK built, automatically replace-installed with saves preserved,
  installed checksum matched and game launched. See QUEST-TEST-BUILD.md for
  the exact artifact/hash and controls. Physical comfort remains unaccepted.
- Exact-build Quest frames show actual Jaime portrait/text beside unobscured
  health/compass in both idle and revealed-bar states. Final-frame diagnostics
  prove belt alpha1 on reveal and alpha0 when idle, with health/compass retained.
- Original keyring/knife/crowbar complete hand-and-item CPU assemblies and exact
  selection helpers are prepared/tested, not yet in the live renderer. See
  HELD-ITEM-VISUALS.md. Their integration remains deferred for this UI request.

## Installed HUD comfort revision (2026-10-15)

- Per the user's change of plans, pause hand-item work for a layout-only HUD
  adjustment. Health/compass move down/left and share the quickbar's left edge;
  the bar initially moved to27 degrees below the eye line. The user found that
  still too high, so the follow-up changes only the bar to about38 degrees down
  (bottom just below40). Original artwork scales, column placement, heading,
  health and equipment remain intact.
- Signed ARM64 APK built successfully and signature verified. Quest reconnected
  during verification; replace-installed without clearing data, installed hash
  matched and game launched. See QUEST-TEST-BUILD.md for the exact APK/checksum.
  The follow-up APK was also automatically installed/launched without clearing
  data. Software projection of the first revision was inspected; physical
  comfort acceptance of the much-lower bar remains pending.
- Placement/presentation240 and heading/view607 pass. Full CTest:87 passes,
  14 explicit owned-data skips, zero failures,11.81s.

## Installed health preview / playable quickbar (2026-10-15)

- The user revised the next test milestone: visible unfinished health plus a
  functional playable quick inventory bar/starter keyring, then APK/install.
  Original Pawn/Window possession is no longer a blocker for that test build.
  The separate native equipment model supplies real collected-item identities,
  selected state and persistent slots; its keyring aggregates collected keys.
  The health preview mirrors aggregate port health across six segments, with
  placeholder full energy. It is not independent limb/energy integration.
  The signed APK is installed and launched on the connected Quest; its installed
  checksum matches the finished build and existing saves were preserved.
  Pure quickbar205129, health/comfort141 and owned Training314 checks pass.
  The coherent host build/CTest passes87 tests with14 explicit owned-data skips
  out of101 registered, zero failures in16.76s; separately invoked original
  player/HUD6095 and VM694494/778 rejection controls pass.
  See QUEST-TEST-BUILD.md for the exact APK and controls, and HUD-COMPLETION.md
  for the broader unfinished original-HUD checklist. Headset acceptance remains
  the user's next test.
- The original1052 bridge now constructs its genuine pending Pawn-owned root,
  ActorDisplay, HUD, ammo, health, crosshair and pending belt with two GCs each.
  It executes original font/cursor/sine/alignment, hidden HUD sensitivity,
  canonical Color Context clearing, ammo visibility callback/size/assignment,
  all six limb textures/colors/requested layouts, the lowered body overlay,
  both ProgressBarWindows, crosshair query/layout and belt assignment. All ten
  Text/Button/Toggle quick slots construct with actual FontTiny, native
  selection/display tables and original sibling order. The new boundary is
  DamageHUDDisplay.InitWindow PC39, unsupported sIconInfo.DamageType struct-array
  String access, after132 objects/8783 writes. Audited IntToString0x53 now executes
  the ten augmentation-key labels. RadioBox/Tile construction, Modal registration, hidden
  visibility and original Tile margin/spacing/order setters now execute.
  A diagnostic checks fonts,
  clip rectangles, constructor flags/preserved CDOs, native modal self-list,
  all255 key/accelerator slots, family/GC edges, hidden-child Show, player binding
  and in-flight child locks before rollback. Expanded original health, Radio/Tile,
  oxygen-source, session/ring and forced-budget rollback controls pass6095 checks,
  including all ten F3–F12 augmentation labels and later configured belt geometry.
  ConfigureChild1461 now uses actual local visibility, scoped configuration
  callbacks, aligned-child ResizeChild and ClipTree/wGC propagation. Native
  Radio/Tile/Root configuration overrides and deferred destruction still refuse.
  Three new pure geometry suites pass130745 controls, including82368 independent
  x87 PC64 oracle tuples. The revised playable HUD passes a full ARM64 APK build.
  Native
  window subclasses/layout/lifetime/input/rendering and Login remain unfinished.
- Original limb-health presentation is connected to the real bound-player
  source: .4s damage,1s healing,.5s refresh, retained applied tint across original
  early returns, per-visible-frame presentation and changed-image uploads.
  Context/unbind resets never simulate health.357 offline checks pass; live oxygen,
  live damage/healing, themes/settings and on-device performance remain open.
  Actual typed head-zone/swim inputs now feed the original oxygen presentation
  state machine: first entry100%, subsequent clamped timer/duration arithmetic,
  exit/hide and no mutation under Tick gates.1067 controls pass; no timer or zone
  is fabricated. Progress height/color now follows integer-valued current state,
  and the original low-O2 modulo flash is covered, including negative finite
  timers. Shared HUD compositor21582 controls pass; live underwater gameplay
  remains unaccepted. Identical belt pixels are not re-uploaded for oxygen reads.
- Native argument-result seeds preserve NoWrite, partial scalar zero-fill,
  default hotspots, sequential copied inputs and OUT aliases with bounded
  allocation. Audited Sin187, sign-preserving Abs186, FMax245 and signed integer
  Max250/Clamp251 now execute. SetTileColor requires a complete canonical Color
  result write because its original temporary is uninitialized; direct-entry
  natives now apply actual audited defaults before snapshotting inputs. NewChild
  snapshots its class before a later bShow argument can mutate that source.
  Sequential seeds preserve alignment defaults based on earlier evaluated
  margins; no prefix overhead applies to ordinary natives.279 generated/12
  refusal controls pass; canonical Color761/115, uninitialized Color733/102,
  direct-entry defaults168/8, signed Max548/15, Tile inputs323/12 and expanded
  Window input-copy/zero-seed checks1016 pass, overall
  VM684788/684. Caught guarded-write failures now poison
  shared execution. Original PlayerPawn.ConsoleCommand's actual
  PlayerNone guard returns empty without executing a command; bound FExec and
  Actor.ConsoleCommand remain explicitly unsupported.355 generated/23 refusal
  and65 original/1 refusal controls pass; no original Windows code is executed.
- Qualified instance slots now keep hidden inherited fields separate from
  redeclarations, including saved declaring keys and warm/cold replacement.
  Actor.Region's PointRegion no longer aliases GameReplicationInfo.Region's
  server Int. Generated defaults/instance integration passes2149 checks/77
  refusals. Hidden inherited class-default writes remain explicitly unsupported.
- Persona menu correction, built/installed BEFORE the HUD gate, uses center
  Y=-.16m and an eye-facing plane shared by artwork/text/controller rays;
  207 placement checks pass. Latest installed APK is33,217,654 bytes,
  `artifacts/crate-runtime-20261013/DeusExQuestVR-persona-eye-facing-20261014.apk`,
  SHA256 `3516B8D72AF8A4B6575A38EB2D25E7F1A2465B521E8DA2BDFAA52D570D57BBD4`.
  Quest update05:45:53 and installed base.apk SHA256 are verified exactly.
  Exact corrected-build comfort remains unaccepted; no later APK was produced.
- Current host compilation and ARM64 syntax-only checks pass, including the
  actual Quest entry point and current runtime/VM; no Android binary/APK was generated by
  these syntax checks. Final CTest80 ordinary/93 registered/13 explicit
  owned-data skips passes with zero failures in30.08s (alongside original-data
  probes). Separate original
  ConsoleCommand checks and repeated-world/shared-budget rollback pass.
  Untouched Training now reaches BeginPlay slot126,DeusExMover16,
  Engine.Mover.BeginPlay PC166/Move266; that native remains unsupported and
  the entire probe rolls back. This is not accepted world/player startup.
  The full campaign/tutorial, NPC animation, gravity/crate fatal
  damage, held-item visibility and lamp illumination remain unfinished.

## Original compass / HUD and player prerequisites (2026-10-14)

- Training music and Jaime's speech are accepted by the user. Original compass
  artwork is now live, using actual Level-slot TrueNorth and rendered heading
  through physical yaw, snap turns, room rebases and Meta recentering. Captured
  Quest frame and COMPASS diagnostic verify original art/source/visibility.
  The user confirms direction works but the initial PC-edge placement was too
  far out. The replacement insets the left HUD column while preserving artwork
  scale, alignment and gap. A capture from the exact replacement APK confirms
  the inward shift; the user accepts the revised placement as comfortable.
- Original 541x69 ten-slot belt, 84x106 six-limb health pane, FontTiny, selection
  artwork and 162 bounded predecoded icons are prepared without pickup-time
  package reads. Production renders health/belt only from a real bound player;
  no live possession means they remain hidden, not healthy/ring fixtures.
  Full belt activation, ammo/counts, breath and health animation remain open.
- Added genuine transient native Client/Viewport UObjects, persistent native
  class identities, GC roots, lease/schema checks and transactional SetPlayer
  pairing with original dynamic Possess. Original CreateKeyRing/GiveTo/
  AddInventory/Idle2/base and warm/cold owned graph controls pass169 checks in
  a deliberately dormant test world. Actual Login/possession is not accepted.
- ClassContext now resolves distinct class-owned default objects, with typed
  result-destination/zero-fill/no-write semantics and native optional omission
  preserved. Audited FMin244 copies inputs sequentially and retains first signed
  zero on ties. Untouched world startup advances past GameInfo.DefaultPlayerName
  to Camera5's ConsoleCommand; isolated Possess advances to InitRootWindow1052.
  Both unsupported paths roll back, not publish partial gameplay.
- Host build passes; final CTest74 ordinary/86 registered/12 explicit owned-data
  skips, zero failures,13.10s. Separate generated HUD20571, heading/placement606,
  original artwork87, player/keyring169, VM678235/369 refusal controls, defaults
  1767/71 and diagnostic mocks26/0 pass. Focused original world-startup repeated
  failure/shared-budget rollback passes. No original Windows code was executed.
- Android build passes in1m9s. Signed ARM64 APK33,139,566 bytes:
  `artifacts/crate-runtime-20261013/DeusExQuestVR-original-hud-compass-inset-20261014.apk`,
  SHA256 `9653002F6CDE58C463EE0EF72D7C59B2532C5F19AA80BF723C26EDFE2733D026`.
  Automatically replace-installed preserving data on Quest2G0YC5ZG620985,
  update01:15:37, launch11070; pulled base.apk hash matches exactly. Final Quest
  capture shows the inset compass and subsequent stationary timing72fps;
  synchronous screenshot capture itself stalls. This is not route/gameplay
  performance acceptance. Current priority remains full HUD/keyring, then NPC
  poses/animation, gravity/crate destruction/held items; lamp illumination open.

## Training music milestone (2026-10-13)

- User priority one is implemented: the loaded map's typed LevelInfo.Song and
  SongSection select its original UMusic export. Training uses
  Training_Music.Training_Music (IT, section 0), title Training Room, 123.726s.
  No game music is bundled in Git or the APK. This is baseline map music;
  dynamic combat/death transitions, original fades/settings and saved cursor
  are not implemented. See TRAINING-MUSIC.md.
- The pinned libopenmpt decodes off the AAudio callback into a fixed 256KiB
  PCM ring. Focus/pause/map gating preserves playback position; the callback
  mixes ready samples with ambience and speech. Newer Vorbis symbols are
  isolated from Meta's older library, and MP3 implementation is linked once.
- Final host build passes. CTest: 72 ordinary / 82 registered / 10 owned-data
  skips, zero failures in 11.37s. Generated tests exercise ring refill/wrap,
  actual song looping, callback/input/gain bounds and both Vorbis libraries
  linked together. Separate original Training metadata/PCM/fingerprint test
  passes; diagnostic-tool mock suite passes 25/0. Original Windows code was
  not run.
- Final Android build passes in 2m13s. Signed ARM64 APK: 32,918,766 bytes,
  `artifacts/crate-runtime-20261013/DeusExQuestVR-training-music-20261013.apk`,
  SHA256 `8673B16C3F37B5771DC0470299786D4834BF2DCD6062B426C66388416FC536C1`.
  APK v2 signature and its 20 entries are checked, including four decoder
  license notices and no commercial packages. Automatic replacement was
  attempted but skipped because the known Quest is absent from USB; no
  alternate device was selected. On reconnect the music APK was replace-
  installed without clearing data on Quest 2G0YC5ZG620985, update 14:23:21;
  pulled base.apk hash matches exactly. One launch confirms process 8558,
  Training CPU preparation and AAudio 22050 Hz. Tracking then becomes valid,
  Training banks publish and the original IT section0 stream plays. The user
  confirms music and Jaime are both audible. A consumed MUSIC diagnostic
  reports stream present and zero underrun frames (focus inactive at the
  later check). Sustained playback/focus-resume and broader performance remain
  unverified; this is initial music acceptance, not full tutorial acceptance.
- Next priority remains the original full HUD, actual starting keyring and
  functional ten-slot belt, health and map-relative compass. Then NPC poses/
  animation, gravity/crate destruction/visible held items. Lamp illumination
  also remains open. No full tutorial or campaign completion is claimed.

## User-prioritized Training work and focus checkpoint (2026-10-13)

- Explicit work order: Training background music; full original HUD including
  starting NanoKeyRing/ten slots, health and compass; authored NPC poses and
  animation; gravity, breakable-crate destruction and visible held items.
  Lamp on/off illumination remains open, despite accepted focus labels.
- Golden original-font focus labels, typed object names/door states and original
  mover/non-mover sizing are implemented. Live Quest capture shows Unlocked;
  the user confirms Desk Lamp. Amber RGB is read from original packages.
- Native306 now scans live actual Level slots/direct Base with exact UClass
  ancestry and shared rollback/budgets. Both original Training crate DropThings
  empty scans and isolated damage6 -> HP4/ScaleGlow0.46 pass. Fatal hit rollback
  is verified at unsupported PlaySound264 PC464; no live destruction claim.
- Typed detached collected-item mesh/material/Int damage/range inspection
  passes original Crowbar metadata and CDO/instance isolation controls. It is
  not an equipped identity or a weapon renderer. Original lamp Frob refuses
  unsupported native264 at PC43 with exact property/save/revision rollback.
- Class field access now validates Class.Children/Next ownership, refuses
  orphan/foreign fields, and bounds complete ObjBase walks. Qualified type
  comparisons respect case-insensitive FNames regardless of display spelling.
- Host build, strict Quest syntax and Android build pass. CTest70 ordinary
  tests/79 registered/nine skips is green,25.37s; added fresh-process mixed-case
  test passes separately (80 registered after reconfigure). Final APK20,094,921
  bytes SHA256B07B838EA6BE257D3E8533ECE35C570C3371DA42DF86655E14FB25D7F46F8D76
  is installed data-preservingly on Quest2G0YC5ZG620985, update13:16:25,
  launch8979, with exact pulled base.apk hash match.

## Streaming safety follow-up (2026-10-13)

- Removing the actor currently awaiting a pose worker now invalidates its
  result and clears the build's pending slot without waiting/destroying the
  unfinished future. This prevents a final-actor pickup from leaving the bank
  permanently unfinished; the shared async helper has regression controls.
- Instant/zero-duration mover commands now queue one transient pose publication
  for the next positive elapsed update, including an already-Idle endpoint.
  Zero elapsed preserves the command; collision corrections immediately expose
  current samples without permitting a stale second block. No save format
  changed and restored state cannot replay the transient flag.
- Production host build, strict Quest syntax and Android build pass. Final
  CTest68 ordinary/76 registered/eight owned-data skips has zero failures in
  21.57s. Separate owned Training route plus generated instant-mover controls
  pass1,226 checks. Latest signed ARM64 APK is20,049,433 bytes:
  `artifacts/tutorial-streaming-20261013/DeusExQuestVR-training-streaming-safety-20261013.apk`,
  SHA256`DBE03808B1A46DCF5D4FD4144D35DFF03AC76D2D72C9269AF44C559C189C3BA2`.
  Initially skipped installation while USB was absent. On reconnect the APK
  was installed without clearing data to Quest2G0YC5ZG620985, update12:06:13,
  process8446; pulled base.apk matches the full hash above exactly.
- Read-only original-data investigation proves the stacked crates start with
  a39-unit/0.743m gap and PHYS_Falling; decoded bounds/pivots are correct.
  Original crowbar base damage is6, versus the old selection heuristic12.
  Actual crate damage needs its Active.TakeDamage/DropThings/Frag/Destroyed
  chain, not the Pawn-only adapter. BasedActors306 is its first concrete
  missing native. Held weapon, crate damage/contents/support physics, NPCs
  and original player/HUD remain open. See TRAINING-WEAPON-CRATE-GAPS.md.

## Retained streaming and visible inventory navigation (2026-10-13)

- The user tested the replacement build and reports freezes are basically fixed
  and inventory browsing now works. Navigation uses the actual visible layout,
  skips hidden keys/unavailable/unplaced rows, preserves selected identity,
  supports dominant-axis held repeat and tracked controller-ray selection.
  Original ItemName/Description now come from bounded reflected FString reads.
  An unsupported A/equip action no longer claims EQUIPPED; selected items still
  drive the older simplified attack path, not a real item-in-hand lifecycle.
- Ordinary pickup/death removal now masks actor-owned triangles in both active
  and staged banks without geometry re-upload, and invalidates targets/audio.
  Spatial rebuilds keep completed gameplay/collision inside retained coverage;
  authoritative restoration/material changes remain blocking. Movers reserve
  collider coverage first and rebase to their current poses before bank commit.
  Emitting removals retain the authoritative lighting-rebuild fallback.
- Live profiling caught 109.273ms all-mover snapshot reads at commit and
  116.01ms preparation. The final source/APK removes those repeat reads via
  an owner-thread cache seeded from authoritative snapshots and updated through
  every mover publication, including final idle samples. The cache's physical
  timing/door acceptance on the final APK is still pending.
- Full host build and strict NDK27 Quest syntax pass. Final CTest passes68
  ordinary tests/76 registered/eight explicit owned-data skips, zero failures,
  10.86s. New navigation75 and ownership/cache1,045 checks pass. Separate
  original three-weapon name/description/footprint metadata and original
  Training/UNATCOIsland/Intro inventory integration pass. Original-artwork
  software preview is visually inspected; it is a fixture, not live UI proof.
- Installed test APK is20,049,273 bytes, SHA256
  `ACC35F28EB61E8900A45391C3C94607EBEA7415809597AA7E030FD3BF605F0B5`,
  with exact pulled base.apk match on Quest2G0YC5ZG620985, update08:20:55,
  process22073. Final additional cache-fix APK is signed/build-successful at
  `artifacts/tutorial-streaming-20261013/DeusExQuestVR-training-streaming-retained-poses-20261013.apk`,
  SHA256`12D465000B70844516AD7AF6B5F40133FA7A4DCBC4E7CDCDD8424A957D865D76`.
  It is not yet installed: USB was absent at its build completion.
- Latest user test identifies invisible selected crowbar, breakable crates
  rejected by the NPC-only damage adapter, and an apparently floating crate
  stack. These, NPC lifecycle/T-poses and original player/HUD bootstrap remain
  open. See VIDEO-REVIEW-20261013.md and ORIGINAL-HUD-CONTRACT.md. No full
  tutorial, campaign, equipment, physics or performance completion is claimed.

## Receiver/context foundation and installed APK (2026-10-13)

- Audited opcode/frame receiver distinctions now cover Self, Let/LetBool,
  ArrayElement indices and ObjectToString, with 65 new checks/15 refusal
  controls. VM coverage is 678,060 checks/360 refusals. Context skip lengths
  are validated before effects, with exact diagnostics; zero-fill metadata is
  retained but result-buffer behavior and ClassContext remain unimplemented.
- Production-loader/AST census passes 1,184,728 checks over all 8,282 original
  sections, 110 contexts and 42 exact function/PC inventories. Full host CTest
  passes 66 ordinary tests (74 registered, eight owned-data skips), zero failures,
  29.58 seconds. State execution554/71 and timer1,469/47 pass after malformed
  synthetic context lengths and wrongly scoped fixture assignments were fixed
  to the verified retail shapes. Original Training/UNATCOIsland/Intro actor
  integration and the unchanged exact Training startup/rollback probe pass.
- Android Gradle/ARM64 build succeeds; strict NDK27 VM syntax passes. Signed APK
  `artifacts/class-context-foundation-20261013/DeusExQuestVR-receiver-foundation-20261013.apk`
  is replace-installed without clearing data on Quest `2G0YC5ZG620985`; installed
  base.apk matches SHA256 `5A382E5DEDA7771B4B3FF4036F0BF1FDACC5D75E0E763BE265F1DC9FD0D78746`.
  Tracked captures show original Jaime speech art, hands, all four door brackets
  during successful door motion, and textured desk items. Door publication is
  8.530ms mean/9.928ms worst over125 frames; streaming/readback stalls remain.
  This is not full tutorial/campaign or performance acceptance.
- The user's replacement recording reports streaming/pickup freezes, NPC T-poses
  and unusable equipment browsing. PC references additionally identify missing
  starting NanoKeyRing, ten quick slots, body-part health and compass HUD. These
  are the current priority. ClassContext,
  original startup/player ownership, automatic AI and the campaign remain open.
  See [CLASS-DEFAULT-CONTEXT-CONTRACT.md](CLASS-DEFAULT-CONTEXT-CONTRACT.md).

## Qualified property reads, source-only checkpoint (2026-10-13)

- VM read aliases now reject unrelated qualified property declarations before
  reading leaf-name storage. Legitimate inherited references remain valid when
  a derived class redeclares the same name. Authored default/map tags and mutable
  overlays are bound to their selected declaration instead of aliasing both
  fields. Retail Actor/PlayerPawn.bSinglePlayer demonstrates this distinction.
- Generated defaults integration passes 1,379 checks and 40 atomic rejection
  controls, including exact foreign-read diagnostics, failure-after-write
  rollback, inherited/redeclared tags, mutable default/instance isolation,
  canonical checkpoint round-trip and legacy reset. Strict NDK27 ARM64 syntax
  passes; the production host build succeeds. Fresh CTest passes 66 ordinary
  tests (73 registered, seven owned-data opt-in skips), zero failures, 58.40
  seconds; owned defaults/save/startup checks run separately.
- The unchanged fresh Training world probe repeats its exact ClassContext
  boundary with complete save/GC/birth/revision rollback. Original defaults
  passes 1,002 checks, and original Training/Combat save/rollback checks pass.
  Full original actor integration passes Training/UNATCOIsland/Intro.
  These are regression checks, not successful original world startup.
- A read-only census decodes 8,282 original script sections with zero errors:
  110 ClassContext expressions in 42 functions include default reads/writes,
  arrays, Bool wrappers, static calls and non-Actor UI classes. Retail static
  disassembly establishes distinct class-owned default blocks and destination-
  sensitive null behavior. See [CLASS-DEFAULT-CONTEXT-CONTRACT.md](CLASS-DEFAULT-CONTEXT-CONTRACT.md).
  ClassContext remains explicitly unsupported; no class-path proxy or guessed
  player name replaces the original callback. Shadowed-base writes, malformed
  orphan-property linkage and frozen-birth shadow controls remain unfinished.
- No APK is built for this backend checkpoint. The installed visual-test APK
  below remains unchanged, and physical desk/window/bracket/stair acceptance
  still needs a new recording. USB currently reports the known Quest offline.
  That USB observation is historical; the installed foundation checkpoint above
  supersedes it. Ignored diagnostics are under `artifacts/class-context-20261013/`.

## Original actor timers, source-only checkpoint (2026-10-13)

- Native280 SetTimer now validates the original declaration and reflected Actor
  schema, captures argument side effects in order, preserves signed rates and
  always resets Counter. An explicit `AdvancePortableActorTimer` phase consumes
  whole periods, retains the remainder and dispatches one current-state Timer
  callback with shared rollback. Timer-only writes persist without rebuilding
  geometry or resetting animation. This is not automatic world ticking.
- Retail arithmetic passes 20,343 checks, including 20,293 independent host x87
  PC64-oracle comparisons. Generated production-host integration passes 1,469
  checks with 47 atomic rejection controls. Owned original-script integration
  passes 1,743 total checks, including untouched PlayerReplicationInfo startup,
  the focused DataLinkPlay text-delay/state Timer path, Sleep isolation, actual
  animation-clock preservation, cold checkpoints and legacy reset. Strict NDK27
  ARM64 syntax passes. See [ACTOR-TIMER-CONTRACT.md](ACTOR-TIMER-CONTRACT.md).
- Fresh full host build succeeds. CTest passes 66 ordinary tests (73 registered,
  seven owned-data opt-in skips), zero failures, 20.47 seconds. The Spawn/AI
  suite passes 8,268 checks/143 rejection controls; review caught and corrected
  an AI-manager-only geometry-publication regression before this checkpoint.
  Raw execution-owned write controls bring VM coverage to 677,995 checks/345
  rejection controls. Original save/rollback, defaults1,002, PawnList67,
  Training route1,184 and speech-art12 controls pass separately.
- Full owned actor integration passes Training/UNATCOIsland/Intro; all seven
  optional owned-data suites complete separately. The rebuilt fresh Training
  diagnostic repeats its exact ClassContext boundary with byte-identical complete
  save/GC/birth/revision rollback. Logs are ignored under
  `artifacts/actor-timer-20261012/`; this remains a diagnostic failure, not an
  accepted started world or a possessed player.
- The unmodified Training startup diagnostic passes the previous native280
  boundary and now stops at PreBeginPlay slot1331 `00_Training.Camera4`,
  `Engine.Pawn.InitPlayerReplicationInfo` PC36/opcode18:
  `ClassContext requires class-default object identity`. The expression reads
  original `Engine.GameInfo.default.DefaultPlayerName`; no guessed value or
  skipped original callback is substituted. Full startup/player ownership,
  automatic AI and the campaign remain unfinished.
- No new APK is built for this source-only work. The installed visual-test APK
  remains the retained-bank/bracket/glass replacement documented below. Physical
  desk/window/bracket and stair acceptance still require the user's new recording.
- Older dated native280/decal boundaries below are historical and superseded by
  this exact diagnostic. Passing isolated timers is not a successful world probe.

## Original Training speech UI, target brackets and stairs (2026-10-12)

- Remote instructor speech now uses the original 431x114 InfoLink art, bitmap
  fonts and exact speaker portrait from the owned installation. Its typing and
  expiry share one clock, including tracking loss and long frames. Face-to-face
  cinematic conversation UI and transmission noise effects remain unfinished;
  see [DIALOGUE-UI.md](DIALOGUE-UI.md).
- Object focus uses the original white L-corners, black shadow and inset
  animation. The A-button and frame share the controller target and live door
  bounds. The complete original object-information/tool-strength panels remain
  unfinished; see [ORIGINAL-TARGET-FRAME.md](ORIGINAL-TARGET-FRAME.md).
- Capsule-footprint grounding and per-sample floor resolution replace the
  center-only stair test. Six real Training/UNATCOHQ routes reproduce the old
  ascent/descent failures and pass the new sweep, alongside wall, ceiling,
  ledge and failed-movement controls (26 checks). Horizontal map-space head
  movement freezes while actor colliders are being republished, preserving
  rotation and vertical tracking; ground/publication controls pass 141 checks.
- Door motion publishes only changed mover actors. An intermediate on-device
  opening ran 141 frames with two moving leaves, but publication still cost
  9.391ms mean/10.468ms worst. The follow-up removes a scan of the full loaded
  export population, using the live world-actor registry instead. The exact
  replacement APK publishes 130 paired-door frames at 8.369ms mean/8.946ms
  worst, with steady idle samples at72fps. Loading/refresh stalls remain;
  final-device performance acceptance is still required.
- Fresh full host CTest passes 64 ordinary tests (70 registered, six owned-data
  opt-in skips), zero failures, 24.57 seconds. Separate original speech-art
  checks pass 13 controls and render Jaime's actual introduction. Pure speech
  composition/timing passes 49,158 checks; 97 target-frame checks include the SDK
  uniform packing path. Original route/save acceptance and exact APK identity
  are tracked in [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).
- An intermediate installed APK produces a tracked live Quest world/hand frame;
  the user's video also shows original Jaime speech art. These do not verify
  corrected brackets or physical stair traversal. Review
  caught an SDK vec4-array upload limitation before final acceptance; all strips
  must use the SDK-supported matrix-array upload. The recording checklist
  remains open. No full tutorial/campaign completion is claimed.
- The user's recording exposes actor refresh pop-out and opaque static glass.
  Source/log correlation identifies full-bank deletion during >10m streaming
  refresh and baked BSP ignoring original translucent flags. Follow-up atomic
  bank publication and original BSP material support now preserve old visible
  generations and use exact original glass flags; device acceptance is pending.
  Shared bank lifecycle controls pass 36 checks (cancellation, scene/revision/
  atlas mismatch, stable renderer addresses, retained atlas lifetimes). Material
  controls pass 15 checks; actual Training window1942 is independently audited.
  Same-camera software before/after frames reveal the observation room/NPCs
  through the formerly opaque window. The exact replacement APK is installed
  and tracked; its inspected frame is from TrainingCombat, not desk/window or
  bracket acceptance. Two live retained-bank swaps complete atomically.
  The 25m/512-actor residency limits remain, not full campaign visibility; see
  [VIDEO-REVIEW-20261012.md](VIDEO-REVIEW-20261012.md).
- At this visual checkpoint the next original startup gap was native280 SetTimer. A fresh static
  retail audit establishes signed rates, >= expiration, one coalesced callback
  and Timer-before-physics ordering, which differ from the independent reference
  engine. The source-only timer increment above supersedes this boundary without
  claiming startup success; see
  [ACTOR-TIMER-CONTRACT.md](ACTOR-TIMER-CONTRACT.md).

## Training recording route and positive CPU decal attachment (2026-10-12)

- Current priority is the introductory recording route documented in
  [TRAINING-DEMO.md](TRAINING-DEMO.md), not a claim of full tutorial completion.
  Exact Mission00 named contact datalinks select original introductory speech
  and audio; conditional/complex graphs are refused rather than flattened.
  A bounded 16-line queue keeps contacts crossed during another transmission;
  it is not yet persisted. Compact gold subtitles remain visible independently
  of the debug panel.
- Supported authored mover keys now drive paired door motion, live rendering
  and player collision instead of hiding an activated brush. Matching collected
  NanoKeys permit keyed-door use. This automatic key use is a scoped VR
  convenience, not execution of the complete original key-ring/state-machine
  behavior. Closing player-blocked pairs reverse together. Inventory/loading
  pauses movement; v14 wraps existing runtime formats to save motion progress.
- Positive native CPU Attach now retains the original pretrace RNG, provisional
  primary/SurfList publication, mover SetBase callback and live postcallback
  texture/scale/raw Multi ordering. Native decal graph codec10/envelope13 is
  unchanged. Generated Spawn/attachment controls pass 8,268 checks with 143
  expected refusals. See [DECAL-GEOMETRY-PLAN.md](DECAL-GEOMETRY-PLAN.md).
- The unmodified Training startup diagnostic advances past Attach to
  PreBeginPlay slot1331, Camera4, Engine.PlayerReplicationInfo.PostBeginPlay:27,
  opcode97, unsupported native280. It remains a rollback-only probe: original
  world/player startup, rendered native shadows and automatic AI are unfinished.
- Fresh original-data route tests pass 358 checks, including exact voice/audio,
  both door pairs, blocking/reversal, mid-motion saves and malformed-state
  rejection. Original runtime/save tests pass cross-map preflight, corrupt-latest
  recovery, unchanged-world refusal and v14/v4/v5 composition. An inspected
  desktop closed-door frame has textured panels and static lighting; its actor
  preview has 64 mesh/25 brush instances and zero missing materials. This is
  software/static evidence, not live Quest door acceptance.
- Fresh ordinary CTest passes 59 tests (64 registered, five owned-data opt-in
  skips), zero failures, 25.17 seconds. Those skips are separate from the owned
  route/save runs above. Original defaults (1,002 checks) and PawnList (67 checks)
  pass separately. NDK27 strict syntax also accepts the final Quest main file.
- Full original actor tests also pass Training/UNATCOIsland/Intro. The signed
  19,983,537-byte APK is installed and its device SHA256 matches the archived
  artifact; see [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md). Fresh Quest captures
  show the original corridor/lobby, hands, lighting and compact control/status
  text; head/grips are tracked. The user confirms the introduction and opening
  door work. Intro contact being just ahead of spawn is authored behavior.
- Physical testing exposes remaining defects: a door-opening interval falls to
  43.3fps/166.66ms worst, despite 72fps idle samples; stairs remain impassable.
  Screenshot readback also stalls about291.65ms. Original speech-panel artwork/
  NPC portraits and interaction brackets are still missing. The demo is NOT
  fully accepted or recording-ready until these issues and the key route pass.
- Earlier dated entries below are historical;
  their old unsupported-Attach boundary is superseded by this entry.

## Native client, Quest target lifetime and original entry guards (2026-10-12)

- Added an engine-session-owned, nonserialized native client and bounded ordered
  viewport registry. Quest registers one actual logical stereo target after SDK
  resource creation, passing immutable metadata to the CPU runtime worker. An
  atomic owner lease is revoked by a new optional SDK hook before framebuffer
  destruction; global teardown follows existing worker joins. Tracking/focus do
  not fabricate target existence, and no PlayerPawn/Viewport.Actor is invented.
- Original Attach arguments are copied as each expression evaluates. Exact
  native/declaration validation now reaches the original absent/empty-client and
  TextureNone bool-false guards before Multi/RNG/geometry. Live nonnull Texture
  retains an explicit unsupported backend error. Client-dependent transactions
  recheck revocation immediately before publication, with full OUT/script/native
  rollback. GameInfo fallback now chooses configured DefaultGame versus
  DefaultServerGame by actual client presence, not viewport count.
- Strict registry controls116/39 rejected mutations, VM677971/338 (66 new Attach
  argument controls), generated Spawn/world/client7072/135 and SDK patch6/0 pass.
  All58 ordinary CTest entries pass (62 registered, four owned-data skips),33.50s.
  Original runtime-save, defaults1002, PawnList67 and focused Training world/
  decal checkpoints pass separately. The live synthetic-client startup still
  stops at positive Attach with exact full rollback. Full actor controls across
  Training/UNATCOIsland/Intro also pass; all four optional owned-data suites
  completed separately. Logs are ignored under `artifacts/native-client-20261012/`.
- The final signed APK builds and verifies; it contains no original commercial
  packages. Both actual builds skipped auto-install because the configured Quest
  was absent. The prior checkpoint APK remains the last device-verified build.
  No renewed visual, tracking, hand/body or performance acceptance is claimed;
  hand/body geometry and controller/recenter behavior were not edited. Deployment
  evidence is in [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).
- See [NATIVE-CLIENT-BINDING.md](NATIVE-CLIENT-BINDING.md). Positive Attach/render
  publication, original login/possession/UI, full world startup/campaign Tick,
  travel archives and full Quest gameplay/visual/performance acceptance remain
  unfinished. The new static reentrancy audit proves primary record/SurfList and
  mover SetBase precede live Texture/radius/Multi reads; the fixed-input geometry
  helper must not replace that callback ordering.

## Native decal ownership, Detach and checkpoints (2026-10-12)

- Added a map-owned native decal GC graph with actual Level/Model/record-owner
  roots, journalled mutation/rollback, native dirty-world revisions and guarded
  map retirement. Ordered records, duplicate/empty memberships and reflected
  SurfList mirrors retain legitimate missing/foreign-record behavior.
- Added bounded whole INT-array VM values and authored tag decoding. Original
  Const SurfList remains script-read-only but native-writable. Actual native-zero
  Detach processes the reflected list in reverse and preserves foreign records;
  original Destroyed dispatch uses the normal script/super path.
- Codec10/envelope13 stores arrays and explicit native checkpoints. Cold schema
  preflight verifies map/Level/root Model, counts, canonical Decal owners,
  membership surfaces and mirror agreement before rooted staging/publication.
  Legacy loads reset rather than replay Attach or RNG. Capacity-aware property
  decoding bounds raw bytes, payloads and simultaneous old/new record buffers;
  schema native Model scratch is separately bounded before decoding.
- Strict standalone VM controls pass677,905/338 expected rejections (new arrays
  96/36); codec controls7355/6482 and model/property controls450 expected
  rejections pass. All57 ordinary host tests pass (61 registered; four owned-data
  entries skipped by default). Runtime-save, original defaults1002 and PawnList67
  pass separately. The focused real Training checkpoint/world proof passes exact
  cold replacement, duplicate/mismatch retention, symbolic refusals, GC and
  post-native rollback. Its original startup still refuses AttachDecal with exact
  complete rollback. Full original actor controls across Training/UNATCOIsland/
  Intro also pass. Thus all four optional owned-data suites completed separately.
  CTest rerun takes30.49 seconds; logs are ignored under
  `artifacts/decal-runtime-20261012/`.
- Test-only corrections keep the cold-asset check before checkpoint collision
  rebuilds warm meshes and shorten a generated padding-property name. The
  inventory migration test still uses65,536 legal properties and rejects four
  derived Planes before publication;65,532 passes under the unchanged16MiB cap.
- The final signed APK automatically installed data-preservingly on Quest3;
  archived and device hashes match. One waited launch passes, but tracking was
  unavailable and the capture mailbox timed out without being overwritten.
  No renewed physical/visual acceptance is claimed. The headset disconnected
  after verified installation. See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).
- See [DECAL-CHECKPOINT.md](DECAL-CHECKPOINT.md). Native Attach, renderer
  publication, original world/player startup, possession and live campaign Tick
  remain unfinished. These changes are not evidence of rendered NPC shadows or
  complete campaign gameplay. Every actual new APK automatically replace-installs
  on the connected authorized Quest without clearing data/saves.

## Original multidecal geometry and ordered attachment state (2026-10-12)

- Added original authored BSP polygon selection, distinct primary/secondary
  plane-center behavior, ascending fresh surface-node cache and bounded
  insert-before-prune quad clipping. Primary quads retain all three original
  orientation paths, USize-only radius and exact random-draw consumption.
  Secondary geometry retains all sixteen default Shadow samples, signed-angle
  handling and the original two-line corner correction, not simple projection.
- Added a bounded geometry-only plan plus surface-owned ordered records and
  SurfList mirrors: first live-Texture insertion, repeated primary records,
  secondary uniqueness, reverse detach/first owner match and atomic copy/swap
  rollback. Review corrections included pre-call aggregate work limits, nested
  schema validation on misses and original ray normalization sum order. An
  accepted empty-membership regression preserves renderer-unrestricted records.
- Real Training evidence separates the BSP hit node1848/surface1837 from the
  actual containing coplanar node1852/surface1861. Original Shadow defaults are
  Multi4/DrawScale1/32x32 FlatFXTex40. The full geometry query produces one
  record/one membership node, six RNG draws and sixteen secondary duplicates;
  independent planar edge checks confirm the selected polygon. Focused original
  controls pass exact repeatability, standalone publication/detach and unchanged
  checkpoint/actor/revision/GC observations. See
  [DECAL-GEOMETRY-PLAN.md](DECAL-GEOMETRY-PLAN.md).
- All56 ordinary host tests pass (60 registered, four owned-data skips).
  Generated quad checks6195/25 expected refusals, surface649/77, state260/102 and
  composed projection79/8 pass. Android ARM64 syntax compilation with warnings
  as errors passes. All four optional owned-data suites pass separately:
  runtime-save, defaults1002, PawnList67 and full original actor controls across
  Training/UNATCOIsland/Intro. Focused startup retains the exact unsupported
  AttachDecal failure and full rollback. Logs are ignored under
  `artifacts/decal-geometry-20261012/`.
- This is not published native Shadow rendering. Truthful viewport binding,
  native graph/GC journalling, reflected dynamic SurfList, mover callbacks,
  checkpoint preflight/restore and Quest render publication remain required.
  The original startup probe still stops at AttachDecal; player possession,
  live Tick and full campaign remain unfinished. No new APK was produced for
  host-only work, so the accepted installed hand/body renderer is unchanged.
  Quest3 is connected; every actual new APK retains default data-preserving
  auto-install.

## Spawn tag fidelity and original shadow prerequisites (2026-10-11)

- Corrected the script/native Spawn boundary: native Class.Name is visible in
  every birth callback; captured optional SpawnTag, including None, is applied
  only after return and to a notification replacement when present. Native278
  copies ordinary inputs as each argument evaluates, before later expressions
  or owner/notification callbacks can mutate their source. Direct world
  GameInfo births retain native Class.Name. Generated and actual original
  WeaponPistol controls cover tags, exact rollback and cold-save restoration.
- Added a bounded, iterative original BSP point-trace prerequisite, audited
  against the original GOTY UModel recursion and wrapper: flags6/null-owner/
  zero-extent only, native LastNode/first-solid-leaf ordering, double threshold
  precedence, float store boundaries and half-unit backoff. Its owned Training
  ray finds node1848/surface1837; actual near-floor clamping and a raised ray
  pass without changing the runtime/save graph. This is not Actor.Trace,
  native AttachDecal or rendered shadows. See
  [MODEL-POINT-TRACE.md](MODEL-POINT-TRACE.md).
- The new original-binary [decal audit](DECAL-NATIVE-CONTRACT.md) establishes
  primary projection, ordered surface-owned records, duplicate SurfList,
  detach/GC, rendering and nonserialized native geometry. Multidecal, movers,
  viewport binding and restore policy still require implementation. The real
  rollback-only Training startup probe retains its exact unsupported
  Engine.Decal.AttachDecal first failure; there is no fake false/success bypass
  or accepted partial startup. Full original player possession/Tick and the
  campaign remain unfinished.
- All 52 ordinary host tests pass (56 registered; four owned-data skips), and
  all four optional original-data suites pass separately: original defaults
  1,002, PawnList 67, runtime save/restore and full actor controls across
  Training/UNATCOIsland/Intro. Point trace passes 943 checks/50 expected
  refusals; Spawn/world passes 5,598/116. The later-argument Tag alias bug was
  reproduced failing before its fix. Final CTest takes 7.04 seconds;
  auto-install controls pass 21/0 without hardware.
- The signed APK was automatically replace-installed on connected Quest 3,
  preserving saves/data; installed/archived hashes match and one waited launch
  confirms the app. The runtime worker changed; the accepted Quest renderer
  library, hand/body geometry and control/recenter mapping are byte-identical.
  Fresh eye captures show Training and original-style inventory; head/grips
  are tracked at 1.650m eye height. Overlapping hands in an uncontrolled pose
  are not renewed alignment acceptance. Inventory toggling was diagnostic,
  not physical-input testing. Uncaptured samples reach 72fps; screenshot
  stalls remain. Exact APK/deployment evidence is in
  [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).

## Transactional original-world startup and BSP regions (2026-10-11)

- Added one shared-budget VM transaction for a sequence of actual declarations
  and events. Later failures restore earlier OUT/property/native/RNG writes;
  catching a failed callback cannot commit its prefix. Coordinator-only errors
  and unresolved top-level event failures cannot inherit a completed script's
  function/PC, while nested failures retain their real active caller.
- Added a deliberately non-publishing original-world startup diagnostic over
  actual native Level slots: GameInfo selection/birth, all-world frames,
  InitGame, separate live actor phases, zone refresh and world attachments.
  Births/deletions are observed within each live pass. Every outcome rolls back;
  physical FindBase and original slot/save compaction remain explicit gaps.
  Native Mover birth, collision encroachment and SpawnNotify paths are refused
  where their original contracts are not implemented. See
  [WORLD-STARTUP-PROBE.md](WORLD-STARTUP-PROBE.md) and
  [WORLD-NATIVE-STARTUP.md](WORLD-NATIVE-STARTUP.md).
- Corrected BSP Region traversal at exact plane equality, child/leaf side
  pairing, empty-BSP leaf and zero-zone behavior. Fallback and InitExecution
  ownership use native XLevel slot0 instead of mutable reflected Level links.
  Original world zone refresh does not apply the older Spawn water shortcut.
- The unmodified 1,308-actor Training probe creates real DeusExGameInfo/Mutator
  objects and reaches PreBeginPlay slot756, JaimeReyes0, before the first
  observed unsupported native: Engine.Decal.AttachDecal. Repeated failure and
  short-budget controls restore exact save bytes, actor count/revision and GC
  totals. This is not accepted world startup, player login or live campaign AI.
- All51 ordinary host tests pass, and all four optional original-data suites
  pass when supplied the read-only installation. Generated world/Spawn checks
  pass5,398/114 expected refusals; VM transactions pass125/31, total VM677,809/302.
  The signed final APK was automatically replace-installed when Quest3
  reconnected during its build; installed/archived SHA256 matches. Default
  auto-install remains enabled for every new build. Exact APK and deployment
  evidence are in [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md). Fresh installed-APK
  eye frames show Training and the original-style inventory; diagnostic menu
  toggling and72fps samples pass. Controllers are untracked in these captures,
  so no renewed hand/body alignment or physical-input acceptance is claimed.
  Accepted player/body, controller/recenter geometry and app saves were not
  changed.

## Original startup primitives and pawn topology (2026-10-11)

- Added audited Log231, full validated ObjectToString0x56, original UTF16
  string literals/option helpers and Tan189/FClamp246/Min249. FString VM
  ingress retains ANSI zero-extension, surrogate/NUL storage and original
  slice quirks, with strict tagged-payload and transport-allocation bounds.
  Known pure-input natives detach inputs before later argument callbacks;
  real OUT aliases and rollback remain intact. See
  [STARTUP-PRIMITIVES.md](STARTUP-PRIMITIVES.md) for explicit fidelity limits.
- AddPawn529/RemovePawn530 now use actual native XLevel slot-zero ownership,
  unconditional prepend, first-match splice and retained nextPawn. Native
  links are journaled/saved as original typed properties. Actual Pawn.Destroyed
  owns removal; suppressed callbacks are not replaced with forced unlinking.
- Removed the invented Spawn AttachTag scan. The read-only original LoadMap
  contract establishes live-count phase loops, before-frame GameInfo selection,
  startup-bit timing, first-match world attachments and restored-world
  differences. Its actor-slot compaction and full world/player transaction
  still require implementation; see [WORLD-BOOTSTRAP-CONTRACT.md](WORLD-BOOTSTRAP-CONTRACT.md).
- All51 ordinary host tests pass (55 registered, four owned-data suites skip
  without arguments and were run separately). Startup VM controls pass333/29
  expected refusals; total VM677,684/271. FString ingress passes263,735/240,
  exhaustive ANSI and single-UTF16-unit coverage. Spawn/Pawn integration
  passes4,162/95. Original defaults pass1,002; original PawnList passes67.
  Original runtime saves and the full original actor suite pass Training,
  UNATCOIsland and Intro, including actual inherited InitGame on a controlled
  DeusExGameInfo birth, base Mutator creation, OUT behavior, rollback and saves.
  No checkpoint-seeded automatic world startup or campaign proof is claimed.
- Signed ARM64 APK builds and is archived as
  `artifacts/startup-primitives-20261011/DeusExQuestVR-startup-primitives-20261011.apk`,
  SHA256 `88008638C1EC7F2DD79C9C1FA66B6DE56D59595A424DB481E4FBBE22B90D9BE9`.
  Default automatic installation was attempted, but the previously connected
  Quest disappeared before deployment; repeat ADB/device checks confirm no
  physical headset. This APK is not installed or visually verified on Quest.
  App data/saves and accepted hand/body/control geometry were not changed.

## Original transition authoring and spawned frames (2026-10-11)

- Production GotoState uses modern controls and the original internal 0/1/2
  result, EndState recursion guard and BeginState preemption flags. The VM
  captures the wrapper name before argument expressions and detaches each
  Name input. Same-state wrapper relabel preserves masks; real selection resets
  them. Labels are searched after BeginState through selected SuperField
  ancestry; misses clear Code without erasing retained identities or throwing.
- Spawn captures Instigator before owner callbacks, publishes it after ownership,
  and initializes a class-backed frame after Instigator, even
  before begun play, with null Code/Locals and an all-one mask. Frame/HasStack
  changes share the birth journal. State selection/relabel retain local storage,
  timer and storage revision. Original package inspection found zero persistent
  State properties across all 261 States, and zero executable Class labels.
- Raw dormant probe overlays retain unknown native latent IDs. Audited clearing
  operations promote exact raw identities; ambiguous transitioned legacy frames
  explicitly refuse new control rather than guessing a selected identity.
- The shared backend passes all 50 ordinary host tests and the three separately
  invoked original actor/default/save suites. Coordinator/VM controls pass
  551 checks with 71 expected rejections. This is not automatic world
  startup, player possession, live NPC scheduling or full campaign completion.
  See [STATE-CONTROL-MODEL.md](STATE-CONTROL-MODEL.md).
- At the user's standing request, every successful new APK build installs on
  the connected authorized Quest 3. The Quest appeared after this APK build,
  and the new APK was replace-installed without clearing data. Hardware visual
  acceptance remains separate; the installed APK hash matches the archive.
  Offline install/lifecycle checks pass 21 and 30 controls.

## Independent state-control representation (2026-10-11)

- Modern frames now retain selected StateNode, running Node, nullable Code and
  positive live mask separately, with independent typed local storage. Raw
  serialized identities can be inspected without converting byte offsets or
  executing unsupported continuations. Modern/raw dispatch and state queries
  use selected ancestry; legacy overrides retain explicit compatibility.
- Codec9/envelope12 preserve modern controls alongside existing optional
  sections. Codecs1–8 keep their bytes/provenance; there is no guessed migration.
  Typed preflight checks ownership, runnable ancestry/PC/local schema, wait
  timers and version pairing before publication. Null Code retains stale PCs.
- Modern EX_Stop clears Code only. Slices use selected-state role eligibility,
  repeated deletion checks and the original fifth-observed-state-change yield
  with pre-poll capture. Modern probe toggles mutate one live mask.
- Production modern GotoState/label authoring still awaits original callback
  ordering/preemption and is explicitly refused on modern frames. Existing
  production transitions remain legacy. This is preparation for actual
  world/player startup, not live AI or campaign completion. See
  [STATE-CONTROL-MODEL.md](STATE-CONTROL-MODEL.md).
- All 50 ordinary host tests pass. Modern frame-codec checks pass1,632/833
  expected rejects; VM state checks499/63 and existing save-codec6,477/5,731
  pass. Generated original Doctor controls pass independent identity/mask,
  Stop, rollback, warm/cold/RNG and11 typed/version refusals. Original defaults
  and runtime save/restore pass; the full original actor suite also passes
  Training/UNATCOIsland/Intro. Signed ARM64 APK builds and is archived,
  not installed; USB currently reports no connected headset.

## Original state-dispatch correction (2026-10-11)

- Auto selection now returns the actual first Children/Next Auto-flag state,
  including inherited exact identities, without CompareIndex sorting, derived
  same-name interception or a literal Auto fallback. Current-state-name callback
  queries still treat Auto literally. Exact-node callback lookup follows actual
  state SuperField links, not inferred same-name superclass states.
- Detached metadata validates state parent ownership/depth/cycles, complete
  child order and cross-kind path uniqueness. Original source-link audit passes
  all 1,502 classes/261 states, 100 explicit state parents and 3,098 scoped
  callback queries without live script state. It shares package decoders and
  does not prove all native hash precedence. See
  [ORIGINAL-STATE-DISPATCH.md](ORIGINAL-STATE-DISPATCH.md).
- All 50 ordinary host tests pass (three optional original-data skips).
  Dispatch controls pass 63,597 checks/36 expected rejects. Focused original
  metadata/program, defaults, full actor and runtime save/restore checks pass separately.
  A signed ARM64 APK builds and is archived, not deployed; the installed APK
  hash still matches the user's accepted hand-direction build.
- The later independent representation above adds modern identity/mask storage;
  original production transition control remains unfinished. Legacy runtime
  calls retain their name-based context rather than passing running code as StateNode.
  This corrects shared lookup prerequisites, not automatic world startup,
  player possession, live AI or full campaign playability.

## Self-view geometry and session lifecycle repair (2026-10-10)

- The user's 41-second recording exposes genuinely open weapon-derived hands
  and the old body's cut-off waist. The new self-body restores 104 authored
  torso/chest/collar faces, for 251 total, omitting static arms/head. It honors
  original culling and uses 0.16 m render-only rear camera clearance; floor,
  tracked eyes, movement and saved player position are unchanged. Near-vertical
  gaze retains a stable render-body yaw rather than flipping its rear offset.
- Hands retain 152 original NanoKeyRingPOV hand/sleeve faces and add four
  explicitly derived rear-cuff closure triangles. Bounded closed-manifold
  checks reject other holes/nonmanifold or inconsistent topology. The original
  source supplies the right grip; a grip-local X reflection supplies the left.
  The earlier extra source reflection was incorrect and physically reversed
  the wrist direction. Captured controller axes establish cuff +Y toward the
  wearer, not -Y; actual-pose regression checks now guard this distinction.
  Source IDs, original UVs, materials and normals/winding remain paired.
- Four native-size JC/hand textures are retained without resampling. Desktop
  front/back/top/underside diagnostics cover the original textured derivative;
  these do not establish physical grip alignment, stereo comfort or animated
  player ownership. See [PLAYER-VR.md](PLAYER-VR.md).
- Independent Persona rail auditing found 439 lower-left original border texels
  matching the shared canvas and full CPU preview exactly. Other sampled outer
  rails also match. They are authored decoration and are not cropped away;
  theme/blending fidelity, actual inventory initialization and input remain
  separate requirements.
- The SDK lifecycle patch removes only Android resume/pause queue-order
  assertions; real OpenXR READY/begin, STOPPING/end, active-session assertions
  and checked errors remain. Launch/install helpers use one waited launch and
  exact authorized serial selection while preserving app data. Offline failure,
  fresh-patch, idempotence and conflicting-edit tests cover both PowerShell
  versions. See [BUILD-DEPENDENCIES.md](BUILD-DEPENDENCIES.md).
- All50 ordinary host tests pass (three optional original-data integrations
  skipped); separate original player proof passes1724/25 expected rejects and
  hand proof147/30 synthetic plus1705/34 original+preview controls. Transform
  tests pass17 groups. The signed ARM64 APK is installed and its actual on-device
  hash matches the frozen archive; app data was preserved. With tracking active,
  the user accepted the body but rejected the earlier hand alignment (fingertips
  toward self). A subsequent diagnostic measured the wrong wrist direction;
  the corrected build was then captured with both grips tracked and received
  the user's "Yes, directions match" physical acceptance. The body remained
  unchanged, with1.650m eye height and four submitted body surfaces. One Home-return cycle
  survived without assertion but did not reproduce the narrow READY-before-
  Android-resume ordering. See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).
- This remains render-only self-view geometry, not finger/arm IK, walking
  animation, held weapons, a possessed original player or full campaign startup.
  The backend/world-simulation boundaries below remain in force.

## Original landing wait and positive authored probes (2026-10-10)

- WaitForLanding527 always retains the original shared2.5s native counter;
  only Falling installs poll528. Nonfalling polling releases without changing
  that counter. Strict negative timeout dispatches actual LongFall within the
  same interpreter/rollback boundary and respects callback frame replacement,
  deletion, budgets and RNG. No physical falling/landing is fabricated.
- The actual original LongFall commits286 instructions into FallingState's
  Sleep at PC20 (0.7s, next28). The isolated original StartUp/inventory fixture
  now reaches Standing's authored FRand()*14+8 Sleep at PC304 (next319), retaining
  the two-object weapon/ammo graph. This is not automatic campaign startup.
- Enable/Disable now require a real owned frame and one of the original64
  probe names. LongFall/custom names are no-ops, including saved nonprobe
  disable records. Original Serialize/IsProbing evidence corrects the inherited
  backwards package mask: dormant frames use actual positive enabled bits,
  independent of class-mask recomputation; frameless calls permit all probes.
  Raw-frame negative overlays preserve real enables/disables. Full independent
  selected-StateNode/running-Node/null-Code/live-mask transitions remain pending.
- Codec8/envelope11 retain landing waits and their native timer without changing
  old wire bytes. Typed preflight requires actual Pawn/timer evidence, including
  null-code waits. Legacy counter reset, warm/cold continuation, invalid-state
  rejection and complete transaction rollback are covered.
- All49 ordinary host tests and all three separate original-data integrations
  pass. State controls pass473 /58 rejections, save-codec6477 /5731,
  dispatch63562 /20, generated lifecycle1353 /14 and original defaults998 checks.
  The corrected actor suite explicitly disables AnimEnd, instead of relying on
  the former inverted authored-mask interpretation.
- The final corrected landing/probe APK was installed with app data preserved
  and its on-device hash checked. A fresh final-build capture confirms Training
  rendering with valid head tracking and automatic seated calibration. Its
  upward view has no tracked controller grips and does not accept body alignment.
  Training/UI/body frames were also captured on the earlier landing APK.
  Automatic seated height reached1.650m and feet match the actual floor.
  Hands/lower-body surfaces render, but this earlier build retains open
  hand/waist geometry. Its UI edge rails have since been verified as original
  authored decoration. Idle Training reports72fps; screenshot
  readback causes large spikes. These are scoped checks, not visual/performance
  or campaign completion. See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).
- Production Quest still has no real GameInfo/world startup, player login/
  possession or automatic actor Tick. Camera/custom gameplay containers are
  not a running original campaign. Next integration: correct shared original
  state identities/control, then staged map-wide bootstrap and actual player
  ownership, before enabling world simulation. See
  [WORLD-STARTUP-GAPS.md](WORLD-STARTUP-GAPS.md) and
  [SCRIPT-LANDING.md](SCRIPT-LANDING.md).

## Original Sleep and explicit wait continuation (2026-10-10)

- Native Sleep256 retains raw signed finite seconds in actor-owned native
  storage. Polling uses the original strict half-frame wake boundary and signed
  remainder, without clamping or reusing elapsed time for a newly reached Sleep.
  Actual Pawn.StopWaiting native0 writes-1 only while sleeping and defers wake.
- A no-time state resume yields committed Waiting. Explicit wait polling and
  continuation share one interpreter/root transaction, including timer, PC,
  locals, callbacks, RNG and native/birth effects. Unknown waits and persistent
  iterator continuations still refuse and roll back; no full actor Tick is enabled.
- Supported state changes and successful labels now cancel waits according to
  original Core.dll evidence, including an in-code Sleep label expression.
  Native actor timers survive state replacement, Stop, null code and frame absence.
- Codec8/envelope11 retain those counters with optional RNG and all earlier
  sections. Sleeping frames require timer evidence; typed validation never
  polls, and legacy envelopes1–10 reset omitted counters. Old codec1–7 bytes
  remain unchanged without timers. Cold wait/RNG restoration is covered.
- Actual StartUp on the isolated generated weapon-slot fixture now commits
  original weapon/ammo initialization and its Sleep at PC6. Waking
  reached required WaitForLanding527 at DeusEx.ScriptedPawn.StartUp PC18,
  opcode0x62 in this earlier increment; the landing increment above implements it.
- All49 ordinary host tests pass. State controls pass296 /40 rejections; save
  codec controls6467 /5731. All three separate original-data integrations pass,
  including the full actor suite's Sleep/cold restore and inventory wake rollback.
  Android ARM64 and APK v2 signature verification pass. Evidence is in
  `artifacts/sleep-state-20261010/`; see [SCRIPT-SLEEP.md](SCRIPT-SLEEP.md).
- Automatic world/player startup, full original state/frame identity and tick
  ordering, physics/AI, campaign progression and device acceptance remain
  unfinished. This backend APK was initially archived without installation,
  then installed at the user's request and superseded by the landing APK above.
  Remaining original state-control distinctions still precede world scheduling.

## Original script RNG and saved continuation (2026-10-10)

- Rand167/FRand195 use an explicit MSVCRT-compatible shared uint32 stream,
  characterized from the original Core.dll: nonpositive Rand consumes no draw,
  Max1 consumes one, large bounds are not clamped and FRand includes exactly1.0.
  Actual scripted RandRange and original Bird Idle1/Idle2 branches execute.
- Root transactions roll back random draws with nested properties, completed
  OUT writes and native clocks. RNG-only commits do not republish geometry or
  block map travel; existing map-scoped actor/default/AI archive guards remain.
- Codec7/checkpoint10 retain the global stream, including nonbaseline seeds
  before the first draw. Quest supplies time once; portable tests default to1.
  Warm/cold restoration continues exactly; legacy envelopes1–9 explicitly reset
  to seed1. Unsupported algorithms or malformed typed state reject atomically.
- In this RNG batch, explicit original StartUp passed FRand and refused Sleep256 at
  DeusEx.ScriptedPawn.StartUp PC6, opcode0x61, with full rollback. Latent scheduling,
  automatic world startup, live AI and campaign completion remain unfinished.
  The subsequent original-Sleep increment above implements that wait.
- See [SCRIPT-RANDOM.md](SCRIPT-RANDOM.md) for contract, tests and limits.
  This backend APK is not installed; the accepted seated-player build is unchanged.
- All 49 ordinary host tests and all three separate original-data integrations
  pass. RNG controls pass 58,942 / 37 rejections; codec controls pass 6,337 / 5,634.
  Android ARM64 build and v2 signature verification pass without commercial
  packages. Local evidence is under `artifacts/random-state-20261010/`.

## Original Switch/Case interpreter path (2026-10-10)

- Original Switch/Case now supports alias-retaining selector evaluation,
  selector-directed typed comparisons, absolute Case links, default, fallthrough,
  breaks and nested control flow. Generic Struct comparison refuses explicitly
  rather than inventing an original layout. Structural validation precedes effects;
  cyclic links share the existing execution budget and root transaction.
- Explicit state slices follow callback-changed live code/position, retain
  same-state/inherited-label locals and reject destroyed local aliases or
  replacement with live iterators. This is not automatic world scheduling.
- The unchanged DeusEx.ScriptedPawn.PlayTurnHead override passes directions
  0–4/default255, exact +/-5461 view offsets, native blend/timer effects, its
  actual false Bool return, short-circuiting and save/budget rollback.
  In this increment StartUp reached unsupported FRand195 at its original PC9,
  with full provisional inventory/AI/world rollback. The later RNG increment
  above implements it; startup is still incomplete at Sleep256.
- See [SWITCH-CASE.md](SWITCH-CASE.md) for source contracts and final evidence.
  The physically accepted seated-player APK remains installed unchanged.
- All 48 ordinary host tests and all three separate original-data integrations
  pass. Structural audit covers all 8,208 functions, 262 Switches and 1,599 Cases;
  this is not general execution feasibility. VM controls pass 677,351 / 242
  rejections, with 420 / 58 specific to Switch; state controls pass 208 / 32.
  The ARM64 APK builds and verifies with v2 signing and no commercial packages.

## Original blend-animation commands (2026-10-10)

- Native1010 PlayBlendAnim and native1012 TweenBlendAnim now mutate actual
  four-channel animation clocks and reflected SimBlendAnim Plane properties.
  Read-only original GOTY binary characterization corrects signed Int slots,
  exact sequence lookup, default tween=-1, negative tween frames, previous-rate
  selection and X/Y/Z/W packing. The pinned wrapper is incomplete here.
- The unchanged Engine.Pawn.PlayTurnHead helper completes all four directional
  head turns in isolated original-data tests. Main animation remains independent;
  tween history, rollback and current-map clock restoration are retained.
  Older portable clocks derive the previously omitted reflected Plane view;
  partial/disagreeing views reject. No save wire-version change is introduced.
- In this blend increment, explicit original StartUp passed native1010, then refused Switch opcode5
  at DeusEx.ScriptedPawn.PlayTurnHead PC52 with complete inventory/AI/world
  rollback. The later Switch/Case increment above implements that dependency;
  neither increment fabricates startup success. See [BLEND-ANIMATION.md](BLEND-ANIMATION.md).
- An actual native-command character capture exposed missing texture paths in
  the lazy mesh cache. Animation queries/commands and map-wide decoding now
  publish through one complete loader. True-cold original controls check both
  paths, exact material references, drawable poses and transaction/save rollback.
  Inspected native-command Doctor/Jaime software captures now show complete
  textured bodies with zero missing material selections at the unchanged gate.
- This is a backend increment, not live animation, a full player rig, working
  NPC AI or campaign completion. The physically accepted seated-player APK
  remains installed unchanged; no new headset performance acceptance is claimed.
- All 48 ordinary host tests and all three separate original-data integrations
  pass on the final production source. Pure clock controls pass 200; legacy
  import boundary validation accepts 65,532 properties with four free slots and
  rejects 65,536 before adding the reflected view, preserving checkpoint/GC.
  The final ARM64 APK builds and verifies with v2 signing and no original game
  packages. See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md) for the local archive.

## Native AI event-state foundation (2026-10-10)

- Implemented actual LevelInfo.InitEventManager650 and Actor event natives
  710/711/713/714/715/716: ordered registrations, deferred-deletion markers,
  perception flags, persistent/pulsed sensory channels and sixteen history
  slots. An absent manager retains the original wrapper branch; actual native
  Level ownership is independent of reflected Level/XLevel overlays.
- Native manager GC ownership and a separate transactional journal preserve
  bindings/actor references through callbacks, failures and cold graph restore.
  AI-only changes do not republish geometry. Codec6 / checkpoint9 retain empty
  manager presence and full ordered native graph snapshots with bounded,
  typed symbolic preflight; legacy saves clear managers atomically.
- Corrected cold-born native XLevel restoration: reflected None stays None,
  but actual native event routing keeps the constructor's serialized Level.
- Original UpdateReactionCallbacks now reaches its actual Return PC484, and
  SetDistress reaches Return PC57 while retaining emission history. Explicit
  begun-play BeginState also completes its original property/state effects and
  real BlockReactions deletion. This does not initialize the world automatically.
  In this earlier increment, explicit StartUp with a real initialized manager
  advanced beyond registration and refused native1010 in Engine.Pawn.PlayTurnHead
  PC211/opcode0x63, with complete graph/inventory/world rollback. The subsequent
  blend increment above advances to Switch; neither is automatic campaign startup.
- Registration is not working NPC AI. Processing/perception/callback delivery,
  native deferred cleanup and automatic world/player startup remain unfinished.
  Original manager destruction notification occurs in CleanupDestroyed, not
  synchronous Destroy279. Original saves normalize deletions; portable graph
  snapshots explicitly preserve them. See [AI-EVENT-STATE.md](AI-EVENT-STATE.md).
- Final host suite: 48 passes / three optional original-data skips (51 entries).
  All three separate final-source original actor/runtime/defaults integrations
  pass. Generated runtime integration passes 3,485 checks / 77 rejection
  controls; AI mutation tests pass 188 / 51 and codec controls 6,128 / 5,574.
- Final ARM64 APK builds and verifies with signature scheme v2; 16 ZIP entries
  contain no original commercial packages. It is archived locally, not installed
  over the physically accepted seated-player build. This batch has no new
  headset performance/visual acceptance. See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).

## Original actor lookup and synchronous foreach (2026-10-09)

- VM Iterator/Next/Pop now execute real native foreach loops with independent
  nested cursors, guarded OUT aliases, transaction rollback and bounded work/
  storage. State loops can complete in one synchronous slice; persistent
  cursors and latent/code/local replacement with live cursors explicitly refuse.
- AllActors304 scans actual live Level slots, including holes, appended births,
  live Tags and Destroy callback timing. GetPlayerPawn720 implements the linked
  current-Level PlayerPawn fallback, not an owned viewport or full Possess.
  Native221 Vector*=Float and native225 VSize support the original helpers.
- Generated inactive-member controls exposed a pre-existing reference-value
  rejection. Valid inactive UObject references can now be assigned and saved;
  identity/class constraints and direct inactive receiver guards are retained.
  Scalar/array/struct/typed OUT, cold restore, GC and rejection controls pass.
  Inactive Context dereferencing and function-local object ancestry enforcement
  remain explicit limitations. See [ACTOR-LOOKUP.md](ACTOR-LOOKUP.md).
- Final host suite: 47 passes / three optional original-data skips (50 entries).
  All three separate final-source original actor/runtime/defaults integrations
  pass. Generated Spawn/lookup integration passes 2,384 checks / 60 rejection
  controls; VM controls pass 676,927 / 184. Original FindTaggedActor matches an
  independently serialized-order nearest-distance oracle (19 AmbientSound
  candidates, result AmbientSound26), and InitializeHomeBase reaches Return
  PC186 with its actual 100-unit HomeRot. Read-only saved bytes/revision/GC stay
  unchanged. Oracle membership uses recognized runtime snapshots; generated
  fixtures independently cover native slot publication.
- Actual StartUp now refuses native711 AIClearEventCallback in
  ScriptedPawn.UpdateReactionCallbacks PC41/opcode0x62. Full saved payload,
  provisional inventory births, roots and world publication roll back. The
  native AI manager must be implemented, not replaced with pin-stub no-ops.
- Final ARM64 APK builds and verifies with signature scheme v2; 16 ZIP entries
  contain no original commercial packages. It is archived locally, not installed
  over the physically accepted seated-player build. Full original player/world
  startup, AI/latent/timers, campaign archives, physics and device validation
  remain unfinished. See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md).

## Real actor births, collision registration and cold graphs (2026-10-09)

- Native Spawn278 creates actual loaded Actor classes with frozen birth
  defaults, original callback/zone/base/notification order and transactional
  rollback. Cached actor collision registration now supports SetCollision262,
  SetCollisionSize283 and Spawn's InitBase overlap path; it is not general
  movement physics, Touch/encroachment or a complete startup scheduler.
- Codec5 / checkpoint8 preserve real born identities, exact appended Level
  slots, frozen typed defaults, instance/state/clock overlays and native links.
  Cold graph replacement and legacy birth-tail clearing are atomic and tested;
  provisional and retired birth allocations are collected at API boundaries.
- Commit/load prepare newly referenced CPU geometry. Quest retires actor chunks
  before staged replacement texture upload; cold-map load prepares materials
  after save restoration. Case-folded aliases retain masked bindings, and
  unavailable assets preserve per-object fallback. ARM64 compilation passes;
  replacement-GPU correctness and owner-thread atlas frame cost remain untested
  on the headset. The accepted player/body APK remains installed unchanged.
- Final host suite: 47 passes / three optional original-data skips (50 entries).
  Separate full original actor, runtime and class-default integrations pass on
  the owned installation (101,375 script objects; 996 defaults/lifecycle checks).
  Generated actual-VM Spawn controls pass 1,416 checks / 37 refusals; cached
  overlap controls pass 6,078 / 18; structural codec controls pass 4,480 / 4,133.
- Unchanged original InitializeInventory reaches its real Return at PC770,
  creating WeaponPistol + Ammo10mm and their owned/base/Idle2/native-link graph.
  Cold v8 restoration and GC pass. Actual StartUp next stops at native720
  GetPlayerPawn in ScriptedPawn.FindTaggedActor PC61/opcode0x62 with full rollback,
  not fabricated startup success. Inputs for this positive inventory test are
  explicitly generated; a complete authored player pawn is still required.
- Returned-actor visual diagnostics exposed the close-up camera discarding
  pitch. Corrected yaw/pitch orbit passes 41 controls. The inspected elevated
  GlockPickup frame shows textured barrel/trigger/grip and passes the unchanged
  1% coverage gate at 2.0434%; initial edge-on failures remain retained. This
  single CPU fixture is not Quest GPU or original-renderer equivalence. See
  [ACTOR-SPAWN.md](ACTOR-SPAWN.md) and [ACTOR-VISUAL-TESTING.md](ACTOR-VISUAL-TESTING.md).
- This supersedes earlier Spawn/reflected-collision limitations below, not
  completion criteria. Automatic authored player/world startup, AI/timers/latent
  behavior, campaign travel archives, general physics, live GPU animation,
  campaign progression and device performance remain unfinished.

## Actor ownership, attachment and deletion (2026-10-09)

- Original SetOwner272/SetBase298/Destroy279 now run real synchronous callbacks
  within the caller's VM transaction. Native ordered lists, touch-event flags
  and world removal are journalled separately from property writes. Deleted
  actors remain directly callable/readable but are excluded from both world
  snapshot modes. Original serialized Level actor order drives initial bases.
- Checkpoint v7 / codec v4 preserve exact native topology, including reentrant
  duplicates. Read-only validation and atomic replacement pass. Legacy v1-v6
  resets restore the original map baseline; no callbacks are replayed.
- Committed world revisions refresh Quest actor geometry/targeting/lighting
  inputs and ambient membership. Restored map clips are reused without XR-thread
  decoding. Static BSP/lightmap atlases and actor collision physics are not
  rebuilt by this path. Android compilation passes; the new backend APK is
  frozen locally, not installed over the accepted seated-player build.
- Final host suite: 45 passes and three optional original-data skips (48 entries).
  Separate original actor/runtime integrations pass. Generated lifecycle tests
  pass 1,243 checks / 14 rejections; codec tests pass 4,209 / 3,948. Original
  defaults/lifecycle integration passes 996 checks on 101,375 script objects:
  natural AssaultGun destruction followed by its actual CDO write, v7 saves,
  GC/direct-call lifetime and nested rollback; explicit enabled/begun callback
  adds exactly the original Weapon/Inventory Destroyed's 21 instructions.
- A second physical cold start calibrated seated eye height automatically at
  1.650 m; the user confirmed correct height and legs without left-stick input.
  The installed accepted player build remains unchanged.
- Actual Spawn278 allocation/frozen birth defaults/manifests, automatic startup,
  authored player inventory, AI/timers/latent actions, campaign archives/travel,
  full physics and campaign playability remain unfinished. This supersedes
  earlier Destroy-refusal notes below, not the full-game completion criteria.
  See [ACTOR-LIFECYCLE.md](ACTOR-LIFECYCLE.md).

## Concrete class-default startup dependency (2026-10-09)

- Original `DefaultVariable` writes now mutate the concrete loaded Actor
  class's own default block, with typed metadata validation and transaction
  rollback. Existing actor instances and loaded sibling/child defaults remain
  unchanged. No instance substitution or original callback suppression is used.
- Runtime checkpoint v6 / script codec v3 retain concrete-class patches
  separately from authored actors. Class-only captures, complete replacement,
  omitted-class reset and legacy v1-v5 clearing are supported. Combined
  persistent default/state retained bytes and value nodes are bounded.
- Native283 publishes original collision radius/height assignments and returns
  true, matching the pin's reflected contract. Actor collision-hash reinsertion
  and full physics remain unfinished.
- All 44 ordinary host tests pass; three original-data integrations skip in
  the default 47-entry CTest invocation. Separate original actor and runtime
  regressions pass with the changed source. Android ARM64 builds successfully;
  the signed backend-only APK is retained locally, not installed over the
  accepted player-controls build. See [CLASS-DEFAULTS.md](CLASS-DEFAULTS.md).
- The complete original AssaultGun `PreBeginPlay` passes with explicitly
  generated singleplayer/listen-server relevance preconditions: original height
  adjustment, concrete default ammo count, existing-instance/base/sibling
  isolation, v6 restoration and nested rollback. This is a scoped callback
  test, not automatic world startup. The subsequent lifecycle batch above
  additionally supports the unmodified dormant map's Destroy279 continuation.
- Real actor births, persistent birth manifests,
  automatic startup, AI/timers/latent actions, authored player inventory and
  complete campaign progression remain unfinished. This is not a full-game
  release or a newly verified on-device gameplay change.

## Quest player/body, input and original UI increment (2026-10-09)

- Original JC trousers/shoes/lower coat and original weapon-viewmodel hand
  surfaces load from the user's packages. Hands follow tracked controller grips;
  the left is a documented mirrored derivative. Own-body back-face culling is
  disabled for the overhead self-view. No full-body IK, arm/finger animation,
  animated walking/crouching, held weapon model or authored player pawn exists.
- Debug HUD menu and SDK auto-layout parent offsets are removed. Actual Quest
  submitted center is head-local `(0,0,-1.05)` and matches projected head forward.
  Left grip hides/shows it. LOCAL_FLOOR reference tracking is used when supported;
  the user confirmed physical Meta-button recenter works. Stick deadzone and
  navigation/turn hysteresis have bounded host regressions.
- Explicit seated calibration targets a 1.65 m comfort eye height while retaining
  real head/controller motion. The app remembers mode separately from saves.
  Reference changes and posture switches preserve map-local feet, including
  standing vertical-origin changes. Collision cannot use partially uploaded
  meshes. The first seated device result was rejected (too low / missing legs);
  corrective culling/floor build plus explicit recalibration was accepted by the
  user (height and legs correct). The final cold start automatically calibrated
  at 1.650 m after loading, with no manual command; its downward capture shows
  hands/lower body and feet on the actual Training floor. This is one device
  acceptance, not complete body animation or all reference-runtime cases.
- Inventory retains the original 5x6 grid, original multi-cell icon dimensions
  and whole-footprint highlights. Read-only bounded authored item descriptors
  and exact qualified texture references supply artwork; no placeholder icons
  or mutation of authored inventory coordinates is used.
- Rotator-to-Vector uses the pinned original Coords semantics with rollback.
  Full ordinary host suite passes 43 tests; two optional original-root entries
  skip in that invocation. Separate original-data runtime, actor/descriptor,
  player-assets and spawn-placement differential checks passed. Original spawn
  placement compares 167,997 controls against the pinned hull oracle, but is
  still not connected to actor allocation/lifecycle. InitializeInventory still
  stops at unsupported Spawn278 PC253 and begun-play needs AIEndEvent715.
- Prior offline status sections below are historical. Current Android/device
  build evidence and remaining acceptance limitations are in
  [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md) and [PLAYER-VR.md](PLAYER-VR.md).

## Offline bounded table reader and spawn-placement prerequisite (2026-10-09)

- Selected a 64 KiB read-only adapter only for package tables, retaining logical
  cursor/EOF/fault behavior and keeping the raw source private. Pinned vendor
  files and other payload readers are unchanged. All 37 ordinary tests pass;
  two optional original-root entries explicitly skip in CTest.
- A separately linked frozen raw reader and buffered reader agree on every
  individual metadata digest for all 38 System packages and three original
  maps. Measured complete loader-call sums were 22.930990 s versus 0.384153 s
  on this PC under uncontrolled OS-cache/concurrent-load conditions. This is
  not Quest performance or full startup timing.
- Both buffered original-data integrations completed exit 0: actor bytecode,
  Class/State layout/dispatch, all 936 inventory slots, references/Region,
  animation/state/clock, paired saves, rollback, asset retention and guarded
  travel pass. InitializeInventory reaches unsupported Spawn278 PC253 with
  full rollback, not successful inventory creation or campaign startup.
- Added isolated world-only spawn placement with original BSP hull semantics,
  bounded graph/work validation and 27 candidate order. Synthetic controls
  pass 9,482 checks/99 refusals; no runtime allocation/lifecycle hookup exists.
  Original-map differential verification is pending.
- Current buffering/UI source is not yet Android-verified or deployed. The
  installed APK remains frozen. See [BUFFERED-READER.md](BUFFERED-READER.md)
  and [SPAWN-PLACEMENT.md](SPAWN-PLACEMENT.md).
- Fresh buffered Training/static-lightmap/actor, Inventory and Health CPU
  captures pass with zero mean pixel difference from frozen raw-reader previews.
  Visually inspected frames retain varied lighting/textured geometry and
  original UI artwork/fonts. This is not original-renderer or Quest GPU
  equivalence. Inventory's single-cell shrinking of large icons is a confirmed
  fidelity defect now under separate development; its original grid is 5x6.

## Offline original object/class casts (2026-10-09; host verified, not deployed)

- Added MetaCast/DynamicCast execution, source-local typed target resolution,
  detached Object results and shared transaction/budget accounting. Actual
  class identity and literal declaration Name semantics remain distinct.
- Added a shared bounded hierarchy helper and a generated-package test of the
  production adapter. Review fixed wrong imported ClassName acceptance for
  targets/ObjClass/ObjBase and permanent interning of rejected native paths.
  Known native class metadata is not spawned actor gameplay.
- Final host build and ordinary CTest pass: 34 tests pass, two optional-root
  entries explicitly skip. VM controls pass 641 checks/150 rejections; pure
  hierarchy controls pass 111/54 and the production adapter passes 381/59 across
  164 generated metadata exports. The separate original-data runtime test
  completed with exit 0: paired-save recovery, retained assets, v4/v5 script
  composition and guarded travel pass. The separate buffered original-data
  actor check also completed exit 0, including the updated initializer assertion;
  the frozen raw-reader reference is still running. The real initializer test
  requires Spawn278 PC253 after its class cast; spawning remains unimplemented.
- Quest USB is no longer available on the latest check. No APK replacement,
  permission changes or save modifications were performed. Original-data
  raw-reader reference verification remains pending; the passes above do not imply startup.
  See [OBJECT-CASTS.md](OBJECT-CASTS.md).
- The cast-only source batch compiles for Android ARM64. Signature v2 verifies, both
  packaged port libraries match the built stripped outputs, and the APK has
  no original commercial packages. This is not an on-device startup test;
  the user's installed build remains frozen. Fresh Inventory/Health CPU
  previews also pass and were visually inspected without obvious missing
  artwork, panel overlap or text clipping; live state/input remain unverified.

## Offline authored struct support (2026-10-08; verified, not deployed)

- Added exact Core.Struct descriptors and bounded declaration-order scalar/nested
  value decoding. Runtime zero values and current-map save validation now use
  original struct fields, including InventoryItem's class<Engine.Inventory>
  member and eight independently inherited/overridden InitialInventory slots.
  Reflected member writes recursively check class/object constraints.
- Added pinned pre/post integer increment/decrement natives 163--166. The actual
  AddInitialInventory search requires postincrement165, which earlier runs
  exposed as missing. Returns are detached snapshots; aliases, write budgets
  and whole-call rollback are retained.
- Reviewed final host build and CTest pass: 32 ordinary tests pass, with the two optional
  original-root integrations skipped by that invocation. Value controls pass
  437 checks/121 rejections, descriptor controls 500/137 plus 33/24 source-import
  provenance controls, and extended VM
  controls 354/75. Separate original InventoryItem/Vector/PointRegion metadata
  audit passes. The separate final reviewed-source original runtime integration also
  completed exit 0: gameplay, asset retention, paired-save recovery, v3/v4/v5
  composition, canonical script blobs, legacy reset and travel guards pass.
- Review corrected export Name versus FriendlyName tag matching, combined
  raw/normalized descriptor and schema/reference identity memory charging,
  source import ClassName provenance and ambiguous import path segments.
  All-eight-slot batched readback avoids repeated per-slot metadata loads,
  retains the same independent raw-data comparisons, and bounds the total
  returned values. A collision fixture now proves both writer and reader
  rejection, rather than aborting during generation of invalid saved data.
- The full final-source original actor suite completed with its terminal PASS.
  All eight slots of 117 original ScriptedPawns (936 slots total) matched
  independent original data across Training, Liberty Island and Intro, including
  123 map and 86 inherited nonempty entries. All 1,502 Class/261 State dispatch,
  2,820-statement/355-label/159-table layout and existing human/robot/bird,
  reference/Level, animation/save, SetPhysics and state regressions passed.
  Earlier runs
  are not counted as passes: the first selected NPC has no eligible empty slot,
  and its real search reached missing native165. The corrected test retains
  that original false-return path, then explicitly identifies generated empty
  slots for positive transaction controls. Those generated fixtures are not
  new game defaults or inventory spawning. The current run has now passed
  actual helper member writes, v4/v5 inventory composition, 18 malformed-schema
  rejections and exact InitializeInventory MetaCast13 PC123 refusal with full
  StartUp rollback. Ordinary StartUp now stops atomically at unsupported
  conversion57 in InitializeHomeBase PC35; begun-play BeginState still refuses
  required AIEndEvent715 in SetDistress PC46. These are not successful startup.
- Visually inspected fresh Inventory and Health CPU previews using the reviewed
  binary and original artwork/fonts. No obvious missing tiles, panel overlap
  or text clipping was seen. The long rifle icon is small within its fixture
  grid cell, and Health callouts are artwork without live limb statistics.
  Preview text is small at640x480; Quest readability/input/stereo/live state
  remain unverified. Generated BMP/JSON stay in ignored artifacts, not Git.
- No Android build or replacement deployment was performed. After the user's
  tracking-ready reply, ADB confirmed the Quest connection and launched only
  the unchanged installed 25e38b3 app. OpenXR was focused, but reported no valid
  head pose; Training CPU preparation completed while GPU upload was pending.
  A later scoped check found the display awake/ON, but VR power reported
  HEADSET_UNMOUNTED and tracking focus None. The system denied HEAD_TRACKER
  access to the actual game PID. This points to a system mount/focus/sensor
  access gate; it does not prove that the user was physically not wearing it
  or identify a missing manifest permission. No permission/security settings
  were changed. One ordinary ADB system screenshot succeeded but was entirely
  black; it cannot establish what the user sees through compositor/protected
  layers. No menu/gameplay/app screenshot mailbox commands or saves were changed
  by the verification commands. This is not a successful visual/tracking test.
  Full campaign startup, casts, dynamic Spawn,
  inventory lifecycle, AI, latent behavior and live scheduling remain unfinished.
  See [AUTHORED-STRUCTS.md](AUTHORED-STRUCTS.md). Local evidence is in ignored
  `artifacts/authored-struct-20261008/`. Only authored source/tests/docs are
  published; commercial data and generated visual/log artifacts remain outside Git.

## Offline persistent actor-state execution (2026-10-08; not deployed)

The following state-execution counts and InventoryItem refusal describe the
previous verified batch; newer authored-struct verification is recorded above.

- Added explicit bounded state slices, transactional GotoState/Enable/Disable
  and synchronous BeginState/EndState within the caller's transaction. State
  code and local declaration ownership are distinct; inherited labels, old
  statement control after reentry, stopped stale PC and local-storage revision
  checks follow the pinned contract. Unsupported required behavior rolls back
  the entire slice. No automatic actor/world startup or live state tick runs.
- Calls retain identity first, evaluate caller arguments, then check fresh
  eligibility and prepare the full callee. Enable/Disable and transitions in
  arguments can therefore affect the call without a cached suppressed stub.
- Runtime v5/codec v2 store current-map state frames, typed locals and independent
  state-keyed disabled sets. State-only records do not fabricate native class
  locals. Original schema/ancestry/HasStack/PC/latent checks precede application;
  legacy saves clear portable overrides. Property/clock-only v4/codec v1 and
  untouched v3 representations remain. Persistent state has aggregate bounded
  noncopying measurement; combined saves retain the 16 MiB envelope cap.
- SetPhysics (3970) now performs the pinned reflected Physics-byte assignment,
  without inventing optional-floor, Base, Velocity or movement behavior.
- Host build completed successfully. The suite has 32 entries: 30 ordinary
  tests pass and two original-data tests explicitly skip without their separate
  root. New state controls pass 75 checks/16 rejection controls; the new state
  codec passes 1,251/621. Existing VM, dispatch and legacy codec controls pass.
  Separate final-source original-runtime/save/map integration passed v3/v4/v5
  composition and paired-save recovery, including state-only records, exact
  canonical script blobs, restored gameplay semantics and legacy reset. The
  legacy gameplay prefix is not byte-canonical. The separate original actor
  integration also completed exit 0: all 1,502 Classes/261 States and terminal
  labels, existing helpers/reference/Level gates, SetPhysics/v4/legacy controls,
  v5 state-only/Auto/Begin/None/Stop/disabled-set roundtrips, 12 new state-schema
  rejections and full-payload rollback passed. Actual StartUp slicing still
  refuses InventoryItem in InitializeInventory at PC 42; actual begun-play
  BeginState still refuses required AIEndEvent715 in SetDistress at PC 46.
  Those atomic refusals are not successful startup. Interrupted runs were not
  counted as passes.
- Visually inspected a 512x512 CPU Doctor1 close-up after the actual compiled
  pre-begun-play SetInitialState: original mesh/materials render, no cube fallback.
  It retains a static All pose; this is not live idle animation, lighting or
  Quest GL/stereo/controller verification. A read-only original DLL audit also
  established deferred AI-event processing and authored manager initialization,
  without enabling AI natives. See [AI-NATIVE-AUDIT.md](AI-NATIVE-AUDIT.md).
- No Android build, ADB command or replacement deployment was performed.
  The frozen installed `25e38b3` APK remains the user-test build. Full startup,
  AI, latent handlers, physics simulation, live animation scheduling, per-map
  archives and campaign progression are unfinished. See
  [STATE-EXECUTION.md](STATE-EXECUTION.md). Logs are in ignored
  `artifacts/state-execution-20261008/`.

## Offline script selection and eligibility (2026-10-08; not deployed)

This previous batch's execution limits and test counts are historical; the
state-execution layer above supersedes its mutable-state restrictions.

- Added a bounded, lazy Class/State/Function graph from original Children/Next
  chains, including the UField prefixes of Struct/Enum/Const siblings. Named
  state and virtual lookup are derived-first; globals skip states; Auto uses
  actual global NameString CompareIndex, not alphabetical order. These are
  read-only selections, not state entry.
- Authored stopped HasStack contexts now supply code/class masks and disabled
  probe names with pinned polarity: IgnoreMask zero denies, compiled class/code
  ProbeMask permits, and serialized stack-mask set bits disable. Class-backed
  stopped records retain their class name. GetStateName/IsInState read this
  context; runnable saved continuations remain unsupported.
- Eligibility now suppresses calls before callee locals, arguments, native work
  and parsing. Fully qualified Standing.AnimEnd selection cannot bypass it;
  this corrects the earlier execution-only fixture's callback interpretation.
  Explicit event dispatch additionally requires begun play and deletion gates.
  No automatic animation/event scheduler or mutable state transition is enabled.
- Read-only bytecode analysis preserves terminal top-level label order and
  duplicates, validates original identities and top-level targets, and never
  invokes variables/natives/transactions. Serialized LabelTableOffset is retained
  without assuming that it points to the opcode; pinned lookup uses the terminal
  statement instead. State label execution remains unfinished.
- The host suite has 30 entries: 28 ordinary tests pass and two original-data
  integrations explicitly skip without their separate game root. VM controls
  pass 285 checks/66 rejections, dispatch 899/20 and descriptor controls 386/101.
  Both separate original-data integrations passed. Graph coverage: 1,502 Classes,
  261 States, 7,511 class functions, 697 state functions and 357 common fields.
  Layout coverage: 2,820 statements, 355 labels and 159 terminal tables, with
  independent table-entry/source-name/target comparison. Doctor/RepairBot/Pigeon
  stopped-context controls, exact receiver-Level null/begun/deleted gates,
  existing helpers, 15 malformed-save controls and save/travel guards passed.
  Inventory/Health original-art CPU previews were visually checked with sample
  data; no new GL/stereo/controller verification is claimed.
- No Android build, ADB command or headset deployment was performed. The user's
  frozen `25e38b3` test APK remains unchanged. Full startup, state/latent/AI
  execution, dynamic disabled sets, scheduling and persistence remain incomplete.
  See [SCRIPT-DISPATCH.md](SCRIPT-DISPATCH.md), including the future identity-only
  callee-stub requirement before argument expressions can change eligibility.
  Logs are in ignored `artifacts/state-dispatch-20261008/`.

## Offline authored-state foundation (2026-10-08; not deployed)

- Retained exact UField/UStruct/UState metadata for script State/Class exports:
  original references/names/source positions, raw and normalized bytecode,
  compiled masks, uint16 label offsets (including `0xffff`) and unknown flags.
  Existing class bytecode/default decoding remains compatible. Map actors now
  retain their exact HasStack headers, without treating dormant class-backed
  records or nonzero latent numbers as ready-to-run AI.
- State/Class payloads are file-checked before allocation and bounded to
  64 MiB; logical expansion, references, names, recursion, outer-chain identity
  and exact termination are checked. The new descriptor test passed 365 checks,
  including 97 rejection controls. A read-only audit passed all 38 original
  System packages: 261 States and 1,502 serialized Classes, 23,037 raw script
  bytes and 30,167 normalized bytes. All 88 original maps have zero Class/State
  exports; modified map-local script definitions remain unsupported.
- The host suite now has 29 entries: 27 ordinary tests pass, two original-data
  integrations explicitly skip without the separately supplied game root.
  Separate original runtime/save/map-transition regression passed, including
  v3/v4 paired-save recovery and retained original assets. The separate original
  actor integration also passed exact State/Class metadata, Doctor1/RepairBot0/
  Pigeon0 raw stacks, case-insensitive lookup, wrong-identity rejection,
  preservation through legacy reset and rejected startup, and retired-map
  lookup rejection, alongside existing bytecode/native-clock/v4 save checks.
- Read-only runtime metadata queries use the game's indexed vector-package
  initialization path, return detached copies and do not dispatch events,
  create live script state, alter clocks or change save/travel guards. The
  single-package lifecycle helper remains outside that query path.
- Documented the pinned level-wide startup, event masks, state lookup,
  synchronous EndState/BeginState and state-PC contracts. Reached startup
  dependencies still include pawn lists, real spawn/collision/destroy,
  conversation binding and unavailable AI callback behavior; pinned AI stubs
  cannot establish faithful NPC reactions. Full startup/state/latent execution
  and its persistence remain unfinished. See
  [AUTHORED-STATE-FOUNDATION.md](AUTHORED-STATE-FOUNDATION.md).
- This source batch was host-built only. No Android rebuild, headset command
  or deployment was performed. The installed/frozen `25e38b3` test APK remains
  the user-test build; its local SHA-256 is unchanged. Game data and saves were
  not modified. Logs are in ignored `artifacts/state-metadata-20261008/`.

## Quest launch responsiveness and headset handoff (2026-10-08)

- Installing commit `9e4779f` preserved the user's data, but PID 8666 then
  exited with Android reason 6 (ANR): MainActivity had no focused window while
  synchronous session startup prepared Training. The launch intent being
  accepted did not prove a working game. This build supersedes that test APK.
- Session initialization now launches one owned CPU preparation worker and
  returns to Android/OpenXR event processing immediately. The worker prepares
  the original runtime, cache, static lighting, actor materials, Persona artwork
  and PCM audio into a private result. While it owns runtime/GC/name tables,
  gameplay, map retries and diagnostic commands stay disabled.
- The frame thread polls readiness without waiting, commits the result only
  after `future.get()` transfers ownership, opens AAudio/Persona graphics on the
  main thread and reuses the existing staged world/actor GPU uploads. Loading
  and startup failure have distinct HUD messages instead of appearing dead.
- Session/application teardown joins an unfinished worker before runtime/GC
  destruction. This prevents detached work or stale data, but teardown during
  expensive preparation can still wait; cooperative cancellation is unfinished.
- Replacement ARM64 APK installed successfully with `adb install -r` on Quest
  serial `2G0YC5ZG620985`; no uninstall or data clearing. PID 10891 returned from
  session initialization at 08:12:50.789 and reported valid head tracking at
  08:12:50.893. The original Training lightmap bake completed on-device: 5,032
  surfaces, 1,156,691 pixels and 2,764,021 shadowed mask samples. Its staged
  visual transition completed at 08:13:30.083 with 1,308 runtime actors and
  24,229 BSP collision triangles; the nearby stream contained nine vertex
  meshes, four mover brushes, zero cube placeholders and zero omitted poses.
- Inspected actual 1680x1760 Quest eye captures of Training/HUD and all four
  enabled Persona pages: Inventory, Health, Goals/Notes and Logs. Original art
  and bitmap text render. These are real empty-start runtime pages, not
  populated inventory/progression fixtures or proof of controller interaction.
- The process remained alive through menu/capture testing; exit history still
  showed the earlier PID 8666 ANR, not a new exit for PID 10891. Idle windows
  report 72.0 fps / 13.89 ms worst. Initial staged preparation peaked at 83.33 ms;
  diagnostic screenshot readback windows peaked at 291.65 ms and need later
  performance work. The 26 ordinary host tests still pass; two optional
  original-data tests skip by default. APK signature scheme v2 verifies.
- Full campaign completion is not claimed by this handoff. See
  [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md) for testing controls and boundaries.

## Scoped script saves and current test build (2026-10-08)

- Runtime v4 preserves supported original actor property overlays and complete
  main/four-blend clocks, native tween histories, rates/flags, simulation time,
  RemoteRole and Fatness. Untouched runtimes still write v3; v1-v3 readers remain.
  Capture validates before writing; restoration validates original identities,
  exact typed property schemas, object/class constraints and clock agreement
  before changing live state. Whole runtime payloads stay within 16 MiB.
- The structural codec passed 1,415 checks (1,355 rejection controls). Original
  actor integration passed four positive authored Mesh/Owner/PointRegion/Texture
  reference controls and 15 malformed schema controls, exact full-clock resave,
  native clock continuation, read-only different-map preflight, metadata binding
  and legacy reset. Original gameplay/progress and paired v3/v4 saves also pass.
- Native Mesh/Texture UClass wrappers can be absent from disk exports. Save
  validation now follows exact authored/native hierarchies rather than falsely
  requiring such wrappers or treating unknown asset classes as Core.Class.
- The host suite has 28 entries: 26 pass without assets, two explicitly skip
  by default and passed separate original-data runs. Four world albedo captures,
  the original helper close-up and inventory artwork remain pixel-exact.
- Retained serialized HasStack headers pass 247 synthetic controls (71
  rejections); all 88 original maps contain 135,479 class-backed records, zero
  named-state continuations and 66,964 nonzero latent fields. These dormant
  class records are retained, not misinterpreted as ready-to-run AI states.
- Map travel/unload still require per-map archiving when script state exists.
  Quest checks this before cancelling UI/audio/geometry or starting replacement.
  Saving does not clear that guard. See [SCRIPT_STATE_SAVE.md](SCRIPT_STATE_SAVE.md).
- A USB install/launch exposed an ARM static-lightmap startup failure. The
  expanded cubic radius falloff can become slightly negative under fused
  floating-point evaluation; it now uses the algebraically identical factored
  form. HDR range checks remain strict, with surface/RGB diagnostics. The
  fix passes 196,608 near-radius controls and an explicit negative FMA fixture;
  the final ARM64 APK builds successfully and the final host suite is green.
  Its first final-APK installation subsequently exposed the independent Android
  startup ANR above, before a completed lighting result could be observed.
- Full authored startup, AI/state/latent execution, timers/spawned actors,
  campaign archiving/progression, live GPU animation and performance remain
  unfinished. This is an installable test build, not full campaign completion.

## Bounded original script execution (2026-10-08)

- Added a typed, bounded interpreter for normalized original UE1 bytecode:
  locals/parameters/out aliases, assignments, fixed arrays/struct members,
  branches, nested calls, lazy boolean operators, object context and selected
  numeric/vector operators/conversions. References use the executing function's
  package; ordered parameters retain the original Children/Next chain.
  Corrected token 0x60 decoding at the conversion/extended-native boundary.
- Each root invocation is transactional. Unsupported execution or exhausted
  budgets undo nested properties, out aliases and native commands. Native
  returns are detached values; escaped argument guards fail safely. Synthetic
  serialization/execution tests pass 241 checks and 32 rejection controls.
- Actor execution stages actual original PlayAnim/LoopAnim/TweenAnim commands,
  writable properties and captured history. Region.Zone uses original BSP
  traversal, not a dry-room guess. One persistent case-insensitive object index
  avoids rebuilding roughly 101,000 lookup entries for every script call.
- The dependency-free native main/blend clock passes 187 command/timing controls,
  including synchronous replacement callbacks, residual budgets, invalid data,
  native history capture and explicitly labelled pinned behavior/corrections.
  Automatic event eligibility, callbacks and renderer ticking are unhooked.
- A desktop fixture can execute one compiled helper before isolation/capture.
  Doctor1 LoopAnimPivot selected BreatheLight through real bytecode: 37
  instructions, three writes, 603 rendered triangles and no pose omissions.
  Visual inspection found correct lab-coat/skin geometry; no manual frame or
  sequence override was used. This is not a live animation or campaign test.
- The host suite has 26 entries: 24 ordinary tests pass; the two original-data
  integration tests explicitly skip without their separately supplied root.
- Separate real-data execution passed Doctor1's PlayWaiting, explicit
  Standing.AnimEnd and Play/Loop/TweenAnimPivot helpers; RepairBot0's inherited
  PlayWaiting; and 00_Intro.Pigeon0's native-default PlayWaiting. Exact original
  state-startup and FRand calls still fail explicitly, with transaction rollback.
  Tests also confirmed PrePivot snapshot propagation and save/travel/unload
  refusal without losing state. The existing original-data runtime regression
  passed Training/Combat restoration, paired-save recovery and retained assets.
- The ARM64 APK builds offline. No headset installation, ADB probe, physical
  controller test, OpenXR session or device performance check was attempted.
  Four world-albedo image regressions remain pixel-exact (MAE 0).
- Committed script/native clock state is currently memory-only: version-3
  save, map replacement and unload refuse rather than discard it or truncate
  an existing save. Validated authored checkpoint restoration resets overlays.
  No automatic gameplay path invokes this scoped interpreter yet. Full startup,
  AI/state/latent execution, remaining natives, dynamic saves, live GPU updates,
  campaign progression and headset validation remain unfinished.
  See [PORTABLE-SCRIPT-EXECUTION.md](PORTABLE-SCRIPT-EXECUTION.md).

## Authored animation poses and offline validation (2026-10-07)

- Retained shared compressed vertex animations, authored sequences/notifies,
  direct normal topology, remapped draw identities and attachment vertices.
  The shared CPU sampler handles interpolation, native-history tweens, raw-space
  Fatness and four additive Deus Ex blends without inventing idle names.
  Package-correct inherited snapshots preserve indexed channels, explicit None,
  and immediate-owner animation while keeping the rendered actor's Fatness.
- Quest initial actor geometry uses one background pose worker, including
  retired work across cancellation. Immutable captures and epoch/ordinal checks
  prevent stale map/save results from entering a new build. Cancellation never
  destroys an unfinished async future; waiting cannot falsely complete a build.
  This design is compiled and reviewed, not on-device timing verification.
- The independent original-asset audit passed 437 meshes (97 animated),
  2,499,176 packed vertices, 2,485 sequences and 359 notifies. It compared
  411,058 source-vertex samples with the pinned DX formulas/matrices, including
  remapped normals, across 3,392 pose calls and 6,930,339 checks. Shared retained
  data totals 17,508,019 bytes for all audited assets; largest asset 899,113 bytes.
- Two original sword exports have six dangling sequence spans. These are
  retained and labelled, not silently clamped, fabricated or rejected as whole
  static/material assets. Missing required frame accesses produce explicit pose
  omissions, as the pinned renderer's bounds checks do.
- Authored snapshots independently passed Training/Island (4,966 actors,
  90 inherited name sources), with five original nonzero corpse frames.
  Visual inspection confirmed a lying corpse without an override, plus separate
  explicit Jaime BreatheLight/Run pose fixtures. The latter are not live NPC
  startup/idle animation or proof that scripted transitions work.
- The host suite has 23 entries: 22 ordinary tests passed and the optional
  original-state test skips by default. Its explicit original-data run also
  passed save/rollback, paired-save recovery and retained pickup resources.
  Four albedo frames and the Persona fixture remained pixel-exact (MAE 0).
  The updated ARM64 APK builds offline; no headset/ADB access was attempted.
- Live animation commands/clocks, notifies/AnimEnd, script states, dynamic tween
  saves and attachment rendering remain unfinished. Full campaign playability,
  visual fidelity and Quest GPU/performance verification are not achieved.
  See [ANIMATION-POSES.md](ANIMATION-POSES.md).

## Original actor rendering and offline close-ups (2026-10-07)

- Corrected original indexed skin inheritance/material precedence, serialized
  mesh RotOrigin, actor pitch/roll, pivots/MainScale, normals and reflected
  winding in shared Quest/desktop CPU helpers. Authored mover Polys now retain
  textures and UV mapping rather than rendering as gray brush geometry.
- Added independently indexed actor-bank software rendering and original actor
  isolation. Actual close-ups revealed texture `bMasked` and magenta filtering
  defects; separate opaque/masked P8 layers now preserve the correct semantics.
  Selected-material packing retains inactive original actors for quickload.
- The original transform audit decoded 437 meshes (228 nonzero RotOrigin).
  The completed material audit covers all 80 numbered maps and 124,199 authored
  actors: 10,482 non-null indexed skin slots, 10,292 selected actor overrides,
  and pickup/restore retention checks on 78 maps with eligible inventory. No
  selected mesh material was missing; procedural texture fallbacks remain.
  Twenty ordinary host
  tests pass; the separately opted-in original-state test also passed, including
  paired-save recovery and retained picked-up mesh/material resources. Albedo and Persona
  image regressions remain exact.
- CPU scenes submitted 64 Training and 280 Island mesh instances plus 25/19
  textured movers, with no missing mesh materials. This does not prove that
  every submitted object was individually visually inspected. Procedural
  textures, live animation, actor lighting/shadows, sprites, skyboxes, complete
  campaign execution and Quest GPU/stereo/performance remain incomplete.
- The revised APK builds offline. No headset/ADB access was attempted.
  See [ACTOR-VISUAL-TESTING.md](ACTOR-VISUAL-TESTING.md).

## Original static shadow-lightmap implementation (2026-10-07)

- Added a bounded, complete v68 UModel decoder; all 88 installed root models
  passed the actual authored-data audit. Ordered per-light masks, zone ambient,
  surface normals and Unlit flags now drive the shared static BSP baker.
- Preserved DXQM v2 geometry and added a bounded parallel DXQS surface/zone
  stream. Exact original fan/winding/zone/position matching rejects mismatched
  caches, including wrong coplanar surfaces. Runtime-first PlayerStart ordering
  differed in Area51; actors, lighting and ambient sound now use the verified
  serialized cache origin.
- Implemented guttered HDR-scaled lightmap atlas sampling on desktop and a
  separate Quest world shader, with per-fragment tile clamp and original UV
  equations. Map-worker baking and frame-sliced initial/transition uploads
  preserve existing runtime rollback and original asset retention.
- Sixteen host tests pass, with the opt-in original-state test explicitly
  skipped. Training albedo regression is exact across four views. Real static
  Training/Island/Hong Kong/Area51 captures are separate from earlier 352-image
  albedo audits; no new full-campaign lighting audit is claimed.
- The updated APK builds offline. Dynamic lights, skyboxes, actors' original
  lighting, complete campaign scripting and physical GL/stereo/performance
  verification remain unfinished. No headset access was attempted.
  See [STATIC-LIGHTMAPS.md](STATIC-LIGHTMAPS.md) for bounds and evidence scope.

## Desktop validation and follow-up fixes (2026-10-07)

- Restored the source checkout and exact pinned dependencies, with repeatable
  build-tool discovery and non-destructive dependency validation. Original data
  is now read from the user's Steam installation outside the source tree.
- Added no-headset captures from real UE1 packages using the same portable map
  cache decoder as Quest. The first audit decoded 73 of 88 installed maps;
  fourteen failures selected a Palette instead of the identically named Texture
  export, and one rejected a case-only object-path difference. Both defects are
  fixed in shared code, including actor and UI texture lookup. The final audit
  decoded all 88 maps and saved 352 world-albedo images with zero map failures.
  The black utility maps `DX`, `DXOnly`, and `Entry` remain explicitly flagged
  as uniform frames, not presented as visually meaningful tests.
  This is not a campaign playthrough: actors,
  authored lights, skyboxes, UI, audio, scripts, and Quest GPU/XR behavior are
  excluded from the software captures.
- Added ten shared transform regression groups. They cover renderer-math
  agreement, nonzero tracking origins, 1,201 repeated off-origin snap turns,
  simultaneous turning/movement, direction/audio basis, room-scale rollback,
  diagonal/hitch bounds, thin-wall path checks, reference-space continuity,
  tracking reacquisition, and loading-position anchoring.
- Fixed an update-order defect that could overwrite the pivot translation after
  a snap turn. Reconstructed room-scale collision rollback against the new yaw
  and sampled the movement path rather than checking only its endpoint.
- Added valid-tracking gating, reference-space rebasing, and a stable map-local
  headset anchor while uploads are incomplete. Unknown/tilted reference changes
  preserve horizontal position through a fallback; heading continuity in that
  fallback remains unverified.
- Set explicit source-alpha blending factors for the Persona artwork. Enabling
  blending alone retained the SDK's opaque ONE/ZERO defaults.
- Original Persona script/default-property inspection identified separate
  masked client/background and border windows, not a coincident 640x512
  composite. The new 640x480 compositor uses their original offsets, clips,
  grayscale default tints, five-by-six grid, and 30 visible slots. Colored icons
  are no longer theme-tinted. Four measured, baseline-anchored text panes replace
  proportional-font space columns and overflowing separator strings.
- The motion/blending APK was rebuilt, installed on the connected physical Quest
  3, and captured at 1680x1760. Training and diagnostic left/right turns rendered
  world geometry and HUD; settled windows held 72 fps / 13.89 ms worst frame.
  Synchronous screenshot readback caused separate 264-292 ms hitches, so those
  captures are diagnostic operations, not a comfort/performance pass. The process
  remained alive at 371,071 KB total PSS. Subsequent Training captures confirmed
  the corrected Inventory layout, selected original Multitool icon, and
  contained item-data text. The old Health page still used inventory artwork;
  that defect is addressed by the page-specific compositor below.
  Controller-driven simultaneous room-scale motion, tracking loss/recenter,
  broader maps, and the latest texture/UI fixes still need hardware validation.
  Earlier hardware observations below apply to their recorded builds, not
  automatically to this new build or to all campaign maps.

## Shared page artwork and near-geometry text fix (2026-10-07)

- Extracted the CPU Persona compositor into a dependency-free shared header;
  both Quest and the desktop preview execute its mask, clipping, tint, grid,
  selection/scroll-window, and original-color icon copy routines. Inventory's
  output remained pixel-identical (exact baseline error zero).
- Read the shipped Health, Goals, and Logs class defaults and embedded UI
  scripts. Each implemented page now uses its own original background/border
  tiles and client rectangles. Health includes the neutral original body and
  overlays; Goals/Notes uses stacked text panes; Logs uses four background tiles
  and six Conversations border tiles with a single central text column. These
  assets are decoded from the user's package, never checked into this repo.
- All four real artwork previews were inspected. Three host CTests pass,
  including page origins/clips, transparent padding, two-column Logs tile
  placement, Health body crop, inventory selection, icon color/aspect, and
  existing software-world and VR-transform regressions. The previews do not
  verify fonts, live menus, GL rendering, or body-part damage simulation.
- The previously failing Hong Kong MJ12 lab physically loaded 2,308 actors and
  29,823 BSP collision triangles. Its initial actor stage still caused a 250 ms
  transition window; settled tracking-valid windows returned to 72 fps/13.89 ms.
  The first eye-buffer view looked into close geometry, so it is not a broad
  visual approval of that map. It exposed a concrete UI occlusion defect:
  Persona artwork appeared but every font label disappeared. The SDK batched
  font surface enables depth independently of menu-object flags. Submitted
  head-locked UI definitions now disable depth test/write without modifying the
  SDK-owned definitions. Follow-up physical Inventory, Health, and HUD captures
  in the lab show text visible against close geometry, confirming this GPU
  regression fix at the tested viewpoints.
- The updated APK builds and is installed. All four artwork pages decoded on
  device. After the wearer restored tracking, original-art Health, Goals/Notes,
  and Logs were captured in Training and inspected; each uses its own artwork
  and contained text. A later lab snapshot measured 415,914 KB total PSS. The
  screenshots exposed remaining tab/footer contrast problems over bright world
  textures: original tab/action-button artwork and fonts are still missing.
  No tracking or proximity safety setting was bypassed.
- Diagnostic requests now wait for the one-slot mailbox and use noclobber rather
  than replacing pending menu/map actions. Request consumption is explicitly
  distinct from map-upload or screenshot completion. Screenshot ADB failures
  fail closed. All 24 mock-ADB regressions pass under PowerShell 7 and Windows
  PowerShell 5.1, exercising the actual helpers without a Quest.

## Headset-free font, loading, and recovery work (2026-10-07)

- Added the original UE1 bitmap-font decoder, including bounded font/atlas/
  palette payloads, class-aware object references, exact glyph rectangles and
  advances, masking, lowercase fallback, and 32 malformed-input rejection
  controls. Both real `FontMenuHeaders` and `FontMenuSmall` decode from the
  user's `DeusExUI.u`: 256x128 atlases, 256 glyphs, and 10-pixel line height.
- Quest and desktop now share original-font text composition and the original
  navigation/action-button artwork. All four CPU previews were rendered and
  inspected. That inspection caught cropped Goals/Notes button captions and
  help text crossing panel borders; both were corrected and recaptured.
  The original eight-tab order is retained, with unsupported pages dimmed.
  Actions are adapted VR binding labels, not every original desktop action.
  Reports explicitly separate fixture composition from GL/XR/live interaction.
  Reviewed baseline rerenders match exactly on all four pages; changing the
  selected inventory fixture correctly fails a zero-error comparison.
- The latest full BSP/albedo audit also passed all 88 maps / 352 images with
  zero map failures. All 352 frame hashes match the earlier audit, including
  after compact-index overflow hardening. The twelve uniform utility-map views
  and static procedural-texture limitations remain explicit warnings. This
  still does not validate campaign progression or the new actor GPU staging.
- Replaced the monolithic actor geometry build with cooperative preparation:
  3 ms / eight operations / 6,144 scanned vertices between-operation limits,
  triangle-aligned chunks, and at most one GPU chunk per frame. Targeting data
  remains immediate. Cancellation/cleanup covers streaming, interaction,
  quick-load, map replacement, and session shutdown. Stable renderer containers
  also prevent SDK self-uniform pointers from becoming stale after relocation.
  Initial metadata, individual mesh copies/material scans, GPU calls, and
  resource deletion remain nonpreemptible; actual Quest timing is pending.
- Guarded main-thread runtime reads/actions while the map worker replaces it.
  A distinct private worker-side transition checkpoint is required before
  replacing a usable runtime. Preparation failure restores the prior map/state;
  failed rollback or GPU staging clears mismatched visuals and suspends unsafe
  gameplay, retaining a next-map retry. Existing user quicksaves are not replaced
  by these checkpoints.
- Fixed the session-restart map mismatch: startup's fresh Training runtime now
  resets the retained map name/pose and stale menu/dialogue state consistently.
  Automatic cross-session resume is not implemented; X still loads the user's
  existing quicksave. The XR session lifecycle needs physical validation.
- Discarded prior-session speech futures and removed audio-worker access to
  global UE1 name storage, which a concurrent map load could reallocate. Failed
  quick-loads no longer cancel unfinished actor staging or replace menu history;
  cross-map history is applied only after successful transition completion.
  Corrupt metadata string lengths are checked before allocation.
- Extracted the actual dialogue MP3 decoder into a byte-only shared helper,
  with bounded input/PCM allocations and decoder cleanup on all paths. Host
  tests cover malformed/truncated input, forged Xing sample counts, output
  limits, mono duplication, distinct stereo tones, and exact resampling at five
  rates. The original Mission01 `ConAudioMission01_289` voice also decoded and
  matched the previous interpolation sample-for-sample (48 kHz: 95,294 stereo
  frames). This is CPU decoding evidence, not audible Quest playback/mixing.
- Seven automatic host CTests pass, including nine budget/triangle groups, six
  transaction failure/recovery groups, text/chrome compositor checks, fonts,
  world rendering, MP3 decoding, and transform math. The real-data state entry is
  explicitly skipped without opt-in. Its separate original-data run passed:
  101,375 runtime objects and 1,308 Training actors, Training -> Combat ->
  Training, with exact restored actor snapshots, inventory, player/pawn health,
  credits/skill points/goals/notes/flags, and effect deduplication. Missing or
  truncated checkpoint loads preserve live state. Seeded progress effects are
  test fixtures, not evidence of full campaign execution. Temporary test files
  are isolated; original packages and user saves remain untouched.
- All 24 mock-ADB diagnostic tests still pass under PowerShell 7 and Windows
  PowerShell 5.1 without a device. These latest changes build for Android ARM64
  but are not installed or tested on Quest
  during this headset-free development batch. The earlier physical observations
  above apply to their recorded builds only.

## Paired saves and authored-light previews (2026-10-07)

- Quicksaves now publish paired metadata/runtime bundles into two alternating
  generations after durable staging and checksum verification. The previous
  complete generation survives partial writes, publication failures, and corrupt
  latest slots. Legacy two-file saves remain read-only fallback inputs. New
  metadata v5 saves map-local floor position and head heading; shared transform
  tests reconstruct both after a physical-origin/heading change without forcing
  a saved eye height. Old v1-v4 poses retain their original interpretation.
- Runtime preflight uses the same parser as application and occurs before a
  cross-map worker starts. CRC-valid but semantically invalid newer runtime data
  can fall back before changing the map. Invalid damaged-path lengths now reject
  before allocation; flags/effect containers are prepared before live-state
  mutation. Same-map restores clear abandoned response choices and speech;
  epoch invalidation discards stale audio results without waiting for a worker.
- Map restore workers prepare all authored actor geometry/material resources
  before applying saved inactivity. This prevents a later older-save reactivation
  from losing a unique item's resources. See [save recovery](SAVE-RECOVERY.md)
  for the modeled state, durability behavior, and pending hardware checks.
- Extracted the shared direct vertex-light evaluator for real-package desktop
  previews. Corrected UE1 hue/saturation, spotlight cone/effect interpretation,
  active light-type filtering, `(LightRadius+1)*25`, and non-Light-class emitters
  against pinned engine source. Tests include 341,760 HSB differential checks,
  all 256 radius bytes, disabled lights and non-Light fixture emitters.
- Original Training and Hong Kong MJ12 lab captures were generated and inspected
  without a headset. Training has 154 accepted lights/75 spotlights, with vertex
  luminance gains 0.075-1.346; the lab has 212/35, with gains 0.085-1.350. Both
  verify cache/actor PlayerStart alignment and report actual light type/effect
  counts. Training's four albedo-only baseline frames and the original Inventory
  fixture still match exactly (mean absolute error zero).
  The lab remains visibly overlit in places: no authored shadow bits, zone
  ambient, or UE1 lightmaps are applied. This is evidence of remaining fidelity
  work, not a visual approval. See [lighting fidelity](AUTHORED-LIGHTING.md).
- Eleven automatic CTests pass; the twelfth real-data entry is explicitly skipped
  unless opted in. Separate original-data runs verify Training/Combat paired
  generation identity, corrupt-latest recovery, v5 metadata, and read-only
  malformed-runtime preflight. These are CPU/filesystem checks, not full
  controller/XR/GPU save/restore verification. The updated ARM64 APK builds but
  was not installed or physically tested during this headset-free batch.
- The original-data asset-retention test passed with Training's unique
  `HazMatSuit2` mesh (471 triangle vertices), including hidden saved-map loading
  and older same-map reactivation without a geometry/texture rebuild. The
  retained array covers 44 material layers: 43 decode, while the procedural
  `Effects.Electricity.BioCell_SFX` FireTexture still uses an explicit fallback.
  This validates the restore-resource fix, not all actor visuals or procedural
  animation. Original packages and user quicksaves were never modified.

Remaining fidelity includes original baked map lighting/shadows, complete
campaign script/runtime state and progression, complete menu behavior, body-part health,
augmentations/skills/images, localized text, and broader campaign interactions.
The campaign, stereo comfort, physical recenter, and controller-driven
simultaneous movement remain unverified; these UI/decoder gates are not full
campaign completion evidence.

## Implemented; broader campaign verification pending

- Physical Quest recordings exposed and now cover two headset-motion failures:
  snap turns preserve the headset's map-space pivot instead of rotating the
  world around its origin, and room-scale head motion is compensated when the
  tracked head capsule would cross one-sided BSP walls. Four diagnostic 30-degree
  turns remained inside the Training BSP without the previous black void frames.
- Removed the Meta sample controller capsules/rings from the scene. Controller
  poses still drive aiming and interaction, but the bright green/yellow fallback
  geometry no longer obscures the authored game world while real hand/weapon
  presentation is unfinished.
- Persona artwork comes from the original DeusExUI background/border exports.
  The 2026-10-07 work above supersedes the earlier approximate 640x512 layered
  layout and guessed theme tint after new framebuffer inspection exposed their
  remaining padding, alignment, and text-boundary defects.
- Actor snapshots now preserve inherited `DrawType` and `bHidden`. Hidden
  gameplay actors no longer produce visible stand-in geometry, and UE1 sprite,
  vertical-sprite, rope-sprite, and one-shot-sprite actors render as masked
  crossed billboards when their authored texture is available.
- Actor meshes and sprites now receive the active map's local authored-light
  result instead of rendering full-bright. Transparent palette pixels have
  zeroed RGB as well as alpha to prevent bilinear magenta-key leakage along
  masked texture edges.
- Runtime diagnostics report sprite, hidden-actor, and cube-placeholder counts,
  plus each remaining placeholder class, so campaign capture passes can identify
  unsupported geometry precisely.
- The portable vertex-mesh loader now decodes classic UE1 `Mesh` triangle and
  UV streams in addition to `LodMesh` and `SkeletalMesh`, removing another
  format-level reason for physical actors to fall back to cubes.
- Mover actors now resolve their serialized `Brush` model. The portable model
  decoder triangulates the authored UE1 BSP polygons, and available doors,
  lifts, and other movers render at their actor transform with local map
  lighting instead of being omitted as metadata. Unsupported mover brushes
  remain non-fatal and are named in the Android diagnostics.
- Mover activation is included in actor snapshots. Until authored keyframe
  interpolation is implemented, opening a mover removes its closed brush and
  closing it restores the brush, matching the existing binary interaction and
  save/load state without leaving an apparently closed doorway passable.

## Verified on 2026-08-23

- The current ARM64 APK was installed on the physical Quest 3 and entered a
  focused OpenXR session. Training instantiated nine authored vertex meshes and
  four decoded mover brushes, suppressed two hidden actors, and reported zero
  cube placeholders. A 1680x1760 framebuffer capture confirmed the centered HUD,
  varied map lighting, masked plants, and clean actor silhouettes; the process
  remained alive at 354,561 KB total PSS with an empty fatal-error filter.
- World-material conversion now recognizes the classic UE1 convention where a
  shared vivid-magenta palette entry occupies all four texture corners. Physical
  logs identified index zero for `Cmd_tunnels.Metal.Ractivesign_1` and
  `UNATCO.Misc.UNATCOseal_A`; the next capture showed the radiation emblem and
  wall seal without their former opaque magenta rectangles. RGB is zeroed with
  alpha to prevent bilinear key-color leakage.
- The diagnostic mover request opened the real serialized
  `00_Training.DeusExMover30`, rebuilt actor geometry with no cube fallbacks, and
  left the OpenXR process alive. Invisible and zone-portal BSP polygons are now
  excluded from both static-world and mover-brush triangulation; Training's
  submitted BSP vertex count fell from 145,620 to 145,374 without changing its
  visible room surfaces.
- A physical transition to `00_TrainingFinal` rebuilt 65 world materials, 155
  authored lights, 11 vertex meshes, three mover brushes, and one real sprite.
  Fourteen hidden actors remained suppressed and cube placeholders remained at
  zero. Its 1680x1760 capture showed clean corridor, trim, sign, floor, light,
  and HUD rendering, and the process remained alive after the transition.
- World-material array layers now use 192x192 texels instead of 96x96, providing
  four times the source samples per material. Physical Liberty Island validation
  uploaded 107 layers, rendered 202,458 locally lit BSP vertices, nine streamed
  actor meshes, three hidden actors, one map exit, and zero cube placeholders.
  The 3,658-actor map used 419,207 KB total PSS and settled at 72.0 fps with a
  13.89 ms worst frame across repeated 720-frame windows after transition and
  screenshot work completed.
- Added compositor-independent visual capture for physical-headset debugging.
  `Capture-QuestScreenshot.ps1` requests a post-resolve left-eye readback from
  the running app and pulls the BMP over ADB. The first physical Quest 3 capture
  succeeded at 1680x1760; OpenXR subsequently held 72.0 fps with a 13.89 ms
  worst frame and no Android crash. Capture validation now retries nearly
  uniform lower-half readbacks and waits for a settled rendered frame.
- Quest-frame inspection fixed the first visible defect set. The head-locked HUD
  is centered and split into five bounded lines instead of clipping beyond the
  left eye. UE1 actor texture palette index zero now remains transparent and is
  discarded in the actor shader, removing the magenta/black rectangles around
  masked plant leaves. LodMesh faces now resolve through the serialized material
  table's texture index instead of treating material IDs as texture slots.
  Invisible trigger/travel/mover metadata no longer renders debug cubes.
  Clean 1680x1760 captures verified Training and Training Final after a physical
  map transition; the following windows held 72.0 fps/13.89 ms without a crash.
- Corrected the right-stick snap-turn sign after physical play exposed reversed
  controls: stick-left now turns the view left and stick-right turns it right.
- Ambient audio now follows every active map's serialized UE1 emitters instead
  of looping one training clip globally. Actor `AmbientSound`, `SoundRadius`,
  `SoundVolume`, `SoundPitch`, and location properties drive distance
  attenuation, equal-power stereo panning, pitch-aware resampling, and looping
  in the low-latency AAudio callback.
- Sound resolution accepts both conventional `Sounds/*.uax` packages and audio
  exports embedded in `System/*.u`, including this installation's
  `DeusExSounds.u`. WAV, MP2, and MP3 sources decode to shared mono clips so
  actors using the same sound do not duplicate PCM storage.
- On physical Quest 3, Training prepared 26 positioned emitters from eight
  decoded clips. A background transition replaced them with Training Combat's
  two emitters from one clip, completed without a crash, and returned to a
  steady 72.0 fps with a 13.89 ms worst frame over the following ten seconds.
  The Android crash buffer remained empty.
- NPC speech now remains associated with its serialized map actor through the
  asynchronous MP3 decoder and is spatialized from that actor every frame;
  position lookup works even when the speaker is outside the visual actor
  streaming radius. Player response audio remains intentionally head-centered.
  Physical validation queued Jaime Reyes' Training line as spatial audio,
  spatialized Paul Denton's 298,944-frame Mission 1 line, then centered JC's
  selected rifle response. The following window averaged 71.9 fps with a
  27.78 ms worst frame and an empty crash buffer.
- BSP rendering now uses each active map's serialized `Engine.Light` actors
  instead of one uniform shader value. Light position, radius, brightness, hue,
  saturation, surface direction, and spotlight rotation/cone are converted to
  Quest-space vertex lighting while preserving the texture-array layer channel.
  Training baked 150 lights (80 spotlights) across 145,620 textured vertices
  with luminance ranging from 0.075 to 1.346. Training Combat rebuilt a distinct
  set of 120 lights (31 spotlights) over 97,836 vertices with a 0.076-1.316
  range. Both maps returned to 72.0 fps/13.89 ms steady state and the crash
  buffer remained empty. This is a direct-light approximation; original UE1
  lightmap textures, BSP occlusion, and dynamic shadows remain future work.

## Verified on 2026-08-22

- The live Quest runtime retains 101,375 Unreal objects across 38 installed
  script packages: 1,502 classes, 8,208 functions, 24,441 properties, and
  1,028,925 normalized UnrealScript bytecode bytes.
- All 28 LodMeshes referenced by the training map decode on-device. Sixty-four
  placed actors render real mesh geometry; 43 of 44 referenced actor texture
  layers decode to a Quest texture array, with one procedural fallback.
- Runtime map replacement is verified across `00_Training`,
  `00_TrainingCombat`, and `00_TrainingFinal`, including collection of the old
  world. The visual BSP/material cache is still training-specific.
- A-button controller rays execute typed interactions. Inventory pickups mutate
  live object state, enter persistent inventory, and rebuild the GPU actor scene
  without the collected object.
- Right-index-trigger rays apply inherited pawn health and remove killed pawns.
  Both pickup and damage/death paths have reversible on-device startup tests.
- Y quick-saves and X quick-loads inventory, actor activity state, player
  position, and facing. The serialized runtime round trip is gated at startup.
- A head-locked Meta TinyUI HUD presents health, inventory count, and controls.
  The left Menu button opens a paused Persona-style VR inventory screen backed
  by the live runtime inventory. Its deep-teal/amber tab rail, 3x4 slot grid,
  item-data column, dividers, and action rail follow the visual conventions
  embedded in the shipped `DeusExUI.u`. Right-stick selection, A equip/use, B/Menu close, and
  quick-save/load are wired. Its local font atlas and texture bindings were
  validated with zero glyph or GL errors on Quest.
- A physical Quest 3 run held 72.0 fps in steady 10-second windows; worst
  steady-state frame delta was 13.89 ms with 24,270 collision triangles and 117
  interactive actors. No Android crash buffer entries were present.

## Verified on 2026-08-14

- Original Deus Ex installation left untouched.
- 220 user-owned UE1 data packages validated as present (738,318,285 bytes).
- Microsoft OpenJDK 17 and Android SDK installed.
- Android platform 32, build tools 35.0.0, NDK 27.0.12077973, CMake 3.22.1,
  Gradle 8.5, and ADB configured.
- Meta OpenXR `XrInput` reference sample built successfully for ARM64.
- Project smoke-test APK built successfully and signed with the debug key.
- APK package is `dev.deusex.questvr.smoketest` and contains only ARM64 native
  libraries, including the OpenXR loader, Meta sample runtime, and the custom
  `libdeusex_data_probe.so` loader probe.
- UE1 header inspection succeeds for `00_Training.dx`, `DeusEx.u`, and
  `CoreTexMetal.utx`; all report package version 68 with valid table offsets.

## Verified on Quest 3

- Device `2G0YC5ZG620985` was detected as `model:Quest_3`, codename `eureka`.
- The inventory panel was physically captured from the app's 1680x1760 eye
  buffer with the real `Multitool0` pickup visible, the gameplay HUD hidden,
  map/health/count correct, and the app steady at 72 fps after startup.
- A second physical capture verified the Persona restyle without overlapping
  panels, clipped text, or stereo-placement defects. The selected `{TOOL}` slot,
  `Multitool0` item data, navigation rail, and action prompts were all readable.
- The portable UE1 decoder now exposes arbitrary indexed texture images. On the
  physical Quest it decoded the six original `InventoryBackground_*` and six
  `InventoryBorder_*` assets from `DeusExUI.u` (256/256/128 by 256 pixels per
  row), composed them into their original 640x512 surface, and installed that
  texture directly on the VR menu. A captured frame verified the original black
  field and metallic edge rails without a GL error or process failure.
- A dedicated depth-safe Persona renderer now updates that surface with a 3x4
  selection grid and lazily decoded original `LargeIcon*` item artwork. A fresh
  physical-headset capture verified the real 32x64 multitool icon, selected-slot
  highlight, live item data, tabs, and action prompts together inside the
  original panel rather than as disconnected or occluded layers.
- Inventory icon fitting preserves each source texture's aspect ratio, and the
  resolver covers 69 verified shipped icons across weapons, ammunition, armor,
  augmentation items, weapon mods, tools, and consumables. The Quest capture
  confirmed the 32x64 multitool is centered without the former square stretch.
- The TinyUI glyph object and native artwork renderer now share one head-relative
  transform and differ only by local in-panel placement. A steep upward-head-pose
  capture verified that the header, item data, prompts, grid, and original frame
  remain rigidly registered instead of separating under pitch.
- The Persona rail now has functional Inventory, Health, and Goals/Notes pages.
  Right-stick left/right changes pages while inventory up/down selection remains
  isolated to Inventory. Health reads live health, credits, skill points, and
  inventory count; Goals/Notes reads save-persisted dialogue progress. Four
  physical captures verified all three pages, cleared stale slots outside
  Inventory, and restored the grid and icon after cycling back.
- Logs is now a fourth functional Persona page. Resolved NPC speech and confirmed
  JC responses feed a bounded conversation history, and quick-save metadata v4
  persists that history while retaining readers for metadata v1-v3. Physical
  validation recorded Jaime Reyes, saved, force-stopped and restarted the app,
  loaded, and captured the restored line under the active `[ LOGS ]` tab.
- APK installed with ABI `arm64-v8a`.
- The first device launch exposed a missing `libktx.so`; packaging was corrected
  to include the pinned SDK's ARM64 KTX libraries.
- The corrected process remained alive and entered an OpenXR session.
- The session reached `XR_SESSION_STATE_FOCUSED` and submitted 1,524 frames.
- It transitioned through `VISIBLE`, `SYNCHRONIZED`, `STOPPING`, and `IDLE`
  cleanly when Guardian/system UI took focus.
- 712 MB of user-owned game data was deployed into private app storage and the
  required training, script, texture, and music packages were verified there.
- The custom ARM64-native loader opened `Maps/00_Training.dx` and
  `System/DeusEx.u` from private app storage on the Quest itself and validated
  their UE1 signature, version 68, and package-table offsets.
- The training map reported 3,744 names, 3,347 exports, and 181 imports.
- `DeusEx.u` reported 12,872 names, 21,422 exports, and 3,336 imports.
- Complete name, import, and export tables were decoded on-device for both
  packages, including compact indices and object-reference validation.
- The training map's first export resolved to `LevelInfo0` of class `LevelInfo`.
  Its 70-byte serialized payload was opened at offset 55,744 and fingerprinted
  as FNV-1a `8a9c93bc`.
- The UE1 object state-frame prefix and tagged-property stream were decoded for
  `LevelInfo0`. Nine properties were found, beginning with `TimeSeconds`, and
  the decoder consumed the complete 70-byte object through its `None`
  terminator without overrun.
- The first `DeusEx.u` export, `DeusExPlayer`, was opened as a 17,523-byte
  serialized payload and fingerprinted as FNV-1a `039b4771`.
- Zero-class-reference exports such as `DeusExPlayer` are now distinguished as
  serialized `UClass`/`UStruct` definitions rather than instance properties.
- The single training `Level` export was resolved as `MyLevel` (3,906 bytes),
  whose serialized body references `Model36` as the root world model.
- The 3,085,440-byte `Model36` payload was decoded on-device using UE1 version
  68's embedded model layout: 601 vectors, 16,399 points, 9,524 BSP nodes,
  5,333 surfaces, 142,494 vertex references, and 8 zones.
- Geometry parsing validates compact indices, object references, array limits,
  and object boundaries; the Quest process remained alive without a crash.
- The Meta `XrInput` executable has been replaced by project-owned
  `libdeusex_quest.so`, while retaining the validated OpenXR lifecycle and
  tracked Touch controller rendering.
- The loader triangulates the root BSP directly from user-owned data and writes
  a private 3,393,240-byte runtime mesh cache. The OpenXR runtime loaded that
  cache into three bounded 16-bit GPU geometry chunks and submitted more than
  2,000 focused stereo frames without a native crash.
- Quest OS screenshot and screen-record APIs return black for this immersive
  compositor layer. A project-owned diagnostic command now reads the resolved
  left-eye swapchain image after rendering, writes a 32-bit BMP in app-scoped
  storage, and pulls it over ADB with `tools/Capture-QuestScreenshot.ps1`.
  Physical Quest 3 validation captured a 1680x1760 frame containing the BSP,
  actors, lighting, Touch controllers, and TinyUI HUD without stopping OpenXR.
- `MyLevel`'s actor array was decoded with 1,337 object references. The training
  spawn resolves to `PlayerStart1` at UE coordinates
  `(-1149.244, 825.844, -65.103)`.
- Runtime mesh generation now uses UE1's 52.5-units-per-meter scale and places
  the BSP around that PlayerStart instead of presenting it as a diorama.
- Head-relative smooth locomotion is mapped to the left thumbstick at 2.2 m/s;
  the right thumbstick performs latched 30-degree snap turns. The rebuilt app
  launched, loaded all three geometry chunks, and remained crash-free.
- A real pinned-SurrealEngine ARM64 target now builds its portable `File`,
  `NameString`, `StrTools`, and `PackageStream` layers with an Android exception
  shim. On-device execution opened the 4,431,551-byte training package and
  independently validated UE1 version 68 through `PackageStream`.
- The upstream package reader and writer share one desktop translation unit;
  the Android build uses a tracked reader-only translation unit with matching
  API/serialization behavior until the UObject/save writer graph is linked.
- The process remained alive and the Android crash buffer was empty after this
  device-side package check.
- A portable package-table layer now uses SurrealEngine's upstream
  `PackageStream`, `NameString`, `NameTableEntry`, `ImportTableEntry`, and
  `ExportTableEntry` types. It validates file/table bounds, object references,
  export payload extents, ANSI names, and UTF-16 names without requiring the
  desktop package manager.
- On-device table loads for both `00_Training.dx` and `DeusEx.u` matched the
  independent decoder exactly (3,744/3,347/181 and 12,872/21,422/3,336
  names/exports/imports respectively). The data gate returned true, all three
  BSP chunks loaded, OpenXR initialized, and the crash buffer remained empty.
- The portable layer now opens a bounded export payload, decodes UE1's optional
  execution-state frame, and parses canonical tagged-property headers including
  type, serialized size, struct name, array index, boolean value, and payload
  offset. On Quest, `LevelInfo0` independently decoded as nine properties
  consuming exactly 70 bytes, matching the established decoder.
- A focused physical-Quest baseline captured the clean launch and idle training
  scene in a 29,715,080-byte Perfetto trace. At idle the process used 172,354 KB
  total PSS and 319,900 KB RSS, including 128,116 KB attributed to graphics.
  OpenXR reached FOCUSED at 72 Hz. Android `gfxinfo` reported zero ordinary UI
  frames because rendering is submitted directly through the VR compositor, so
  it is not used as a headset frame-rate result.
- The OpenXR runtime now retains 24,270 unique training BSP triangles for
  collision, indexed into 2,153 two-metre spatial cells. Locomotion follows
  walkable floor triangles with bounded step/down ranges and rejects motion when
  a 28 cm, three-sample player capsule reaches wall-like surfaces.
- A 15-second physical-Quest Simpleperf run of the idle collision build recorded
  6,363 samples with zero loss. The capture represented about 10.6% of one CPU
  core across the whole debug app; the largest individual collision leaf
  functions accounted for 3.33%, 2.40%, and 1.59% of sampled app CPU. Collision
  indexing increased total PSS from 172,354 KB to 178,017 KB while graphics
  remained 128,116 KB. Movement feel and doorway/step behavior still require
  direct in-headset confirmation.
- Training BSP surface object references now resolve through complete import and
  export outer chains into 71 unique qualified materials. The first is
  `Cmd_tunnels.Metal.Ractivesign_1`.
- The portable asset layer resolves that material into `Cmd_tunnels.utx`, finds
  its grouped texture export, parses eight indexed mip levels (128x128 / 16,384
  bytes at the top level), follows its tagged `Palette` object reference, and
  decodes all 256 colors on Quest. The data gate, collision mesh, and OpenXR
  runtime remain healthy after the asset load.
- The world cache format now preserves BSP surface texture vectors, pan values,
  UV coordinates, and material slots. It separates the first real material into
  its own GPU chunk while retaining flat diagnostic chunks for unresolved
  materials; the cache is 5,242,364 bytes.
- Indexed pixels and the 256-color palette are converted to a private 65,552-byte
  RGBA cache. A project-owned UV/sampler shader uploaded the 128x128 texture and
  rendered one of four BSP chunks with it on Quest. OpenXR initialized and the
  crash buffer remained empty. The later in-app eye capture path makes UV and
  framing inspection available without relying on Quest OS compositor capture.
- The map now emits an authoritative manifest of all 71 qualified BSP materials.
  The portable decoder caches their source packages and successfully opens the
  mip and palette graph for all 71 on Quest. One (`Effects.water.drtywater_a`)
  is a procedural `UWaterTexture`; its empty serialized image is correctly
  classified and initialized from its `UClamp`/`VClamp` dimensions, matching
  SurrealEngine's `UFractalTexture` load behavior.
- All 71 materials are resampled into a 71-layer 256x256 RGBA texture-array
  cache (18,612,244 bytes). Native surface UV scale is retained per source
  dimension, and each vertex selects its texture-array layer. On Quest the
  complete BSP now renders through three textured chunks with no flat fallback;
  the array upload, mip generation, OpenXR initialization, and crash check all
  pass. Visual UV inspection remains an in-headset task.
- A 15-second Simpleperf run of the full-material idle scene recorded 6,638
  samples with zero loss, representing about 11.1% of one CPU core across the
  debug app. Total PSS was 210,234 KB and graphics 159,300 KB; collision hotspot
  proportions remained comparable to the pre-material build.
- The portable export-property reader now validates every non-null serialized
  actor referenced by the training `Level`: 1,308 actors and 16,285 tagged
  properties decode successfully on Quest. An authoritative histogram contains
  52 classes, led by 992 `Brush`, 80 `Spotlight`, 66 `Light`, 25
  `DeusExMover`, 19 `AmbientSound`, 18 `DataLinkTrigger`, and the expected AI,
  trigger, mover, inventory, camera, keypad, decoration, and NPC classes.
- Actor `AmbientSound` object references resolve across `.uax` packages into
  three unique UE1 sounds totaling 431,158 bytes. A 177,858-byte WAV is cached,
  decoded by the native runtime into 88,832 stereo frames at 22,050 Hz, and
  played through a low-latency AAudio stream on Quest. The stream reached
  STARTED before OpenXR initialization and the crash buffer remained empty.
  This initial cache was superseded by the live per-map spatial emitter mixer
  verified on 2026-08-23.

## Not yet verified

- Adding BSP collision/grounding and confirming player-scale framing and
  locomotion in-headset.
- Expanding the Android Surreal runtime from package streams into Package,
  UObject/reflection, property classes, and UnrealScript bytecode execution.
- Training map rendering or gameplay.
- Campaign compatibility.

## Next engineering gate

Expand the portable table runtime into export object streams and tagged
properties, then use it to resolve surface textures, actor classes, and actor
state. Add BSP collision/grounding and lightmaps before actor meshes, the VM,
and gameplay.
## Generic visual level replacement

- The on-device cache builder accepts any sanitized entry in the validated
  88-map catalog and regenerates the active BSP mesh and a Quest-budgeted 96x96
  material array.
- B advances to the next catalog map; a developer request file exercises the
  identical path through ADB without synthetic controller input.
- Cache generation runs in the background while the current world continues to
  render and the HUD reports `LOADING...`.
- Physical Quest validation completed Training -> TrainingCombat -> Training:
  TrainingCombat loaded 37 materials, 875 actors, and 16,306 collision
  triangles, then stabilized at 72 fps. Background runtime/texture preparation
  plus staged GPU replacement reduced the measured worst transition frame from
  1.22 seconds to 222 ms; further incremental uploads are still needed.
- Training decodes one outbound teleporter destination and TrainingCombat
  decodes two. Player proximity requests their real URL/DestMap destination;
  manual B cycling remains as a developer fallback.
- Version-2 quick-saves include the active map. Physical validation completed
  Training save -> TrainingCombat -> quick-load -> Training, restoring the map
  runtime and player transform after the asynchronous transition.
- Runtime-state v2 preserves player health and partial pawn damage in addition
  to inventory, inactive/dead actors, and activated movers/triggers. A physical
  100 -> 90 save -> 80 -> load test restored 90, and the HUD uses the live value.
- Right grip cycles persistent inventory and the HUD shows the selected object.
  Physical pickup validation selected `00_Training.Multitool0`; non-weapons no
  longer provide free hitscan, while weapon families set damage and range.
- The head-locked HUD now reports each A-button interaction for three seconds,
  including pickups, mover open/close, triggers, conversations, exits,
  unsupported actors, and missed rays; interaction results are no longer
  available only through ADB logs.
- A with no usable target consumes a selected medkit, food, or drink and applies
  item-specific healing. Physical validation picked up Training's real
  `Candybar0`, damaged health 100 -> 90, cycled to it, consumed it to reach 95,
  and reduced inventory from nine items to eight. Save -> damage to 85 -> load
  restored the post-consumption 95 health state.
- Firearms now require a compatible owned ammo class (10 mm, shells, .30-06,
  rockets, plasma, napalm, darts, or batteries); melee weapons remain ready
  without ammo. The HUD reports missing weapon/ammo, misses, invalid targets,
  hits, remaining target health, and kills. Physical Training validation blocked
  a selected multitool and accepted `WeaponCrowbar1` at 12 damage without ammo.
- Zero player health now enters a death state: locomotion, turning, travel,
  interaction, firing, item cycling, saving, and debug map cycling stop, while X
  remains available for recovery and the HUD prompts `DEAD - PRESS X TO
  QUICK-LOAD`. Physical validation damaged 100 -> 0 and quick-loaded back to
  the saved 100-health state without terminating the OpenXR process.
- The portable runtime now loads and validates 50,353 serialized conversation
  objects with 183,152 tagged properties: 1,955 conversations, 25,789 events,
  and 10,079 decoded speech lines. UE1 compact-length `StringProperty` values
  are normalized for both dialogue and map destinations.
- A mission/speaker index follows `ConversationList -> ConItem -> Conversation`
  ownership and `ConEventSpeech -> ConSpeech` references. Pawn A-button use
  resolves inherited `BindName`/`BarkBindName`, advances a per-pawn subtitle
  cursor, and exposes the real sound ID. Physical validation matched Training's
  Jaime Reyes to mission -1 speech 94 and Liberty Island's Paul Denton to
  mission 1 speech 314. Indexing occurs before frame submission; lookups did
  not disturb steady 72 fps/13.89 ms rendering.
- Dialogue indexing now traverses each conversation's real `eventList` and
  `nextEvent` chain rather than export-table order. Two consecutive physical
  Training requests advanced through adjacent `ConEventSpeech8844/8845` and
  sound IDs 263/264 while maintaining 72 fps; the index is built once before
  OpenXR frame submission and queried without rescanning conversation objects.
- Conversation `audioPackageName` and sound IDs now resolve through each custom
  `ConAudioList` object-reference tail to the real `USound` export. Bundled
  minimp3 decoding resamples mono/stereo speech into the active stereo AAudio
  rate and mixes it over ambience without looping. Physical Training validation
  resolved speech 263 to the 5,460-byte AIBarks MP3, decoded 44.1 kHz mono to
  20,160 frames at 22.05 kHz, queued it successfully, and held 72 fps with a
  27.78 ms worst dialogue window.
- Quick-save metadata v3 persists per-pawn dialogue cursors while still reading
  v1/v2 saves. Physical validation played Training event 8844, saved, advanced
  to 8845, loaded, and replayed 8845 rather than resetting to the first line;
  both referenced MP3 clips resolved and queued.
- Conversation indexing now attaches unconditional `SetFlag`, `AddGoal`,
  `AddNote`, `AddSkillPoints`, `AddCredits`, `Trigger`, and player-facing
  `TransferObject` events to the preceding authored
  speech while stopping at every choice, condition, random/jump, trade, or
  transfer boundary. The shipped corpus exposes 1,546 such safe effects across
  34,071 indexed dialogue lines and 1,256 speaker/mission keys. Effects are
  idempotent and runtime-state v3 persists flags, goals, notes, skill points,
  credits, and applied-event IDs. Physical Quest validation applied the flag
  following `ConEventSpeech9950` once, rejected a replay, quick-saved/loaded,
  and still rejected the replay while maintaining 72 fps steady state.
- Portable `NameProperty` decoding resolves real conversation flag, goal, and
  trigger names. Map actors are indexed by tag during background preparation,
  so a conversation trigger performs a direct lookup instead of rescanning and
  reopening the active map on the render thread. Physical Mission 1 validation
  executed the trigger after Paul's `ConEventSpeech448`; the indexed version
  reduced that dialogue window from 125 ms to 55.55 ms and returned to a steady
  72 fps/13.89 ms.
- Object and class properties now share validated compact-reference decoding.
  Of 244 serialized transfers, 243 resolve a player endpoint and their item
  class through `giveObject` or the authored `ObjectName`. A failed Training
  equipment removal applied nothing with empty inventory. In Mission 5,
  Miguel's `ConEventSpeech3684` granted its serialized item (inventory 0 -> 1),
  replay granted nothing, and quick-save/load preserved both the item and the
  applied-event ID.
- Material package lookup now searches the deployed UE1 package catalog rather
  than assuming every texture lives in `Textures/*.utx`. This fixed Mission 5's
  `DeusExDeco` import, which is actually `System/DeusExDeco.u` in this install.
  `05_NYC_UNATCOMJ12lab` physically loaded 101 materials, 3,043 actors, and
  37,099 collision triangles and stabilized at 72 fps. Its first uncached
  transition still peaked at 166.66 ms and needs further staging work.
- The portable conversation index resolves 109 authored response choices,
  including choice text, label, voice sound ID, display mode, skill/flag
  constraints, and the invoking NPC's next branch speech. An open choice pauses
  normal A-button use; the right stick selects an unconditional response and A
  confirms it, plays the shipped JC voice clip, and redirects that NPC's cursor
  to the selected label. Saving is refused while a response is open so a quick
  save cannot serialize a half-finished branch. Physical Mission 1 validation
  presented Paul's weapon response, selected "I'll take the rifle," queued
  sound 295, and resolved `ChoiceSpeechLabel_0` to Paul target ordinal 42.
- Choice availability now evaluates each serialized flag name and required
  boolean against the case-normalized, save-persisted conversation flag table.
  Skill-gated responses remain locked until real per-skill player levels are
  implemented. A physical Mission 1 regression still exposed Paul's valid
  weapon menu and held the dialogue window to 27.78 ms.
- Dialogue lines retain their conversation invocation mode. Of 34,071 shipped
  indexed lines, 12,123 belong to conversations explicitly marked
  `bInvokeFrob`; A-button NPC use prioritizes those and falls back to all lines
  only when that mission/speaker has no frob dialogue. Training's fallback
  Jaime Reyes line still resolved and played at a steady 72 fps.
- Dialogue MP3 decoding and resampling now run in a worker; the frame thread
  only performs a ready check and a short mutex-protected PCM swap. Shutdown
  waits for any in-flight clip. Repeating Paul's 80,964-byte Mission 1 line on
  Quest reduced its measured dialogue window from 250 ms to 27.78 ms; its
  298,944 stereo output frames became ready asynchronously. The selected rifle
  response then resolved to `ConEventSpeech392`, queued sound 295, and the next
  10-second window held 72.0 fps with a 13.89 ms worst frame.
- `01_NYC_UNATCOIsland` physically validated generic non-training loading: 107
  materials, 3,658 actors, 34,140 collision triangles, and two decoded exits,
  followed by steady 72 fps.
- Maps with more than 1,000 actors stream them within 25 m and refresh after
  10 m of movement. UNATCO Island initially instantiates 12 targetable actors;
  Training now instantiates 14 instead of rebuilding 119 meshes in one frame.
- World BSP buffers upload in triangle-aligned 4,800-vertex batches. Collision
  cells are indexed alongside each batch, and both world and actor texture arrays
  upload two layers per frame with explicit driver completion. Textured renderers
  share one retained shader program.
- On physical Quest 3, the largest measured island transition frame fell from
  1.22 seconds originally, to 471 ms after coarse collision optimization, and
  finally to 41.66 ms. It then holds 72 fps with a 13.89 ms worst steady-state
  frame. Island -> Training peaks at 27.78 ms instead of the previous 292 ms
  actor rebuild. Island save -> Training -> quick-load -> Island remains valid.
