# First Training: live-state and save-file memory policies

2026-10-22. First `Maps/00_Training.dx` only; other real-map integration is
deferred. This correction is a startup prerequisite, not acceptance of live
crate damage, surviving usable contents, gravity, or the full campaign.

## Cause established from current code

`PortableActorVmHost::ValidateStateBudget` used `kMaximumSaveRuntimeBytes`
(16 MiB) for a different representation: live native/script retained state.
That measurement includes the weak UObject lookup index, native collision
state, overlapping query scratch, native package/class/linker records, window
objects, mutable class-default patches, instance patches, and script states.
It is not a measurement of encoded save bytes.

The preceding actual Training startup failed creating the first Ammo HUD child
at `DeusEx.DeusExHUD.InitWindow` PC68/native1410. Native/default accounting was
16,665,955 bytes, including an 11,405,424-byte weak lookup index, before the
separately measured script-state component was added. The remaining 111,261
bytes of the file cap were insufficient. Both cold/warm observations rolled
back completely; this was not missing player credentials or an account system.

Both the existing structural codec's default retained allowance and the VM's
independent execution retained allowance are already 32 MiB. Neither represents
the complete process heap. Mutable CDO/frozen-default maps hold only runtime
patch blocks; immutable authored defaults are already shared, so the evidence
does not justify a new wholesale default-sharing design.

## Scoped correction

`quest_runtime_state_budget.h` names a separate fixed 32 MiB live-native/script
allowance. The runtime uses it for the existing accounting walk and the same
composed native scratch admissions. No native service, script, HUD callback,
world Actor, startup phase, or original class is skipped to fit the allowance.
The combined state/native check uses overflow-safe subtraction, with exact-cap
success and independent byte/node refusal. Budget diagnostics now include the
measured state component, live cap, and native window count.

The 16 MiB runtime save-file/envelope policy is unchanged. Save snapshot
capture, prepare/restore validation, encoding, decoding and file readers retain
their independent bounds. No save format changes. All existing object,
property, node, depth, link, work and execution limits remain unchanged.

This does not promise 32 MiB total RAM or comprehensively audit every existing
undo allocation. Asset caches, execution allocations and other native owners
have separate policies. Full device memory/performance and live saves remain
unverified; in particular active retail collision and DumpLocation checkpoint
capture still have explicit unsupported-service guards.

## Verification scope

`quest_runtime_state_budget_test` exercises the production combined validator:
crossing the unrelated file limit, exact live cap, one byte over, oversized
component/size_t overflow, unchanged value-node bound, nested scratch admission
and unwind, and real save encoder/decoder behavior at/over the unchanged file
cap. The actual first-Training startup probe must separately demonstrate the
original Ammo allocation proceeds and preserve complete rollback at any later
unsupported boundary. Neither a helper test nor a safely rolled-back startup
diagnostic establishes playable crate/loot/gravity behavior.

The new focused test passes 22 checks / 9 refusal controls. The six focused
CTest cases (runtime budget, save bundle, load-context save policy, script-state
codec, generated class defaults and generated Spawn) pass. ARM64 strict syntax
checks for the production runtime and new test pass; Android `deusex_quest`
native compilation/link passes. The coherent full desktop build passes;
shared CTest passes 172 executed / 204 registered, with 32 explicit owned-data
skips and zero failures in 105.60 seconds. The separately invoked owned first
Training crate regression passes 347 checks / two genuine contents refusals;
its Game=None fixture still removes the Lockpick and is not surviving loot.
The separately invoked owned original-player/HUD regression passes 6,095
controls, including native-window prefix, pending132-object graph, pairing
failure rollback, cold checkpoints and GC. Its isolated fixture reaches the
known DamageHUD fixed-Name-array boundary, not the actual larger world's
earlier SetToggle1611 boundary. Neither fixture publishes possession.

Both actual first-Training startup attempts now pass the former Ammo allocation
and reach `DeusEx.HUDObjectSlot.SetItem` PC69/opcode102, missing native1611.
Each executes 209,469 instructions / 20,928 writes, with native work
480,589 cold / 480,047 warm of 1,048,576. The caller stack enters original
HUDObjectBelt.InitWindow, PopulateBelt, AddObjectToBelt and ClearPosition.
No UI callbacks are bypassed. Original Login returns genuine JCDentonMale0;
possession remains unpublished because this later original UI operation fails.

Both attempts verify full save/state/revision/hash/RNG/client/lookup/Actor/GC
rollback, with all 41 original source bytes/sizes/timestamps unchanged. The
probe's terminal exit2 is the expected **observed unsupported boundary**, not
a passing startup acceptance test. No APK is produced by this CPU/native build.

Read-only metadata identifies native1611 as `Extension.ToggleWindow.SetToggle`
with required Bool parameter `bNewToggle`. That is the next actual startup
boundary, before the previously known nested fixed-Name-array HUD gap.
Neither this memory correction nor that UI dependency completes the direct
headset damage path: `DamagePortableRuntimeActor` still rejects non-Pawn
targets, and crate interaction needs original TakeDamage with a genuine player
instigator, original contents relevance and real falling/support simulation.

Evidence under `artifacts/`: `training-runtime-budget-startup-20261022.log`,
`training-runtime-budget-focused-tests-20261022.log`,
`training-runtime-budget-unit-strict-20261022.log`,
`training-runtime-budget-runtime-strict-20261022.log`, and
`training-runtime-budget-android-20261022.log`.
Full verification logs: `training-runtime-budget-full-build-20261022.log`,
`training-runtime-budget-ctest-20261022.log`, and
`training-runtime-budget-crate-20261022.log`.
Owned player/HUD: `training-runtime-budget-player-hud-20261022.log`.
