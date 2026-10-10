# Restoring the Android build dependencies

The source repository excludes `third_party/`, Android build output, and the
commercial Deus Ex game data. A fresh checkout can restore its pinned source
dependencies without a Quest headset:

```powershell
.\tools\Initialize-ThirdParty.ps1
.\tools\Build-QuestSmokeTest.ps1 -ValidateOnly
.\tools\Build-QuestSmokeTest.ps1
```

The initializer reads the repository's `third-party-lock.json` and fetches the
exact commits into `third_party/Meta-OpenXR-SDK` and `third_party/SurrealEngine`.
It verifies both revisions on every invocation. Git and network access to the
locked repositories are required for the first restore. Once restored, checking
the pins needs no network access. Submodules, if declared by a locked revision,
are initialized at their recorded revisions.

Existing checkouts at the pinned revisions keep their local modifications,
including the project-owned TinyUI and session-lifecycle patches. Existing checkouts at a different
revision, submodules at a different revision, and nonempty directories without
a Git checkout cause an error. The initializer does not reset, clean, or switch
an existing dependency checkout. To resolve an error, inspect that checkout and
move it aside or explicitly resolve its revision yourself. A failed initial
fetch can be retried: the new checkout records that restore is still pending.
Never use this initializer as a way to discard dependency edits.

For automation or isolated validation, the initializer also accepts
`-DependencyRoot <directory>` and `-LockFile <file>`. The production CMake and
Gradle configuration use the default `third_party/` directory.

## Windows Android toolchain

Install a JDK 17 and an Android SDK containing:

- Android SDK Platform 32 (`platforms;android-32`).
- Android SDK Build-Tools 33.0.1 (`build-tools;33.0.1`).
- Android NDK 27.0.12077973 (`ndk;27.0.12077973`).
- CMake 3.22.1 (`cmake;3.22.1`).
- Android SDK Platform-Tools (`platform-tools`) for later Quest deployment.

Accept the Android SDK licenses through Android Studio's SDK Manager or
`sdkmanager --licenses`. The Gradle wrapper supplies Gradle 8.5; the Android
Gradle Plugin, OpenXR loader, and other Maven dependencies are resolved by the
versions in `android/build.gradle`. Their first resolution requires network
access. The build does not install the Android SDK or JDK automatically.

The build script accepts explicit installation paths:

```powershell
.\tools\Build-QuestSmokeTest.ps1 `
    -AndroidSdkPath 'D:\Android\Sdk' `
    -JavaHome 'C:\Program Files\Microsoft\jdk-17.0.20.8-hotspot'
```

Without those parameters it finds a complete Android SDK in this order:
`ANDROID_SDK_ROOT`, `ANDROID_HOME`, `android/local.properties` (`sdk.dir`),
`%LOCALAPPDATA%\Android\Sdk`, `Android\Sdk` under available filesystem drives,
and the parent of `platform-tools\adb.exe` on `PATH`. It finds a working JDK 17
through `JAVA_HOME`, installed Microsoft `jdk-17*` directories, or `java.exe`
on `PATH`. Stale environment paths are skipped; explicitly supplied invalid
paths fail with an actionable error. The script restores the caller's Java and
Android environment variables after the build.

`-ValidateOnly` checks these toolchain paths, restores/checks the pinned source,
and prepares both SDK patches without invoking Gradle. A normal build produces:

```text
android/build/outputs/apk/debug/DeusExQuestVrSmokeTest-debug.apk
```

Git operations, patch operations, Java version probing, and the Gradle build
check their native exit codes. Compile success does not demonstrate a playable
campaign; installation and runtime validation are separate steps. Original game
data must be supplied from the user's legally owned installation and must remain
outside the source repository.

## OpenXR/Android focus ordering

The session-lifecycle patch preserves OpenXR READY/begin and STOPPING/end rules
and the existing active-session invariants, but removes assertions that require
Android resume/pause commands to have already been consumed. The two event
queues can interleave during a real focus change; a 2026-10-10 Quest log captured
READY before the queued APP_CMD_RESUME and the old assertion aborted the game.
Starting/ending a session still follows the actual OpenXR event, not a fabricated
Android state or a swallowed OpenXR error. See the official
[xrBeginSession](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrBeginSession.html)
and [xrEndSession](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrEndSession.html)
contracts. The patch is checked against the pinned pristine SDK index and
idempotently checked on the working checkout; on-device focus tests are separate.

`tools/Launch-QuestSmokeTest.ps1` sends one `am start -W` intent, then checks the
process without sending another intent. It preserves app data and does not
claim map/tracking readiness from an activity launch alone.

Offline launch/install and fresh-patch regression tests require no headset:

```powershell
.\tools\tests\Test-QuestLifecycle.ps1
.\tools\tests\Test-ThirdPartyPatches.ps1
```

They cover serial authorization/selection, one-intent launch, nonzero ADB
failures, replace-install/data preservation, fresh and repeated patch application,
retained OpenXR contracts and conflicting-edit preservation. Generated fixtures
are confined to validated temporary directories, not the real SDK or device.
