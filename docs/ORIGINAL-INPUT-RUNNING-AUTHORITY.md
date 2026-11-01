# Input running-state authority and port loop binding (2026-11-01)

Source audit followed by an explicit port-loop capability implementation. No
original commercial module was loaded or executed. Build/test acceptance is not
yet claimed here. This does not complete Input.ExecInputCommands, Init/ReadInput,
original startup, gameplay frames or an APK.

## What is known from original source

Core.dll exports `?GIsRunning@@3HA`; its export RVA is `0xE555C`, or original
preferred VA `0x101E555C`. Engine.Input.ExecInputCommands imports that actual
global through IAT10599D98 and reads it once AFTER allocating its lexical frame,
before parsing any command line. It does not recheck running between lines.
Input.ReadInput independently checks the same original global at its own entry.
The detailed selected command body is in ORIGINAL-INPUT-EXEC-LINES.md.

The owned DeusEx.exe is the known packed64000-byte launcher, SHA256
`E7217090C435C7AE01F875A08FA25C10303CD56203C25A29C6E77FB041B992A2`.
Its visible imports are only kernel32 loader/allocation routines. No original
launcher's exact assignment timing for GIsRunning is authenticated here. The
owned Core.dll SHA256 remains
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
External objdump read these files as PE bytes only.

Consequently, a port must identify its own real execution-loop owner and make
its adaptation explicit. Neither a guessed original startup value nor a
callback that always returns false supplies that authority.

## Existing port state is not an equivalent

- `persistentGameScriptsEnabled` is the separate launcher GIsScriptable policy.
  InitializePortableRuntime sets it from successful package/reflection import;
  ShutdownPortableRuntime clears it. OriginalGameScriptsEnabled uses it for
  script ProcessEvent. It does not prove entry into the game execution loop.
- `runtimeAvailable_` becomes true in CompleteInitialPreparation after the
  worker future is joined and presentation assets are received. That path has
  not committed original GameEngine startup. It is not GIsRunning.
- `Focused`, head/controller tracking, mounting and SessionActive describe XR
  input/session observations. Loss of any one is not process-loop termination.
- `NativeClientLifetime` authenticates real stereo target/session lifetime. It
  is deliberately created independently of focus/tracking, revoked before eye
  target destruction, and replaced for a later XR session. An engine process
  may outlive that target session, so this token is not the running global.
- `bBegunPlay`, presence of a spawned PlayerPawn, nonnull Input, successful
  metadata construction and the absence of a startup exception are also not
  substitutes. In particular, the current startup still intentionally rolls
  back at an explicit remaining-provider guard.

## Concrete real owner seams

The pinned Meta SampleXrFramework source is local primary source:

- XrApp.h ENTRY_POINT creates the actual application and calls its Run method.
- XrApp.cpp Run1699 establishes the activity loop context, then calls MainLoop.
- MainLoop1522 performs Init, InitSession, the real outer while-loop, EndSession
  and Shutdown. A SessionActive=false condition only continues that outer loop;
  it does not mean the process is not running.
- XrApp::Init calls the port's AppInit; XrApp::Shutdown calls AppShutdown.
- EndSession calls SessionDestroying before releasing stereo targets, then
  SessionEnd. That is a different lifetime boundary from the application loop.

In quest_main.cpp the corresponding concrete hooks are AppInit, AppShutdown,
SessionInit, SessionDestroying and SessionEnd. SessionInit sends only immutable
descriptors/shared token/source into PrepareInitialRuntime; ValidateInitialRuntime
owns InitializePortableRuntime and all native preparation on the CPU worker.
CompleteInitialPreparation::future.get is the later exclusive-CPU handoff. These
are usable places to carry a separate explicit loop-state capability; no GL/XR
handle access or runtime mutation on the producer thread is needed.

## Explicit implemented embedding handshake

This is an explicit PORT policy, not a claim that the packed PC launcher used
the same callback timing:

1. The actual application/Run owner constructs one distinct ProcessLoopControl
   with a monotonic identity. The tracked SDK app-loop lifecycle patch adds
   AppLoopStarted AFTER InitSession and BEFORE the actual outer while-loop,
   and AppLoopStopping AFTER that loop and BEFORE EndSession. The real hooks
   publish Ready -> Executing -> Stopped, not headset/package observations.
2. Pass the same shared control through the actual startup worker capture.
   After InitializePortableRuntime has established its new engine generation,
   an explicit CPU-owned bind stores that exact control/identity/generation.
   Missing state stays unavailable, not false. Runtime reinitialization first
   unbinds the old capability; map replacement/save/load does not invent a new
   process or copy this transient state into gameplay archives. Ready is a
   genuine explicitly bound pre-loop false state: SessionInit's worker may
   reach binding before the outer-loop entry hook runs.
3. Keep execution bool and irreversible capability revocation DISTINCT.
   Explicit LeaveExecution publishes genuine false. Revoked, missing, foreign
   or stale-generation capability is an error, not a legitimate false branch.
   An atomic producer may change execution state; only CPU-owner binding
   storage mutates on the worker/frame owner. Account its control and binding
   bytes with the existing shared state budget.
4. ExecInputCommands first admits its actual lexical frame, then obtains ONE
   authenticated running observation. True parses/routes every selected line;
   false legitimately returns without reading Action or demanding dispatch
   providers. A later child callback changing the bool must not suppress the
   rest of this invocation. Reentrant invocations take their own observation.
5. Pin exact runtime/control identity and lifetime through the caller's outer
   root and check those identities at publication. Do NOT demand that the bool
   remain unchanged: that would contradict the original once-per-invocation
   observation. External process state is not actor-COW state and is not
   restored by a failed gameplay transaction. Input cursor/keys/Actor writes
   still roll back through their actual existing journals.
6. Actual process exit publishes LeaveExecution before joining outstanding
   workers, then revokes/releases its capability after those owners are done.
   Session target destruction separately revokes the NativeClientLifetime.
   ShutdownPortableRuntime unbinds loop authority before retiring UObject/GC
   storage. AppPaused/focus/tracking loss/map loading do not silently publish
   LeaveExecution. A new app/process cannot revive a revoked control.

Binding true only after completed original startup would unnecessarily make
startup-time native input calls lie about a live embedding loop. Conversely,
marking the current presentation-ready flag as completed original loop startup
would fabricate progress. Name and document the real port-loop transition
actually implemented; original world-startup success remains a separate gate.

## Required controls for an eventual implementation

- No binding, wrong control with matching numeric identity, wrong runtime
  generation, replaced/revoked control and shutdown/reinitialize all refuse.
- Explicit live true takes the full actual Input/Viewport routes; explicit live
  false does not read Action, parse or reach missing dispatch callbacks. These
  are explicit controlled fixtures, not evidence of on-device producer timing.
- Admission failure occurs before running is queried; one successful invocation
  reads running once. A child changes running false after the first line and
  the outer invocation still routes later lines; a nested invocation observes
  the current false. Identity revocation remains a separate invalidation.
- Failure after actual Input/Actor/COW writes restores complete save/revision/
  GC/lookup/key/cursor state but does not rewind an external producer's state.
- Actual Android entry/exit hooks must produce diagnostic observations tied to
  the same control bound by the CPU worker. Focus/tracking toggles must not
  change execution state. Session recreation and worker cancellation must not
  retain a stale target or runtime capability.

The new quest_process_loop_test and portable_process_loop_test cover typed
concurrent transitions and actual runtime binding/publication/rollback controls.
The patch helper and isolated third-party patch test include the optional
framework hooks, genuine ordering, idempotent application and conflict refusal.
Frozen verification passes the pure concurrent component79 checks/9 refusals
and owned binding57/15. Actual native Input alias integration76/10 independently
checks Ready/Stopped genuine false and missing/revoked authority refusal. The
SDK patch workflow passes8 cases/no hardware/no actual checkout mutation.
Strict ARM64, Android-native/full desktop and final CTest pass; see STATUS.md
for full scope and source-integrity counts. Physical Android hook timing still
requires headset/app acceptance. Missing running authority remains an explicit
refusal; pure parser callbacks alone cannot authenticate this lifecycle.
