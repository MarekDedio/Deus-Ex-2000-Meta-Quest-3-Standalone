# Real Quest audio endpoint before startup worker

Implementation2026-11-04; focused, strict ARM64 and broad verification passes.
No new APK, real-device readiness, native Engine.Audio or playable
startup is claimed by this prerequisite.

## Used production path

SessionInit now opens a real initially silent48kHz/stereo/I16 AAudio stream on
the app owner and validates its returned format. RequestStart is asynchronous:
owner Update polls actual state without blocking Android/XR event pumping. Only
observed STARTED or a positive-frame callback from that stream can confirm its
capability ready. Terminal states and independent errors defeat historical
callback proof. A nonzero sample rate, requested start, focus, tracking or a
registered viewport cannot stand in for readiness.

The CPU preparation future launches only after that check. It receives the
exact revocable endpoint capability and immutable format, never AAudio handles,
callback userdata, app pointer, GL or XR handles. It rechecks capability before/
after preparation; future get remains the ownership barrier before publication.
Prepared ambience attaches later to the same generation without close/reopen.
Missing optional WAV content is empty content, not a fabricated ready stream.

Each stream has immutable callback userdata holding its own capability. Error
callback only records/revokes that generation; it never stops/closes/waits or
looks up the app's newest token. The data callback outputs silence while Pending
or Revoked. Owner teardown revokes first and keeps userdata alive through close.
Startup failures clean partial resources and use the existing failed-start HUD.
No worker is replaced or runtime shut down before its future has been consumed.

Delayed map/music/dialogue results require the exact ready endpoint, not merely
an equal rate. A consumed dialogue future also consumes its captured capability.
Map replacement checks revocation before mutation/completion and before staged
publication. This is not a native multi-Level audio provider or completed travel.

Optional8/16-bit mono/stereo WAV is bounded and resampled on the CPU worker to
the actual output rate. Stereo channels, signed samples and equal-rate identity
are preserved; malformed chunks, partial frames and excessive allocations refuse.

## Verification and remaining requirements

The first host build invocation failed because the existing generated build tree
had no rule for the newly added target. Explicit reconfiguration then passes;
the failure is retained. Focused host protocol/PCM tests pass158 checks/12
refusals, including concurrent start/revoke, stale generations, stereo up/down-
sampling,16-bit extrema, truncated WAVs and bounds. These are protocol tests,
not simulated hardware acceptance. Production Android compile/link and strict
ARM64 helper pass. The coherent full desktop build passes, and CTest passes263
executed/322 registered/59 explicit owned-data skips/zero failures in163.01s.
Final audit verifies1940 frozen compiler inputs,45 protected originals,3 selected
assets and4 source DLLs unchanged, with zero live jobs and no new APK.

On Quest, verify actual format -> real start proof -> worker launch -> same-
generation content attachment, initial silence, music/Jaime, independent output
failure and repeated sessions without stale callback/decoder publication. Current
callback mutex/realtime performance is not improved or accepted by this split.

Still required: concrete authenticated Quest AudioSubsystem UObject/Class and
Engine.Audio edge, rooted endpoint binding, bounded native channel COW and
commit-reserved commands, original GetViewport/SetViewport (including channel
stops before same-viewport equality), NoteDestroy and ClientHearSound. Do not
instantiate an abstract original class, impersonate Galaxy, use a fake endpoint,
force Audio=None to bypass work, or expose audible rollback side effects.
Original initial startup/player/render/detail/weapon/frame/crate integration
remains unfinished. First Training is still the immediate gameplay target.
