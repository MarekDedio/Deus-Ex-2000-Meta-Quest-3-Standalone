# Runtime-visible Quest voice feedback

Implementation scope:2026-11-04. The original native audio Update needs to know
whether a submitted backend voice is pending, playing or finished. Previously
that word lived only in the owner's mixer; the CPU runtime could not query it.
This batch moves the atomic words into the exact per-output capability already
shared by the real owner and runtime. It does not create an AudioSubsystem
UObject, implement original channel COW/spatial/clock/resource rules, publish
Training startup, finish crates or package a new APK.

## Ownership and observable state

`AudioOutputIdentity` retains32 packed atomic64-bit words. Each acquire/release
word contains voice ID and playback state together; readers cannot combine a
new state with another voice's ID. The capability also has an atomic exclusive
mixer claim. Owner reset can attach/transfer only to an unclaimed capability;
failure leaves both the current voices/retirement and the destination consumer
unchanged. Detach and owner destruction clear feedback and release the claim.
The owner must exclude/join the real callback before reset/destruction, as before.
No mutex, UObject, decoder, allocation or ownership release is added to MixAdd.

Concurrent readers use `AudioOutputTransport::Feedback`, not mutable mixer
storage. The transport's immutable exact identity cannot change underneath the
read. Equal generation/Client/Viewport numbers do not alias feedback from another
transport. `MixerAttached` is separately observable: a host-confirmed endpoint
without a consumer must not be misreported as an initialized audio driver.

`ReadPortableRuntimeAudioOutput` first authenticates the real bound runtime/
lookup/Engine/Client/Viewport provenance, then exposes those per-slot words and
the actual claim state. Each slot is coherent, but the array is deliberately
not described as one frozen callback frame. Stream revocation still means
ready=false even if historical feedback says Finished or Playing. A matching
numeric voice/generation alone is never output authority. A future original
audio Update must also distinguish an old/mismatched feedback ID from the
pending replacement in its own committed native channel state.

The Quest startup owner's existing future-get barrier now checks that the
returned native binding has its actual exclusive mixer attached before attaching
prepared content. Owner reset failures retire/refuse the transport rather than
silently accepting a second consumer. Existing music, Jaime, HUD, hands/NPCs
and map presentation are unchanged by this feedback-only connection.

The lightweight `quest_audio_voice_protocol.h` holds only the shared capacity,
voice ID bound and state packet. The public runtime header no longer imports
the full queue/mixer/PCM implementation; its private implementation and the
owned transport test include that dependency explicitly.

## Verification boundaries

The host transport test adds denied live-consumer transfer, preserved current
PCM, successful owner detach/transfer, destructor release, equal-generation
feedback isolation and a concurrent third-thread runtime reader during24,000
SPSC publications/mixed frames. The strengthened reader must actually witness
Playing before the first voice may advance, preventing an Empty-only vacuous
pass.373 checks/50 refusals and ten complete strengthened repeat runs pass.
Endpoint/WAV158/12 controls pass. This is real host PCM
math/protocol evidence, **not** AAudio hardware readiness or audible Quest output.

The separate original-package owned fixture checks runtime-visible Empty before
publication, queued-versus-owner Pending, actual positive PCM -> Playing and
natural Finished, truthful externally revoked readiness, and empty feedback
for a fresh same-ID native lifetime. Its explicit host start confirmation is not
headset evidence. Generated sound2690/24, owned output binding/local sound72548/12
and owned startup Client protocol72880/21 pass. Focused/full desktop and production
Android native builds pass; strict ARM64 runtime/helper checks pass. CTest passes
264 executed/325 registered/61 explicit owned-data skips/zero failures in79.14s.
The earlier suite passed in121.58s; only the host stress test was strengthened
after it. Production inputs are identical between the two frozen manifests.

Final integrity evidence uses `artifacts/audio-feedback-final-audit2-20261104.json`
with frozen inputs in `audio-feedback-frozen2-20261104.json`. Build/test logs use
the `audio-feedback-` prefix. No new APK was packaged or installed.

The last real Training probe remains the preceding transport batch's exact
unfinished post-partition startup boundary/full rollback. This feedback-only
batch does not change that startup branch. Its original Audio UObject/provider,
committing initial driver and live first-map crate/loot/gravity integration are
still required. Other-map integration remains deferred.
