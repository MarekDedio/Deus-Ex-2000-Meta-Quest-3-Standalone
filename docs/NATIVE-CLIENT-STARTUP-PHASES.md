# Native Client and viewport startup phases

Implementation batch2026-11-04, focused and broad verification passed. Immediate integration
target is the first `00_Training` map. This is an engine startup prerequisite,
not an account login or character-creation interface, and not a playable APK.

## Separate identities

`BindPortableRuntimeClient` owns the revocable embedding lifetime;
`RegisterPortableRuntimeViewport` registers bounded stereo-target metadata.
Neither operation constructs a game Client or adds a native Viewport UObject.

`PreparePortableRuntimeClientObjects` now explicitly constructs the actual
Client and performs the audited base Client.Init Engine-pointer store. It
requires the prepared genuine Engine and a live owner lease, but no registered
target. It does not require the Player/Actor schema or register Viewport's
UClass. It creates no Viewport, Canvas, Console, Input, Pawn or window.

`ReadPortableRuntimeClientSession` reports actual native viewports in their
array order, separately from `registeredTargets`. Its `initialized` value is
derived from the authenticated Client.Engine/Engine.Client pair, never from a
synthetic readiness bit, target registration, tracking or possession. A typed
session-owned payload retains the actual Engine backpointer and client
identity. GC marks that edge; session byte accounting includes its storage.

`PreparePortableRuntimePlayerObjects(viewportIdentity)` remains the explicit
native viewport-materialization entry. Its original constructor-only fixture
profile can operate before Engine preparation, and such a Client is reported
honestly as uninitialized. The explicit Client-only entry can bind the real
Engine later without replacing an existing Client or viewport identity.
Engine construction itself no longer imports half of that pair from an earlier
constructor-only session: explicit Client.Init owns both edges together.

## Atomic ownership and teardown

Both preparation entries share one staged publication path. Client-only mode
skips Player/Actor validation and viewport allocation. Requested viewport mode
validates the real metadata and registered target before staging its UObject,
payload and array append. Identity/header snapshots, lookup records, map
capacity and package-byte admission are prepared before publication. A final
lease/lookup/Engine-authority check precedes nonthrowing publication of the
indexes, session arrays and reciprocal Engine edge. Session preparation is
refused inside an active VM root; it cannot create unjournaled native state in
the middle of a script transaction. Already initialized Client preparation and
already materialized viewport preparation are idempotent.

Client teardown clears Engine.Client and the typed Client payload, retires
the concrete objects and clears viewport payloads. Persistent native UClasses
survive until engine shutdown; active transaction roots are not prematurely
collected. Actor checkpoints do not serialize these engine-session objects.

## Verification scope

The new owned regression covers zero-target Client initialization, no implicit
Viewport UClass/instance or player/UI/input allocation, real Engine identity,
metadata-only registration, explicit ordered viewport appends, idempotence,
missing-Engine/unknown/stale/revoked refusals, whole checkpoint/lookup/GC
protection, late Engine construction followed by explicit Client.Init,
teardown and more than16 initialized sequential client lifetimes.
The first run exposed a fixture error: untyped ANY_PACKAGE lookup matched
`Engine.Console.Viewport`, a Property rather than a UClass. The revised
Class/Engine-Outer lookup then reproduced the real late-Engine half-binding
failure. Both failed runs are retained. Corrected focused desktop and
Android-native builds pass, as do strict ARM64 runtime/helper checks. The
expanded owned regression passes10698 checks/98 refusal controls, including
the late-Engine upgrade and all18 sequential generations. Console5505/34 and
input-alias80/10 and Root input300/10 also pass. The actual first-Training
regression passes its expected-boundary check in675.572s with267089 original
instructions/38087 writes: Init/ReadInput/laterReset checks remain valid, complete
save/state/revision/hash/RNG/client/lookup/Actor/GC rollback passes,45 original
sources are unchanged and immutable Model replacement starts cold. It still
reaches the same explicit post-partition client/player/detail refusal. No
startup is published. The full desktop rebuild passes. CTest passes262 executed
of321 registered, with59 explicit owned-data skips and zero failures in184.54s;
the relevant owned regressions above ran separately. Final audit verifies1934
frozen compiler inputs,45 originals,3 selected assets and4 source DLLs unchanged,
with zero live native jobs and no new APK. These are not playable-world results.

The source contract is in ORIGINAL-QUEST-INPUT-PROVIDER.md: original UClient
construction creates an empty viewport array, base Init stores Engine, and
actual NewViewport later appends its instance. PE32 offsets identify the owned
source behavior only; portable layouts are not inferred from them. Original
commercial binaries remain read-only data and are never loaded or executed.

## Remaining production integration

The Quest CPU bootstrap now explicitly prepares the actual zero-target Client
before registering target metadata, checks that registration creates no native
viewport, and then explicitly materializes the viewport without a Pawn. The
matching owned first-Training Engine regression passes42656 checks/34 refusals
using the exact38-package Quest source set; Android-native linking also passes.
This verifies the existing dormant-demo seam, not original initial startup.

This API split does not complete initial GameEngine.Init. The Android demo
still loads a dormant map. Original initial map startup, explicit viewport
construction/Canvas/Input/Console/player sequence, concrete audio/render
readiness, detail callbacks and the final URL/config tail remain required.

Startup/sound/decal consumers still need actual-array migration. The subsequent
root-lifetime checkpoint replaces raw commit guards with exact graph/lifetime
authority and migrates the distance prepass to the actual Client array; see
NATIVE-CLIENT-ROOT-LIFETIME.md. It does not equate registered targets with created
viewports or claim that all consumers are migrated. Local-player Walking/Falling, tracked pose/aim, original
weapon routing and committed crate/loot/debris scheduling remain separate live
Training integration work. See ORIGINAL-PLAYER-WALKING-CONTINUATION.md.

The follow-on root-lifetime checkpoint addresses the identified raw HasClient
rollback hazard after Clear/same-ID rebind, Input COW owner resurrection and
Canvas/Console peer restoration with nonthrowing structural-owner predicates.
Its owned fixture is genuinely unpossessed/uninitialized: positive player/Init
receipt/nonzero physical-state retirement remains unproven, as does revocation
during config backend preparation. Lookup refusal alone is not used as proof
that retired graph references are safe. See the follow-on document for the exact
verified scope and remaining production requirements.
