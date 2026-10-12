# Engine-owned native client and Quest viewport

Added2026-10-12. This supplies original client/viewport entry state, not player
possession, accepted world startup or successful native decal rendering. The
original entry and bootstrap authority is in [DECAL-NATIVE-CONTRACT.md](DECAL-NATIVE-CONTRACT.md)
and [WORLD-BOOTSTRAP-CONTRACT.md](WORLD-BOOTSTRAP-CONTRACT.md).

## Ownership and teardown

`quest_native_client.h` retains one actual embedding-owned client and an ordered
fixed-capacity registry of at most16 logical viewports. A viewport holds bounded
dimensions, per-eye image count and generation identity, never GL/OpenXR pointers.
The native client lifetime is shared by renderer owner, startup worker and runtime
registry; only its irreversible atomic revocation is cross-thread. Registry
mutation and querying remain on the exclusive runtime CPU/script owner.

Quest captures one logical stereo viewport only after SDK SessionInit has created
both eye color swapchains, image/FBO/depth resources and agreeing dimensions/image
counts. It passes immutable metadata and a monotonic generation identity into the
startup worker. Binding follows InitializePortableRuntime, whose engine reset
clears any old client. Head tracking, focus and active-session flags do not stand
in for target creation or destruction. No PlayerPawn/Viewport.Actor is fabricated.

The tracked `meta-openxr-session-destroying.patch` adds an optional SDK hook before
any framebuffer teardown. Quest revokes the lease there; it clears global client
state only after the existing runtime-worker joins in SessionEnd. A dependent
script transaction checks the lease at commit entry and again after immutable
asset/tag preparation immediately before publication. Revocation aborts and
rolls back script/OUT/native writes without resurrecting the renderer lease.

The registry survives ordinary map replacement/unload and checkpoint application.
It is neither serialized nor map-scoped unsaved state. Engine reinitialization
and shutdown clear registration; the live owner must explicitly bind its new
session. Cached descriptors are not proof that the lease is still alive.

## Original native guard and configuration behavior

Actual Engine.Decal.AttachDecal has native index0/function flags0x500, required
Float TraceDistance0x80, optional Vector DecalDir0x90 and Bool return0x580. The
wrapper evaluates/copies each ordinary argument before later expressions can
mutate its source. Native-zero input snapshotting is declaration-specific;
unrelated/OUT native-zero aliases are not changed.

After argument evaluation and exact declaration validation, an absent client or
empty viewport registry returns original bool false before reading Texture,
clamping MultiDecalLevel, consuming RNG or accessing native geometry. A live
client/viewport then requires actual Actor.Texture schema; None logs the original
`AttachDecal: No Texture` and returns false. Nonnull Texture still reaches an
explicit unsupported backend error: geometry-only projection is not accepted as
Attach and no fake no-hit/success bypass is used. Const SurfList is untouched by
these guards. Detach/checkpoint behavior is unchanged.

Original GameInfo class precedence is URL GAME, then map DefaultGameType, then
configured DefaultGame if the client exists or DefaultServerGame if it does not.
Viewport count and player possession are irrelevant to this branch. The probe
options carry both configured defaults; an omitted required branch refuses
instead of substituting the other. Existing Level.Game bypasses class selection.

## Verification scope

Final host build and CTest pass58 ordinary tests (62 registered, four owned-data
skips),33.50s. Registry116/39 rejected mutations, VM677971/338 (66 new Attach
controls), generated Spawn/world/client7072/135 and SDK patch6/0 pass. All four
optional owned-data suites pass separately: runtime-save, defaults1002,
PawnList67 and full actor controls across Training/UNATCOIsland/Intro. The focused
world probe also retains its exact positive Attach refusal and complete rollback.
These are host proofs, not device or complete campaign acceptance.

Generated registry controls cover absent/bound-empty/live/revoked lifetimes,
ordered targets, stale/duplicate identities, exact limits and strong refusal.
VM controls cover earlier-input alias mutation, exact/mixed-case qualified native
identity, direct inputs, optional Nothing and unchanged foreign/OUT aliases.
Generated runtime controls cover original negative guards, argument effects,
post-native OUT-triggered revocation, save/map independence, default class
precedence and exact complete rollback. SDK patch fixtures prove pre-teardown
ordering, idempotence and preservation of conflicting local source edits.

The owned Training startup probe explicitly owns a synthetic stereo target so
its real Texture reaches the remaining positive Attach gap. That fixture is not
device rendering or an accepted playable-world result. APK/device evidence is
recorded in [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md); missing headset/tracking
prevents renewed physical acceptance, not these host checks.

Remaining: faithful positive Attach sequencing/live Texture ordering, primary
mover/SetBase callbacks and secondary paths; ordered renderer publication and
LastRenderedTime; original player login/possession/UI and complete campaign Tick,
travel archives, gameplay and Quest performance/visual acceptance.
