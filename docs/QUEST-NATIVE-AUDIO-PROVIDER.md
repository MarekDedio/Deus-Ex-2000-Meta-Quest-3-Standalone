# Quest native audio provider

## Status and acceptance boundary

This describes the current source implementation of the concrete Quest sound
provider. It is not a report that first Training now starts and plays through on
the headset. The provider, its native runtime integration and the current build
are still being verified. No new APK or headset acceptance is claimed here.

The short-term gameplay target remains the first Training map: genuine startup,
interactive and destructible crates, authored loot and gravity, with the native
sound calls those interactions require. Passing audio helper tests alone does
not establish that gameplay target or completion of the full game port.

## Concrete Class, defaults and instance

`native/portable_native_audio_api.inl` prepares an explicit
`Engine.QuestAudioSubsystem` derived from the actual `Engine.AudioSubsystem`.
It does not instantiate the abstract base as an output implementation, load the
retail Galaxy DLL, or pretend the Quest class is `Galaxy.GalaxyAudioSubsystem`.
The existing explicit Galaxy device selection in the bound read-only engine INI
selects this audited platform mapping; an unknown selection is refused.

Preparation requires the genuine native Engine, its sound-enabled and vacant
Audio slot, actual registered Core/Engine Classes and Packages, live object
lookup, and the already-started authenticated output with an attached consumer.
The provider reads bounded explicit channel, volume, ambient, Doppler and stereo
configuration. Effects channels are bounded to the transport's 32-slot capacity.

Both the base and Quest class-default objects are real rooted runtime objects.
Each has its actual Class/default-owner edge and superclass-default edge. The
derived default uses the staged base default directly, before either existing
Class is mutated. Mutable payload construction finishes before installation
into the immutable prepared-default storage. Defaults remain nameless: they are
not inserted as ordinary named objects into the object hash. The Class and
transient `QuestAudioSubsystem0` instance are staged for genuine namespace and
lookup publication.

The domain retains the Class/default/instance/Engine graph. Admission checks
Class metadata, actual defaults, namespace ownership, Engine.Audio, the reciprocal
Engine/Client/Viewport/output tuple and the lookup generation. Numeric output or
viewport IDs alone do not confer authority. Output consumer epochs additionally
fence a cleared or replaced mixer, including a successful reset of the same
attached consumer. A retired provider cannot be silently rebound to a replacement
output; it requires complete native runtime teardown.

Name creation uses the real `NameStringArenaStage`, with bounded work and scratch
admission. Pending names are read through their staged spelling and are only
published through `Validate` followed by `Commit`. Net committed name-arena
growth is retained in the process-lifetime `nativeAudioNameArenaCredit`, rather
than disappearing from accounting when the provider is released. Namespace
replacement buckets and snapshot strings are admitted before allocation; the
final publication transfers nodes into reserved empty maps, then publishes the
actual lookup and Engine.Audio edge.

## Native call and transaction pipeline

The native ClientHearSound bridge now reaches the concrete provider instead of
declaring original audio publication unsupported. Channel selection and backend
playback are separate: selection fills native channel state; Update prepares the
real Sound resource and stages the corresponding PCM command.

Every mutating provider operation belongs to the caller's VM transaction. The
first mutation journals a copy of the channel/resource/viewport state and roots
the existing referenced UObjects. One final command per slot coalesces
uncommitted Play, Update and Stop operations. No selection callback stops an
audible voice immediately.

Before outer commit, the provider validates the live ownership tuple and
consumer epoch, reserves a bounded transport batch, verifies publication
readiness, and performs retained-state checks. The root's user-config publication
remains the last fallible boundary. Only afterwards does the nonthrowing
`PublishPrepared` suffix make the reserved audio commands visible. A failed
transaction cancels the reservation and restores the journaled native state;
uncommitted PCM commands never become audible. External output retirement is
not undone by rolling back the native state.

SetViewport stops every live channel before testing viewport equality, so even
the same-viewport call is not a no-op. A changed viewport must be the actual
initialized output target. Its resource inventory is cleared transactionally;
the Quest adaptation keeps the already-open output stream. The authored
Song/nonzero-transition setup is journaled through the VM when the original
changed-viewport condition requires it. This does not by itself implement the
retail music backend.

Actor destruction preserves the original distinction: ambient owner channels
are stopped, while other sounds detach only the Actor pointer and keep their
sound, ID, cached location and voice. The identity check accepts the still-live
UObject at the correct post-world-slot-removal point, rather than confusing
`bDeleteMe` with an already-destroyed object.

## Real Sound resources and decoding

Resources are resolved from the actual admitted Sound UObject and its source
package export. The export's Class must be `Engine.Sound`; a string path or
synthetic placeholder is not a substitute. The provider retains at most 256
prepared resources. Encoded package data is read only, decoded on the CPU owner,
and associated with PCM prepared by the exact output capability. Resource
references and retained PCM have separate accounting; decoding is not performed
in the real-time callback.

`native/portable_native_sound_decode.cpp` supports these bounded profiles:

- RIFF/WAVE PCM8 and PCM16, mono or stereo, at native source rates from 8 kHz
  through 192 kHz. Mono is duplicated into stereo; signed PCM samples and source
  frame rate are preserved rather than pre-resampled to the output rate.
- MP3/MP2 format-tagged resources decoded by the existing minimp3 path, within
  the same source-rate, channel and frame bounds. Malformed tails or decoder
  rate/channel changes are refused.
- At most 64 MiB of encoded input and 4,194,304 source frames per decoded clip.
- One forward, integer-position, infinite WAV `smpl` loop. The file's inclusive
  loop end is converted to an exclusive source-frame end. Duplicate, multiple,
  fractional, finite or non-forward loops and invalid/truncated extents are
  refused, not silently converted to whole-file loops.

The transport retains native-rate PCM. Its explicit fixed-point phase step
represents source rate, pitch and the Quest Doppler adaptation relative to the
actual output rate. A loop plays its prefix once, then wraps only within its
declared region. Interpolation across the region end uses the loop beginning,
not an excluded tail sample. Large steps wrap within that region. One-shot
commands reject loop markers; Update cannot change a playing voice's markers.

The callback does not decode, grow resource containers or destroy the last PCM
owner. Replaced/stopped PCM is retired to the non-callback owner. Exact transport
capability and consumer-epoch fencing prevent equal numeric generations from
authorizing foreign resources or an obsolete mixer session.

The decoder exposes bounded-work and workspace-admission callbacks. Workspace
reports exclude borrowed encoded bytes, so the host must combine them with its
still-live encoded input and other staging state. WAV output is admitted from
its actual validated frame count. MP3 first counts genuine frames with an
allocation-free iterator, then admits the codec's 4096/doubling index capacity,
including old-plus-new realloc storage, before opening the decoder. Subsequent
admissions cover actual index/source/output coexistence before vector allocation.
Callback refusal from inside frame preflight returns to the C++ owner before it
is rethrown. The Sound-export loader separately accepts a bounded working-byte
allowance covering raw export, properties and encoded-copy preparation.

The frozen-4 resource boundary tests verify these pre-allocation admission
changes, including refusal at each decoder admission stage and the Sound-export
minimum-passing versus minimum-minus-one working allowance. Decoder hard caps
alone are not the evidence: the callback and loader boundary fixtures exercise
the actual admission path. Full gameplay acceptance remains separate.

## Source-derived semantics versus Quest output adaptation

`native/quest_native_audio_channels.h` carries source-derived game-side behavior:

- Radius assertion precedes ordinary null-viewport/null-sound exits.
- SLOT_None advances the fresh free counter and generates its whole channel ID.
- Existing-ID matching ignores bit zero; the no-override bit refuses replacement.
- Slots are scanned in ascending order. Priority comparison preserves the audited
  native scalar/float-store boundary, updates the running threshold, and lets a
  later equal-priority candidate win.
- The selected channel is stopped before assignment. Selection itself starts no
  backend voice; Update owns that step.
- Exact voice feedback marks finished playback. The completed-voice branch stops
  and immediately continues before elapsed or spatial work.

Update uses actual listener/Actor positions, ambient eligibility and maintenance,
elapsed time and source priority. The concrete backend then deliberately adapts
these to Quest stereo: equal-power left/right gains from listener yaw, bounded
distance gain, configured sound/speech volume, reverse stereo, collision-based
occlusion fade and bounded ambient Doppler phase adjustment.

That stereo backend is **not Galaxy DSP parity**. It does not claim the retail
pan quantizer, HRTF, Windows mixer/driver behavior, reverb, or sample-for-sample
output equivalence. Original channel control behavior and an explicit portable
output profile are different claims and must be tested separately.

## Explicitly unsupported boundaries

Ambient light-type modulation requires the authenticated native renderer
service; the current path refuses that case instead of substituting a guessed
brightness curve. General Galaxy/native music-device behavior is not established
by this Sound implementation or by earlier audible Training music adapters.

Live map unload/travel is refused while the provider still owns a viewport,
resources or source/voice channels. Correct transactional audio retirement and
travel ownership are not implemented by simply clearing pointers before the
existing unload operation. Other pre-existing map-travel guards still apply.

An unavailable, revoked or differently owned output is an error, not permission
to silently disable Audio. Helper fixtures or a zero-voice startup do not satisfy
the genuine native startup or playable-crates acceptance requirement.

## Verification: frozen 4, 2026-11-13

The following results belong to the frozen-4 compiler inputs, not to binaries
left over from the earlier implementation:

- Strict runtime and resource compilation both exited zero. Evidence:
  `artifacts/native-audio-provider-strict-runtime4-20261113.log` and
  `artifacts/native-audio-provider-strict-resources4-20261113.log`.
- The Android native build exited zero:
  `artifacts/native-audio-provider-android-build4-20261113.log`. This is native
  compilation evidence, not an assembled or installed APK.
- The focused desktop build completed successfully. Fresh executable timestamps
  were checked before the rebuilt resource suite. Evidence:
  `artifacts/native-audio-provider-focused-build4-20261113.log` and the
  frozen input manifest `artifacts/native-audio-provider-frozen4-20261113.json`.
- The rebuilt resource suite passed all four tests in 4.14 seconds:
  `portable_sound_sources`, `portable_sound_sources_owned`,
  `portable_native_sound_decode`, and `portable_native_sound_decode_owned`.
  The pure decoder reports 530 checks / 149 refusals; the pure Sound-source
  helper reports 71 checks / 27 refusals. Evidence:
  `artifacts/native-audio-provider-resource-tests4-rebuilt-20261113.log`.
- The three pure audio helper tests each passed ten consecutive repetitions,
  1.72 seconds total, with the transport rebuilt from current source. Evidence:
  `artifacts/native-audio-provider-helper-repeat4-20261113.log`.
- The rebuilt routing suite passed all three tests: pure routing, owned routing
  through the genuine callback/provider (329.54 seconds), and owned startup-client
  mode (145.35 seconds), 479.31 seconds total. Evidence:
  `artifacts/native-audio-provider-routing-tests4-rebuilt-20261113.log`.

The owned decoder test confines itself to actual `00_Training.dx` imports and
their Sound packages, and verifies the source files remain unchanged. The owned
routing/provider result proves the tested native call and transaction path, not
headset audibility or successful execution of the complete game startup.

The earlier `native-audio-provider-resource-tests4-20261113.log` was produced
before the new focused executables finished linking and is **not** evidence for
these memory-admission changes. Its scope is recorded in
`artifacts/native-audio-provider-resource-tests4-early-scope-20261113.md`; use the
explicitly rebuilt resource log above for this batch.

One earlier helper repetition under heavy parallel compilation exceeded its
30-second feedback observer deadline. The unchanged idle repeat-3 run and the
fresh repeat-4 run each passed ten repetitions. That load-dependent timeout is
preserved as an observation, not hidden by extending a deadline or widening a
cap. Evidence: `artifacts/native-audio-provider-helper-repeat3-20261113.log`,
`artifacts/native-audio-provider-helper-idle-repeat3-20261113.log`, and repeat-4
above.

## Remaining verification

On November 20 neither the full desktop build nor the real first-Training run
was still active. Neither log contains a final result: the build stopped during
compilation and the Training run initialized the concrete host audio provider
but did not report its rollback boundary. Do not treat either as a pass. Logs:
`artifacts/native-audio-provider-full-build4-20261113.log` and
`artifacts/native-audio-provider-real-training4-20261113.log`. A final integrity
check matched all 1,954 frozen compiler inputs and the protected game files.
Completion of the
owned startup-client mode is not completion of the whole native game-startup
sequence.

A newly compiled APK, installation on a connected Quest, audible native output,
and a real first-Training crate destruction/loot/gravity walkthrough remain
unverified. No headset capture, new APK or user acceptance has occurred for this
implementation batch merely because this document exists.
