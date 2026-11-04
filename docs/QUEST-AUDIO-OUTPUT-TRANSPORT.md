# Quest audio output transport

Implementation scope: 2026-11-04. This is the real output handoff required by
the native audio provider, not that complete provider and not a new APK.
Original first Training startup and live crate damage/loot/gravity remain
unfinished. No headset acceptance is inferred from host tests or native links.

## Production connection

`quest_main.cpp` constructs an output transport for the exact initially Pending
AAudio endpoint, retained Client lifetime and Viewport identity. No PCM or batch
can be prepared/published until the endpoint has actual successful-start proof.
Each real callback retains its own immutable endpoint/transport context, never
a lookup of the newest stream. Existing ambience, music and Jaime playback are
preserved; native PCM adds to the existing ambient buffer before music/dialogue.
An empty native mixer leaves the existing samples unchanged.

After actual reciprocal Client.Engine/Engine.Client initialization and explicit
native Viewport creation, the CPU startup worker binds this capability to that
actual runtime/lookup generation and Client.Viewports member. Numeric IDs or XR
target descriptors alone cannot bind it. The runtime exposes a truthful snapshot
of output readiness, exact native tuple, pending batches and separate PCM usage.
It does **not** construct an AudioSubsystem UObject or assign Engine.Audio.
An identical binding is idempotent only at an exclusive CPU boundary; replacement
requires Client clear/runtime shutdown, which revoke the transport before native
roots/lookup identities are removed. Closing the real stream remains the app
owner's responsibility, after joining producers and retaining callback userdata
through the AAudio close barrier.

## Transaction and real-time boundaries

`quest_audio_transport.h` has a fixed 16-batch SPSC queue, up to 64 commands per
batch, and 32 mixer slots. A CPU producer reserves a queue slot before its final
fallible publication boundary. Cancelling/destructing the reservation publishes
nothing. `RequirePublishable` is the fallible final readiness check;
`PublishPrepared` only releases an already-owned slot, without allocation or a
decoder. A future original-world/config commit must reserve and validate before
publishing config, then use the no-throw release after that boundary. This change
does not yet add that channel-state journal to the VM.

Each transport has an unforgeable identity capability, including the exact PCM
budget. Queue admission and direct owner mixer admission both reject foreign
clips/batches even with identical numeric generations. Batch validation happens
before any mixer mutation. Uncounted fixed slots cannot retain hidden resources.
Prepared immutable stereo I16 PCM must match the output rate, contains at most
4,194,304 frames per clip and shares a 64 MiB aggregate resource budget including
retained vector capacity/object storage. PCM usage is not hidden in VM reference
accounting; fixed output capability/queue metadata is charged separately.

The app owner pops committed batches outside its existing audio mutex and applies
only fixed prepared state under it. Replaced/finished PCM goes into fixed owner
retirement storage and is released after unlocking. The callback never allocates,
decodes, touches UObjects, releases PCM ownership or adds another lock. It advances
unsigned 32.32 source cursors with explicit looping, linear stereo interpolation,
prepared per-channel gains and signed-16 saturation. These are backend mechanics,
not a substitute for the source-authenticated native priority/spatial math.

Play feedback is Pending on owner admission, Playing only after a positive PCM
frame is actually mixed, and Finished on natural completion. Stale voice IDs
cannot stop/update a replacement slot. Finished PCM remains retained until owner
retirement. Revoked transports cannot enter the callback mixer; the owner drains
their queue and resets voices. The Pending-start path preserves the prepared
mixer identity rather than accidentally retiring a not-yet-started stream.

## Verification and remaining work

`quest_audio_transport_test` is a pure host protocol/PCM test, **not** real AAudio
or headset evidence. It covers reserve/rollback/commit, capacity/reuse, exact
capabilities, malformed-batch atomicity, resource budget/retirement, stereo gains,
interpolation/looping/one-shot/clipping/feedback and 24,000 deterministic concurrent
producer/consumer publications and mixed PCM frames.

The separate owned `portable_sound_routing_test --owned` uses actual original
runtime classes with the existing explicitly generated helper map/callbacks.
Added output-binding controls check missing actual Client/Viewport, metadata-only
targets, Pending endpoints, wrong retained owner, exact idempotence, in-root bind
refusal, separate PCM/reference charges, external revocation, same-ID fresh
lifetimes and Client clear/shutdown. Its start confirmation is deliberately a
host protocol fixture; it never pretends to be AAudio hardware readiness.

The concrete Quest AudioSubsystem UObject/Class and initialized supported virtual
profile, original channel COW/counter, source-specific spatial/clock/resource
rules, ClientHearSound/NoteDestroy/LoadMap coupling and commit-reserved channel
publication remain to be implemented together. Existing explicit unsupported
native branches remain unsupported; this transport does not turn them into a
successful fake provider. See [the original source contract](ORIGINAL-AUDIO-DEFAULTS-AND-CHANNEL-UPDATE.md).

Run the added host checks from the repository root:

```powershell
.\desktop\build\quest_audio_transport_test.exe
.\desktop\build\quest_audio_endpoint_test.exe
.\desktop\build\portable_sound_routing_test.exe --owned --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

Verified 2026-11-04:

- Helper:326 checks/50 refusals, including24,000 SPSC publications/PCM frames;
  ten additional complete repeat runs pass. Endpoint/WAV:158 checks/12 refusals.
- Generated sound routing:2690/24; owned output-binding/local sound:72543/12;
  owned startup Client protocol:72880/21. All are terminal passes, with the
  generated callback/hardware-evidence boundaries described above.
- Focused/full desktop builds, production Android native link and strict ARM64
  runtime/helper checks pass. CTest:264 executed/325 registered/61 explicit
  owned-data skips/zero failures in166.68s; skipped tests are not passes.
- Real first Training:823.155s under concurrent build load,267089 instructions/
  38087 writes, native work14254762/16777216, original initial Input/reset prefix,
  exact existing post-partition Client/player/detail refusal and complete rollback
  with45 originals unchanged. This verifies the expected unfinished boundary,
  not committed startup, output-device readiness or live crate gameplay.

Compiler inputs were frozen throughout these jobs. Final integrity evidence is
under `artifacts/audio-transport-final-audit1-20261104.json`; build/test logs use
the same `audio-transport-` prefix. No new APK was packaged or installed. The
remaining full provider/startup/live-gameplay requirements above are unchanged.
