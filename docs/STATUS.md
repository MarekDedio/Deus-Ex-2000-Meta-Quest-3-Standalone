# Port status

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
- The process remained alive and the Android crash buffer was empty after this
  device-side package check.

## Not yet verified

- Confirming in-headset framing, then switching the diorama mesh to player-scale
  geometry positioned at the training PlayerStart.
- Training map rendering or gameplay.
- Campaign compatibility.

## Next engineering gate

Decode the training actor collection and PlayerStart transform, render the BSP
at UE1 player scale, and add locomotion/collision. Then resolve surface texture
objects and lightmaps before moving into actor meshes, the VM, and gameplay.
