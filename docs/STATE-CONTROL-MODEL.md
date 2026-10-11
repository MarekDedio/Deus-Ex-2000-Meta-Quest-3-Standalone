# Independent original actor-state control

This increment separates selected state, running code and nullable execution in
the shared runtime and its saves. It is backend work, not automatic world
startup, player possession, live NPC scheduling or campaign acceptance. The
latest successful APK builds install automatically when the authorized Quest
is connected; installation and physical acceptance remain separate checks.

## Original contract and representation

Read-only inspection of the user's Core.dll establishes FStateFrame's running
Node (+04), nullable Code (+0c), selected StateNode (+18), positive 64-bit live
ProbeMask (+1c/+20) and native latent action (+24). Its constructor sets Node
and StateNode to the receiver Class, Code and locals to null, and the mask to
all ones. Latent initialization is not established by that body. DLL hash and
audit addresses are in [original dispatch evidence](ORIGINAL-STATE-DISPATCH.md);
ignored disassembly is under `artifacts/landing-state-20261010/`.

`StateFrame.control` explicitly identifies the modern representation:

- `control.selectedPath`: selected State/Class, independent of running Node.
- `codePath`: retained running Node, not cleared when Code stops.
- `control.codePresent`: nullable execution. `statementIndex` is a normalized
  statement ordinal, not an original serialized byte offset.
- `control.liveProbeMask`: literal positive enabled bits, not disabled names.
- `localsCodePath` and typed locals: independent retained storage identity.

Modern frames require stack ownership and a frame override, reject legacy
state-keyed disabled sets and reject synthetic `StateLatent::Stop`. Portable
latent ordinals are an abstraction, not native LatentAction IDs. A read-only
audit of all 38 original packages independently walks 261 States and their 100
explicit parent links: no persistent State-owned properties exist. Original
transitions retain null Locals, without fabricating an empty owner. Nonempty
schemas require retained typed storage; their original allocation lifecycle is
not established by this audit.

Detached context exposes separate selected/running identities and frame/Code
presence for original serialized stacks and modern overrides. It can inspect
raw runnable identities without executing them or converting byte offsets.
Raw runnable continuation execution is still refused. Legacy portable overrides
remain visibly inexact; selected identity is not inferred from running code.

## Runtime behavior

Exact raw/modern contexts resolve virtual functions and events from selected
StateNode ancestry. GetStateName reports its leaf, including Class-backed frames;
IsInState walks explicit SuperField ancestry. Global dispatch bypasses selected
state. Modern Enable/Disable mutate only the live mask; Enable uses original
selected-node authored eligibility. Nonprobe names remain no-ops.

Modern slices honor null Code before polling or decoding stale positions.
Original EX_Stop (0x08) clears Code presence only, retaining identities, mask,
locals, latent action and actor timer. Unproven opcode0x15 is refused. The
initial ProcessState role gate uses selected StateFlags, not running flags;
deletion is rechecked between statements. Selected identity is captured before
polling and compared after each completed top-level statement. The fifth
observed change yields; running-node changes and A-to-B-to-A changes entirely
inside one statement do not increment the counter.

Production GotoState now authors modern frames. The VM captures the wrapper's
class-as-None name before evaluating optional arguments, snapshots each Name
input before the next expression, and retains independent nested captures.
The shared original coordinator cancels latent execution, runs old EndState
under the original recursion guard, publishes selection/nullable Code/authored
mask, runs BeginState, and reports the original 0/1/2 result. Journaled flags,
not a generation counter, determine callback preemption. Inner None does not
set the successful-change flag. Name equality, not path equality, controls the
callback comparison. The wrapper's same-state shortcut preserves the live mask
and positions a label on the selection left by argument callbacks.

Label positioning happens after BeginState unless a callback preempts entry.
Search uses selected-node explicit SuperField ancestry. A miss clears Code
without throwing or erasing Node, selected identity, stale PC, mask, Locals or
actor timer. Successful relabel changes only running Node/PC/Code and cancels
latent execution. Neither selection nor relabel replaces local storage.

Successful Spawn now initializes its owned class-backed frame after SetOwner
and Instigator, before startup callbacks or the before-begun-play return. The
original base SpawnActor and InitExecution DLL bodies establish this order.
The frame starts with class identities, null Code/Locals and all-one live mask;
HasStack publication shares the birth transaction. Portable Continue while
Code is null is an explicit policy, not an inferred native constructor value.
GotoState/GotoLabel on a frameless actor remain no-ops, not frame constructors.
Other original Spawn side effects and world startup are still incomplete.

## Persistence and compatibility

Codec9/runtime envelope12 are selected only when a modern control is present.
All prior optional sections compose, including timers, RNG, AI graphs, births,
lifecycle and class defaults. Each present frame has a control discriminator
after locals and before disabled sets; modern controls then carry selected path,
Code-presence byte and little-endian U64 mask. Codec9 without a modern control
is noncanonical.

Codecs1–8 retain old bytes and explicit legacy provenance, including mixed frames
inside codec9. Legacy runtime compatibility is the old model, not a faithful
original-frame migration. No global mask complement, selected=running inference
or silent locals-owner promotion occurs. Ambiguous transitioned legacy overrides
refuse new state/probe control and require an explicit fresh-world reset.
Exact raw stopped identities can be promoted by audited clearing operations,
preserving the effective literal positive mask and retaining null Locals. Raw
dormant Enable/Disable still use their existing probe overlay without converting
unknown native latent IDs: Training Doctor's stopped raw frame contains 101,
not a proven portable Continue. Loaded dormant overlays retain their explicit
compatibility meaning; unrelated-state/nonprobe records refuse promotion.

Typed preflight supports State/Class running and selected nodes in the receiver's
actual class hierarchy. Runnable Class code and other UStruct running kinds are
refused; detached raw inspection can report broader identities. Runnable State
must be in selected-node ancestry with a valid ordinal and complete typed local
schema. A runnable zero-variable State can retain null Locals; declared or
orphaned values cannot. Null Code permits an unconstrained retained ordinal and
empty local owner; nonempty locals still require an independent State schema.
Envelope/codec pairing, identities, kinds, wait timers and budgets are checked
before publishing restored state.

## Verification scope

Asset-free codec controls cover literal masks, nullable Code, independent
identities, mixed legacy provenance, optional sections, malformed/truncated
data and retained-byte bounds. VM controls cover Stop retention, null-Code
Switch paths, post-statement throttling, pre-poll capture, deletion and rollback.
Original-data integration exercises production modern transition authoring
against original metadata as well as isolated generated checkpoints. Neither
kind of control constitutes automatic campaign startup or a playthrough.

Earlier representation-only baseline (before production authoring, 2026-10-11):
all 50 ordinary host tests pass, with three optional
original-data tests invoked separately. Frame-codec controls pass 1,632 checks
with 833 expected rejections; state execution passes 499/63; the legacy/composed
save-codec suite passes 6,477/5,731. Generated original Doctor controls pass 11
typed/envelope refusals plus mask/Stop/rollback/warm/cold/RNG checks. Original
class defaults (998 checks) and runtime rollback/save/restore also pass.
The full original actor integration passes Training, UNATCOIsland and Intro,
including the new generated modern controls and existing inventory/state/AI/
animation/RNG/native rollback checks. It remains isolated execution, not a
campaign playthrough or automatically started world.

The Android ARM64 build and APK v2 signature pass. The archived APK is
`artifacts/state-control-20261011/DeusExQuestVR-state-control-20261011.apk`
(18,919,145 bytes; SHA256
`6489354E5E59E374E33544FD19CF6C9A5ACEB96C93C77D19D7AE826E1FFED5BA`).
All 16 ZIP entries were inspected; no original game packages are included.
USB reports no headset connected at this verification, so it was not installed
or device-tested. No app data or manual game saves were changed.

Campaign bootstrap, player initialization, world tick/physics, NPC behavior,
cross-map archives and final device performance/visual acceptance remain
required. See [world startup gaps](WORLD-STARTUP-GAPS.md).

## Production-authoring verification (2026-10-11)

The current host build passes all 50 ordinary CTest tests. Three original-data
tests are optional and run separately with the user's original GOTY installation.
The shared coordinator/VM controls pass 551 checks with 71 expected rejections;
generated lifecycle tests pass 1,356/14, class-default tests 1,307/37, and Spawn
tests 3,652/78. Spawn checks include before-begun InitExecution, canonical restore
and an owner callback that changes the spawner's Instigator: the birth still
receives the entry-captured pawn, after the owner callback has observed its
pre-initialization frame/flags/Instigator. Original default integration passes
1,002 checks; original runtime rollback/save/restore also passes. The full original
actor suite passes Training, UNATCOIsland and Intro, including actual inherited
Mover labels, empty-Name/None shortcuts, Sleep/landing, inventory births, warm/cold
state restore, RNG/animation and transactional rollback. These are isolated
execution controls, not an automatic campaign world or playthrough.

The ARM64 APK builds and its v2 signature verifies. The current archived APK is
`artifacts/original-state-control-20261011/DeusExQuestVR-original-state-control-20261011.apk`
(18,919,145 bytes; SHA256
`A17B145B1D4C29D8E6D0F27FB1A46D721904738F09ED2F14B980456A91943112`).
All 16 ZIP entries were inspected; no original game packages are included.
The automatic build step initially reported the known Quest absent. It appeared
on a later USB check, and this exact APK was replace-installed successfully with
app data preserved. Its installed APK SHA256 exactly matches the archive above.
It was not automatically launched or visually verified.
No user/device saves were replaced.
