# First Training: original quickbar toggle native

2026-10-23. Only first `Maps/00_Training.dx` is the actual-map integration
target. This native addresses the original startup refusal in
`HUDObjectSlot.SetItem` PC69/native1611; it does not establish playable
crates, collectible contents, gravity, complete startup or the campaign.

## Implemented path

`quest_native_window_toggle.h` follows the static original contract in
[ORIGINAL-TOGGLE-WINDOW.md](ORIGINAL-TOGGLE-WINDOW.md): compare the pressed bit
with the incoming raw INT before masking bit0; unchanged means no stores or
callbacks; otherwise update appearance, then notify Self and live parents.
Each notification rereads the source's pressed bit. RadioBox selection is
stored before resetting the former toggle, with original one-check and handled
branches. Native recursion is bounded at128 as an explicit port stack policy,
and every traversal/operation spends the caller's shared native-work budget.

`portable_window_toggle.inl` binds this coordinator to actual constructed
native Windows, typed original pressed/Radio fields and registered native lists.
It checks the original declaration and captures the original thunk's false
argument seed before evaluation. Script callbacks use state-aware resolution
and the same `Execution.CallFunction`, not the Actor-only event dispatcher or
a new VM transaction. Missing/no-result returns preserve initialized false;
other returns must be Bool. Typed script ingress is canonical Bool; the pure
coordinator additionally preserves the original noncanonical raw-INT behavior.

Button appearance now reads sensitivity/focus before pressed state, and stores
text, texture, then tile color in the audited original order. No toggle sound,
mouse flags, focus changes or reconfiguration is invented.

## Isolated native-window controls

The rollback-only probe constructs a concrete original base Root, Radio and
Toggle children, invoking their original scripts and native constructors. A
second case uses an actual HUDObjectSlot, whose original ToggleChanged rejects
an empty selected slot by reentrantly calling SetToggle(false). Explicit
generated typed Info colors make normal/pressed appearance observable; original
package files are never edited. All setter calls go through the original VM
declaration, including no-result and malformed-input controls.

The shared native root constructor is extracted from native1052 without
changing production's configured class, most-derived InitWindow or unfinished
visibility refusal. The isolated base-Root fixture does not replace Training's
HUD and never runs extra operations after an already-poisoned HUD callback.
Its output has an admitted128KiB bound for the entire transaction. The new
read-only `Execution.WriteCount()` observes the real root write ledger, rather
than maintaining a diagnostic approximation. Every scenario deliberately
rolls back, including partial-budget failures.

## Verification

Standalone strict-g++ coordinator tests pass70 checks/eight deliberate
refusals; the freshly built CMake target repeats that pass. Strict ARM64 syntax
checks for runtime, VM and owned player/HUD test pass. Android `deusex_quest`
native compilation/link passes. Focused coordinator and complete VM CTests
pass. The expanded owned original-player/HUD test passes6,758 controls,
including cold/warm name reuse, actual Radio replacement/one-check, generated
typed appearance, original empty-slot reentrancy, false no-result seed,
malformed ingress, partial-write/shared-work refusals, checkpoint and GC undo.

Both actual first Training startup attempts now pass original quickbar
population and reach `DeusEx.DamageHUDDisplay.InitWindow` PC39/opcode1:
`Unsupported authored struct array/string member DeusEx.DamageHUDDisplay.sIconInfo.DamageType`.
Each executes217,260 instructions/24,101 writes, with native work489,947 cold /
489,405 warm of1,048,576. This is a fixed `Name DamageType[3]` member inside
the authored iconInfo struct array, not an established String-property failure.
The original `GameInfo.Login` returns genuine JCDentonMale0, but possession
remains unpublished. Full save/state/revision/hash/RNG/client/lookup/Actor/GC
rollback and all41 original source fingerprints are verified. Diagnostic
exit2 is the observed unsupported boundary, not passing startup acceptance.

The initial full desktop build and crate regression were interrupted; after
confirming their process handles were missing and no compiler/test process
remained, the incremental full build passed. The separate owned crate test
passed347 controls/two genuine contents refusals. Coherent shared CTest passes
173 executed/205 registered, with32 explicit owned-data skips, zero failures
in76.64s. No new APK or on-device acceptance is claimed.

Evidence: `artifacts/training-toggle-focused-build-20261023.log`,
`training-toggle-focused-tests-20261023.log`, `training-toggle-android-20261023.log`,
`training-toggle-owned-20261023.log`, `training-toggle-startup-20261023.log`,
`training-toggle-full-build-resumed-20261023.log`,
`training-toggle-full-tests-20261023.log`, and
`training-toggle-crate-resumed-20261023.log`.

## Next direct crate checkpoint

Before another unrelated UI increment, add typed original TakeDamage coverage
in the genuine initialized Training context, using real Login's returned JC
and GameInfo/collision state in the existing rollback root before SetPlayer.
Continue normal possession afterward and retain its real failure/rollback;
do not publish this prefix, invent an instigator or force relevance/startup
flags. That can expose actual crate/hash/contents gaps before full HUD
completion, but cannot certify possessed-player effects, live input or falling.
The eventual headset damage bridge must use actual paired/equipped ownership.

Simply removing viewports/clients is not an accepted startup shortcut: the
original no-client branch changes default server-game selection, and native
detail/skybox/remaining LoadMap providers and publication guards are still
unfinished. Live crate destruction, surviving usable loot, world physics and
the entire first Training map remain unfinished.
