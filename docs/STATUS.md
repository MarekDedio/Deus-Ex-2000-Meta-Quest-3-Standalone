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
  libraries (`libopenxr_loader.so` and `libxrsamples_xrinput.so`).
- UE1 header inspection succeeds for `00_Training.dx`, `DeusEx.u`, and
  `CoreTexMetal.utx`; all report package version 68 with valid table offsets.

## Not yet verified

- Device installation or runtime launch: no authorized Quest was attached.
- Loading UE1 packages inside the Android process.
- Training map rendering or gameplay.
- Campaign compatibility.

## Next engineering gate

Connect an authorized Quest 3 and validate OpenXR launch, tracking, controller
input, suspend/resume, and thermal stability. In parallel, isolate the portable
UE1 package/object/VM subset and compile it for Android ARM64, then load
`00_Training.dx` from app-private storage without bundling commercial data.
