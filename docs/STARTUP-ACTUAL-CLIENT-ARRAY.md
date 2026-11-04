# Startup selects the actual Engine Client and viewport array

The original standalone startup continuation now bases configured local versus
server GameInfo selection on the actual current Level.Engine.Client edge.
It authenticates the initialized reciprocal Client.Engine ownership rather
than treating a raw render-owner token as a native Client. URL and authored
DefaultGameType precedence remain unchanged.

After the original live world callbacks and Actor partition, the player setup
loop rereads that actual Client's live viewport UObject array in ascending
order. It supplies each actual viewport identity to the existing original
SpawnPlayActor/Login continuation. A target descriptor is not a player slot.
An initialized Client with zero actual viewports chooses local DefaultGame,
runs zero player Logins and still reaches the original non-Entry brush-tracker
continuation. A genuine Engine.Client=None chooses DefaultServerGame and does
not enter the Client-only tracker branch. Revoked/stale native authority is an
error, not an invented headless/no-players case.

## Test ownership and preserved coverage

The existing generated Spawn callback fixture deliberately has no prepared
native Engine. It now selects its test GameInfo explicitly through URL, retains
its world callback/live growth/deletion/partition/base/rollback controls, then
refuses actual Client access at its genuine missing-authority boundary. It
does not use raw rendering descriptors as an Engine substitute.

The original Login packet/long OUT/class-selection/late failure controls move
to the owned hybrid in `portable_sound_routing_test --startup`, reusing that
fixture's package writer and isolated byte-identical original source copies.
CTest names this mode `portable_startup_client_owned`; without an explicit
owned game root it skips with77. The fixture constructs real native Engine,
Client and Viewport objects. Its generated GameInfo lifecycle/Login overrides
are explicit test scripts, not completed retail GameInfo/JC startup providers.
Login returns real script None; no synthetic player is paired.

Controls include actual Engine.Client=None, raw token/descriptor-only absence,
initialized empty Client with zero targets, metadata A,B with no actual players,
URL/map/config precedence, actual B,A Login order distinct from metadata A,B,
missing Login input, ordered CLASS/user/INI fallback, missing/unmaterialized
classes, malformed secondary PlayerPawn constraint, growing4096-byte OUT Error,
late native999 refusal, final OUT-copyback write cutoff and complete outer-root
undo. Revocation and fresh same-ID raw binding may not revive old native arrays.

Run from the repository root:

```powershell
.\desktop\build\portable_sound_routing_test.exe --startup --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

## Verification2026-11-04

Focused desktop builds, production Android native link and strict ARM64 runtime
pass. Corrected owned startup72880 checks/21 refusals, generated Spawn13404/221,
generated sound2690/24 and owned sound72421/two pass. Full desktop rebuild and
CTest pass263 executed/324 registered/61 explicit owned-data skips/zero failures
in174.68s. The two older owned world/collision fixtures compile with explicit
Client.Init prerequisites; their complete independent legacy suites were not
rerun by this focused startup batch.

Actual first Training retains its exact later post-partition refusal in522.945s,
267089 instructions/38087 writes and native work14254762/16777216. The real
Init/reconciliation/pre-post/rate ReadInput/later Reset prefix and complete
save/state/revision/hash/RNG/client/lookup/Actor/GC undo pass, with45 original
sources unchanged and cold Model replacement. Final audit finds1940 frozen
compiler inputs,45 originals,3 assets and4 source DLLs unchanged, zero live jobs
and no new APK. This is an expected-boundary diagnostic, not playable startup.

The first two owned runs reached the final revocation assertion but failed its
expected error wording. Revocation is rejected during actual native UObject
lookup (`Portable native session UObject is revoked or stale`), earlier than
the separately checked root-publication lease. The corrected fixture requires
that observed exact early refusal, empty GameInfo/Login observations and full
rollback; no runtime guard was weakened. Both failed runs are retained.

The existing startup probe still deliberately rolls back at the later missing
post-partition client/player/detail boundary. This change does not implement
native Audio/render providers, initial GameEngine ordering, live player physics,
crate gameplay, headset acceptance or a new APK.
