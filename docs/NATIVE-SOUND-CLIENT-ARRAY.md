# Original local sound uses the actual Client array

The local/simulated Actor.PlaySound264 path now authenticates current
Level.Engine and the initialized reciprocal Engine.Client/Client.Engine pair.
It iterates that Client's actual live viewport UObject array, rereading the next
member after each synchronous listener callback. Registered XR descriptors are
target provenance only; they provide neither listener count nor order.

The caller root retains the exact Client graph and irreversible lifetime token.
Sound checks that root during delivery, and the existing root commit checks
protect publication. Clear followed by fresh binding with the same numeric IDs
does not revive old listeners. A genuine Engine.Client=None, initialized empty
array, or actual Viewport.Actor=None remains an empty-recipient success; absent
native Engine authority or a detached constructor-only Client refuses.
Nonlocal PawnList/nextPawn routing and the existing packet/hearing math are
unchanged. This is not a native audio driver or audible sound-channel consumer.

`portable_sound_routing_test` retains generated nonlocal packet, live PawnList,
callback mutation, BSP occlusion and full rollback coverage. Its separate owned
mode loads the unchanged 38 original script packages with a generated helper
package/map. Verified byte-identical copies of the original packages and INI
live in the isolated temporary fixture root: save-source validation must find
the generated map alongside the actual Actor class source without changing
production authority or writing into the owned installation. The first owned
run exposed that source-root mismatch; its failed log is retained.
Actual native Client/Viewports and real SetPlayer dispatch select
explicit generated test Possess/ClientHearSound overrides. That fixture proves
local callback routing; it does not prove original Login/Possess, first-Training
startup, sound playback, physical controller input or crate gameplay.

Owned controls cover actual Client=None, initialized Client with zero targets,
metadata A,B with no native viewports, actual unpossessed B, actual B,A order,
paired packet delivery, synchronous source mutation proving B-before-A, later
listener failure rolling back earlier effects, revoked lifetime and fresh same-ID
binding without revived actors. Callback-time lease revocation remains covered
by the separate root-graph regression, not claimed as this fixture's own case.

Run from the repository root:

```powershell
.\desktop\build\portable_sound_routing_test.exe
.\desktop\build\portable_sound_routing_test.exe --owned --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

CTest registers `portable_sound_routing_owned`; without DEUSEX_GAME_ROOT it
explicitly skips with77. The first-Training startup probe also initializes the
actual empty Client before target registration/NewViewport and verifies exact
Client/Engine/index/array preservation after rollback. It remains nonpublishing.

Verification2026-11-04: focused desktop build and corrected generated2688/24
and owned72420/two suites pass; production Android native link and strict ARM64
runtime pass. Full desktop rebuild/CTest pass263 executed/323 registered/60
explicit owned-data skips/zero failures in236.60s. Actual first Training retains
its exact later post-partition refusal in591.794s with267089 instructions/
38087 writes, complete Input/reset prefix and full-root rollback. Final audit
finds1940 frozen compiler inputs,45 originals,3 selected assets and4 source DLLs
unchanged, with zero live jobs and no new APK. The expected later refusal is
not playable startup; no headset acceptance is implied by these host results.

The subsequent [Quest output transport](QUEST-AUDIO-OUTPUT-TRANSPORT.md) adds
an exact real-output capability and owner/callback PCM handoff. It is separate
from this routing work and does not yet implement the original AudioSubsystem
UObject/channel selection or make first Training startup/crates playable.
