# Independent original actor-state control

This increment separates selected state, running code and nullable execution in
the shared runtime and its saves. It is backend work, not automatic world
startup, player possession, live NPC scheduling or campaign acceptance. The
user-confirmed hand-direction APK remains installed.

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
latent ordinals are an abstraction, not native LatentAction IDs. Original local
allocation/reset semantics remain unproven; typed-local safety is not claimed
to reproduce them.

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

**Modern GotoState and label authoring are not switched yet.** They fail
transactionally instead of applying the old conflated model to exact frames.
The next change must install original internal/wrapper ordering and flag-based
EndState/BeginState preemption together, then switch production frame authoring.
Current production GotoState still authors explicitly legacy frames, preserving
their documented compatibility behavior. This increment is not complete actor
control and must not be used to enable a partial NPC scheduler.

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
or silent locals-owner promotion occurs.

Typed preflight supports State/Class running and selected nodes in the receiver's
actual class hierarchy. Runnable Class code and other UStruct running kinds are
refused; detached raw inspection can report broader identities. Runnable State
must be in selected-node ancestry with a valid ordinal and complete typed local
schema. Null Code permits an unconstrained retained ordinal and empty local
owner; retained nonempty locals still require an independent State schema.
Envelope/codec pairing, identities, kinds, wait timers and budgets are checked
before publishing restored state.

## Verification scope

Asset-free codec controls cover literal masks, nullable Code, independent
identities, mixed legacy provenance, optional sections, malformed/truncated
data and retained-byte bounds. VM controls cover Stop retention, null-Code
Switch paths, post-statement throttling, pre-poll capture, deletion and rollback.
Original-data integration uses isolated generated checkpoints and original
metadata, not production modern transition authoring.

Fresh checks (2026-10-11): all 50 ordinary host tests pass, with three optional
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
