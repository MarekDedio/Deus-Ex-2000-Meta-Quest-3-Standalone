# Port status

## Offline script selection and eligibility (2026-10-08; not deployed)

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
