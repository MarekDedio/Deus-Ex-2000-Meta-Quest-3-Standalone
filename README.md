# Deus Ex Quest VR Port

This workspace is an independent ARM64/OpenXR runtime project for running a
user-owned copy of Deus Ex on Meta Quest 3. It does not modify or redistribute
the original game.

## Current milestone

Build and launch a native Android ARM64 OpenXR smoke-test APK, then establish a
read-only path for loading Unreal Engine 1 packages from user-supplied game data.

## Data boundary

- Source code, build products, and tests live under `QuestVRPort/`.
- Original maps, textures, music, audio, and compiled scripts remain outside the
  port source tree.
- Release packages must never contain Deus Ex assets.
- A player must provide their own legally obtained installation data.
- The Windows executables and DLLs are not used on Quest.

## Milestones

1. ARM64 Android/OpenXR application launches on Quest 3.
2. Runtime discovers and validates user-supplied game data.
3. UE1 packages, names, imports, exports, and properties can be read.
4. `00_Training.dx` geometry and textures render stereoscopically.
5. UnrealScript and Deus Ex native functions support the training campaign.
6. Motion controls, interaction, weapons, inventory, HUD, conversations, and
   saves are usable in VR.
7. All campaign maps pass progression, performance, and comfort testing.

This is a long-term engine port. A smoke-test APK is not presented as a playable
game build.

## Build the current Quest smoke test

The checked-in Android project references the pinned Meta OpenXR SDK checkout in
`third_party/Meta-OpenXR-SDK`. On this workstation, the reproducible toolchain is
installed at `D:\Android\Sdk` with Microsoft OpenJDK 17.

```powershell
.\tools\Build-QuestSmokeTest.ps1
```

With one Quest in developer mode connected and authorized over USB:

```powershell
.\tools\Install-QuestSmokeTest.ps1
```

The current APK proves native ARM64 packaging, OpenXR session creation, stereo
rendering, head tracking, and controller input. It does not load the game yet.
