# Port status

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
  Its local font atlas and texture bindings were validated with zero glyph or GL
  errors on Quest.
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
  compositor layer, so visual framing still requires in-headset confirmation;
  runtime logs verify active frame submission and GPU mesh initialization.
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
  crash buffer remained empty. UV orientation still requires direct in-headset
  visual confirmation because Quest OS immersive captures remain black.
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
  Per-emitter 3D attenuation and panning are not wired yet.

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
  88-map catalog and regenerates the active BSP mesh and 256x256 material array.
- B advances to the next catalog map; a developer request file exercises the
  identical path through ADB without synthetic controller input.
- Cache generation runs in the background while the current world continues to
  render and the HUD reports `LOADING...`.
- Physical Quest validation completed Training -> TrainingCombat -> Training:
  TrainingCombat loaded 37 materials, 875 actors, and 16,306 collision
  triangles, then stabilized at 72 fps. Background runtime/texture preparation
  plus staged GPU replacement reduced the measured worst transition frame from
  1.22 seconds to 222 ms; further incremental uploads are still needed.
