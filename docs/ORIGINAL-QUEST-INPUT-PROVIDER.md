# Pending Quest viewport input provider and original Input initialization

Static audit2026-10-28. **This service is not implemented.** This document does
not claim successful Input.Init, ResetInput, ReadInput, OpenWindow, a bound
render-device provider, original startup or live Training gameplay. The current
Console batch is separate. No original commercial DLL/EXE was loaded or
executed. This contract was written while compiler inputs were frozen; only
this new documentation file was added.

Immediate integration is the first `00_Training` map. Other-map work, account
interfaces and character-creation menus are not prerequisites for this batch.

## Correct original startup order

The earlier interpretation of GameEngine.Init1038972F as OpenWindow was wrong.
The actual sequence is:

1. UViewport's constructor constructs configured Canvas and calls Canvas.Init,
   then constructs configured Input. These are103731D9..10373223.
2. GameEngine constructs configured Console, publishes viewport.Console(+34),
   and calls Console.Init(viewport) at10389702..1038970A.
3. GameEngine calls the actual level's SpawnPlayActor at1038972F. The receiver
   is the level from GameEngine+68, NOT the viewport. The arguments are the
   viewport/UPlayer, role2, actual FURL and FString error output.
4. After a successful player spawn, GameEngine calls Input.Init(viewport) at
   1038975F..10389765.
5. GameEngine calls viewport.OpenWindow at10389768..10389776, passing
   `(0,0,-1,-1,-1,-1)`.
6. It then reads viewport.RenDev(+64).HighDetailActors(+64) and calls the actual
   level's DetailChange at10389779..10389785.

The call identities are independently authenticated by the original export
table and vtables, not inferred from argument count:

- ULevel UObject vtable export EAT721/RVA128AFC is10428AFC. Its+A8 cell10428BA4
  contains1030209A, the exact exported
  `ULevel::SpawnPlayActor(UPlayer*,ENetRole,const FURL&,FString&)` thunk,
  which jumps to10396BD0.
- UViewport UObject vtable10428F80+74 is purecall. WinDrv's concrete
  UWindowsViewport UObject vtable111126EC+74 contains11101181, the exported
  `OpenWindow(uint32,int,int,int,int,int)` thunk, which jumps to11107590.

This explains why ResetInput already dereferences a real viewport.Actor:
original player spawning/SetPlayer/Possess precedes initial Input.Init. An
Actor=None identity wrapper cannot initialize Input faithfully. Conversely,
an already-open window or nonnull RenDev is NOT an original prerequisite for
Input.Init; actual OpenWindow follows it. The concrete platform input service
must exist before logical window opening.

Keep the later LoadMap reset distinct. After its own SpawnPlayActor/SetPlayer/
Possess continuation, LoadMap calls Input.ResetInput at1038DFF3, followed by
conditional RenDev virtual+68(1) at1038DFF6..1038E001. Do not move that later reset
ahead of Possess merely to satisfy nested HUD/key queries. Existing constructor
and key-query services do not need a fabricated early reset.

Main-agent independent static order evidence is retained in ignored
`artifacts/console-startup-order-static-20261028.log`.

## Exact Input.Init and ResetInput

Configured Input is Extension.InputExt. Its UObject vtable1005C3EC inherits
the relevant native operations:

- Init+58 ->1005464E -> imported UInput.Init.
- ResetInput+68 ->10054642 ->IAT100592D8 -> imported UInput.ResetInput.
- ReadInput+64 ->10054648 ->IAT100592DC -> imported UInput.ReadInput.
- Process+60 is the distinct InputExt override1002D0B0, not base Process.

UInput.Init10301460 ->10393D30 stores the supplied actual Viewport at native
Input+EA0 at10393D5F. It calls virtual+68 at10393D65, then logs using the
actual Viewport's Name. Its native back-pointer store alone is not completed
initialization. A failed reset must roll back the pointer and all prefix work.

ResetInput10302A45 ->10394910..10394A7E performs:

1. Require a nonnull native Viewport(+EA0), checked at10394935..1039495A.
2. Clear all255 native Keys bytes at+EB0,1039495D..1039496E.
3. Read Viewport.Actor(+30), then Actor.Class(+24),10394970..10394979.
4. Walk actual inherited ByteProperty declarations. For each property with
   flags bit4, clear one byte at its actual native offset1039499C..103949AE.
   The iterator10303030 ->10394EC0..10394F15 tests actual property-class
   inheritance against Core.ByteProperty and follows real Children/Next/Super.
5. Separately walk inherited FloatProperty declarations and, when flags bit4
   is present, clear one DWORD at the actual offset10394A05..10394A1A.
6. Clear Action(+EA8) and Delta(+EAC) at10394A5D..10394A63.
7. Call the actual viewport.UpdateInput(1), virtual+7C, at10394A6B.

This is NOT a loop clearing all script inputs by name, ArrayDim slots, packed
Bool properties or Int properties. Preserve the actual declaration/type/flags
and inheritance authority. Native Action/Delta field use is independently
visible in InputExt.Process1002D186..1002D1B6 and1002D212..1002D243, including
clearing after its own command dispatch.

Base UViewport's+7C cell10428FFC contains104135CA ->IAT1059A348, `_purecall`.
Reset cannot substitute an empty callback. WinDrv's concrete+7C cell11112768
contains1110101E ->11109280, exported UWindowsViewport.UpdateInput(int).
That service exists before OpenWindow. It performs actual device polling and
key reconciliation; its reset argument is not a no-op notification. For
example11109853..11109879 conditions key acquisition on the reset argument
and actual window focus, then11109881..111098F0 reconciles held keys. The
Quest adapter needs genuine Quest behavior at this boundary, not Windows
handles, a claim to have run WinDrv, or an unauthenticated empty sample.

## Actual ReadInput remains a separate service

UViewport.ReadInput10303733 ->10374990 first requires Input at viewport+60.
For delta other than-1 and ordered, it calls actual UpdateInput(0) at103749EF.
Equal-1 or unordered/NaN skips that platform update, but all cases still call
Input.ReadInput(delta,*GLog), virtual+64, at10374A03.

UInput.ReadInput103020EF ->10394580..10394849:

- Tests actual GIsRunning first. False skips its cache/held-key/scaling body,
  NOT UViewport's preceding Input requirement or eligible platform update.
- Caches ALL inherited actual UProperty declarations with flags bit4, not
  just floats or a hard-coded PlayerPawn basename. Its native cache key is
  uint64(Class.ObjectIndex)*0x100+0x1F. Portable caching needs actual class,
  declaration and runtime-generation authority, not reusable raw PE pointers.
- For ordered delta other than-1, held Keys[0..254] call the actual configured
  Process(output,key,action2,raw delta),1039475C..10394785.
- Scales only actual FloatProperty values, one native offset each, with
  PC64 Double20.0/Floatdelta, or Double+0 for-1/unordered,103947B7..10394829.
  Multiply then store Float: no clamping or replacement by assignment0.
  Signed zero, infinity, NaN and post-input scaling remain significant.
- Releases its acquired property-cache lock before returning.

InputExt.Process first offers the real Root input handler1003B3E0 when the
actual PlayerPawnExt/root relationship exists. A consumed event clears held
keys and runs nonempty release bindings. Its unconsumed path preserves press/
release duplicate suppression, key-state updates, Action/Delta and its own
Exec call/clear ordering. Do not replace this by base Process or direct Fire.

The existing quest_player_local_tick.h preserves pre-input(raw delta), checked
PlayerInput, checked PlayerTick, then post-input(-1), rereading actual Player
before both calls. The post-input call must not repoll a Quest sample. Do not
force GIsRunning, clear Player or choose a nonplayer path to bypass this work.

## OpenWindow and RenderDevice authority

WinDrv.OpenWindow11101181 ->11107590 checks actual viewport.Actor at
111075B5..111075D4 and additional native preconditions at111075D7..111075F4. Its body
creates/attaches the native window, then establishes rendering. At111078F2
it checks viewport.RenDev(+64). If absent, it attempts appropriate real
configured render-device construction; it asserts RenDev remains nonnull at
111079C7..111079E3.

The owned original choices include GameRenderDevice=GlideDrv.GlideRenderDevice
and WindowedRenderDevice=SoftDrv.SoftwareRenderDevice. Actual WinDrv literals
are1111C0B0=`ini:Engine.Engine.GameRenderDevice`,1111C050=
`ini:Engine.Engine.WindowedRenderDevice` and1111C114=
`SoftDrv.SoftwareRenderDevice`. These desktop choices do not authenticate a
Quest render device.

TryRenderDevice1110104B ->1110D150 first retires any old renderer. It resolves
the selected actual class, constructs the device, stores viewport.RenDev at
1110D235, then calls its virtual+5C Init with viewport/dimensions/color bytes/
fullscreen at1110D247. Failed initialization destroys the device and clears
RenDev at1110D29D..1110D2AE. A temporary pointer store is not proof of successful
driver initialization.

Quest needs an explicitly identified, genuinely bound port-owned native
rendering adapter and lifecycle, with real render-owner target authority and
actual settings/provider semantics. Do not instantiate abstract original
Engine.RenderDevice, impersonate Glide/Software, infer RenDev=None from Engine
constructor defaults, or make an arbitrary wrapper decline Exec. Original
URenderDevice.Exec10371B60's behavior is valid only when an authenticated
provider really inherits/forwards that behavior. All earlier FExec providers
must retain their actual order before Core SET; see ORIGINAL-INPUT-FEXEC-SET-GAP.md.

## Current port seams

- RuntimeNativeInputInstance in portable_unreal_runtime.cpp retains configured
  values, constructor provenance and initialized/retired markers. It currently
  lacks held Keys[255], Action/Delta and an initialized-state protocol.
- ActualNativeInputInstanceValid and NativeInputSnapshot in
  portable_viewport_input.inl deliberately reject initialized=true or a
  nonnull native Input.Viewport. Split/authenticate constructor and initialized
  services rather than weakening those refusals globally.
- portable_input_mutation.inl now owns full-instance copy-on-write beforeimages
  with original payload ownership and GC roots for owner/viewport/Enum/native
  Viewport. Extend that real journal for input-state writes and pointer/state
  publication; do not add nonjournaled initialization flags.
- RuntimeObject.Mark already follows nativeInputInstance.values.viewport.
  Preserve rollback roots, shared work/state budgets and revoke/teardown rules.
- portable_actor_pawn_tick.inl authenticates actual native Player binding and
  reciprocal viewport.Actor, then explicitly refuses the unprepared ReadInput
  service. That refusal is not an implementation of Input.ReadInput.
- quest_main.cpp::CaptureNativeViewport checks actual stereo OpenXR swapchains,
  images/depth/framebuffers and matching dimensions on the render owner. Its
  NativeViewport descriptor exports metadata only, not a platform UpdateInput,
  OpenWindow or RenDev capability. The CPU worker must not dereference GL/XR
  handles merely because the descriptor was registered.
- quest_main.cpp::SyncActionSets has real focus, head/controller tracking and
  controller observations. Current presentation locomotion/fire consumes these
  through a competing path, which must be retired when the original committing
  player pipeline takes ownership to avoid duplicate motion/fire/physics.

## Minimal faithful next batch and acceptance

The smallest next service is an explicit revocable Quest viewport input-provider
binding, callable before logical OpenWindow, followed by transactional
Input.Init/ResetInput after genuine original player spawning. Rendering and
ReadInput command execution remain separately required, not implied successes.

The binding must authenticate the same current client, viewport generation and
render-owner lifetime. Samples must come from the real Quest owner with bounded
sequence, focus/tracking state and explicit event/axis units. An unknown or
missing provider is a refusal, not a manufactured zero-event sample. A genuine
provider may report a valid inactive/zero-input state; do not confuse that with
absence. Controller-to-original-key/axis mapping is an explicit port adaptation,
not a recovered PC packet-unit guarantee.

Keep the handed-off samples immutable and bounded. Stage CPU consumption/cursor
changes inside the root journal and commit them only with that root. Rollback
must restore held state, Action/Delta, reflected Actor fields, native Viewport,
initialization state, retained capacity/bytes and sample consumption; hardware
polling itself cannot be undone. Tracking/focus loss must release held actions
through the real admitted protocol, not leave a stuck Fire state. No render
handle access or renderer-thread runtime mutation is authorized.

Acceptance before claiming this batch complete:

- Missing/stale/revoked providers and Actor=None wrappers refuse without prefix
  publication. Valid initial Init requires the already-initialized Console and
  genuine SpawnPlayActor/SetPlayer/Possess order, not a detached test pawn.
- Generated protocol controls cover reset/poll order, focus/tracking loss,
  duplicate events, sample sequences, bounded queues and every mutation/failure
  prefix. Test observation providers are explicitly fixtures, not proof of a
  real Quest adapter.
- Owned first-Training controls verify the authentic Byte/Float declaration
  inventory, all255 keys and Action/Delta reset, unchanged aliases/bindings,
  initialized native back-pointer/GC edges and exact late-LoadMap reset order.
- Full rollback controls compare GC, lookup identities, naming counters, native
  inventory capacity/bytes, world revision and complete actor-save bytes.
- Shared write/work/retained-memory limits cover both beforeimages and working
  storage. Client revoke/rebind and map reload cannot retain stale capabilities
  or serialize transient platform/Input/render objects into actor saves.
- A real headset acceptance run must verify the actual sample source and reset/
  release effects. Separate later controls must prove actual RenDev/OpenWindow,
  complete InputExt/ReadInput, command routing and the committing frame bridge.

Until those implementations and controls exist, preserve explicit refusals. No
new APK, working live crate destruction/loot/gravity or completed Training demo
is established by this audit or by successful Console initialization alone.
