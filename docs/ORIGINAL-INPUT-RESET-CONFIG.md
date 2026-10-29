# Original native544 and Input ResetConfig (2026-10-29)

This implements the reached keyboard-configuration reset, not Input.Init,
ResetInput, ReadInput, completed player startup or live VR crate/loot/gravity.
Original DLLs remain data-only static evidence; none is loaded or executed.
See ORIGINAL-INPUT-CONFIG-INSTANCE.md for the underlying source contract.

## Actual provider and storage order

`PlayerPawnResetKeyboardNative` authenticates the original Engine.PlayerPawn
native544 declaration (flags401, no input/result fields). Indexed native bytecode
does not supply a Function pointer: authenticate the original metadata in all
cases, and additionally authenticate a supplied declaration when present. The
first discovery run exposed an incorrect nonnull-declaration requirement; that
failure is retained as discovery evidence, not a passed startup.

`RunOriginalResetKeyboard` preserves the actual Player None, non-Viewport and
Input None early returns. Only a real nonnull Input selects its actual Class
for ResetConfig. It never initializes Input or requires its native Viewport
back-pointer. Session/input leases are authenticated rather than converting
revoked native identities into None.

The schema domain owns a separately pinned read-only DefUser source and its
immutable parsed cache. The wrapper checks regular/single-link/no-symlink file
and parent identity, immutable stamps and bytes, with admission before reads.
It has no directory-creation, stage, lock or publication API; it is not a
writable User backend alias. Preparation is atomic and same-source idempotent;
use/publication revalidate source identity. Root-owned callbacks are borrowed,
not retained in the source or persistent import profile.

ResetConfig obtains the entire selected-class section packet before its first
SetString. The packet preserves source-order duplicate and empty entries, UTF16
CString behavior, the first-equals split and original key-prefix stop rule.
The unsafe original value-length overflow explicitly refuses before mutation;
it is not emulated or silently truncated. Packet/conversion/query workspace is
retained through nested cache writes under the unchanged resource limits.

Only the selected actual Input class and descendants are reloaded: indexed CDO
pass first, then indexed matching live instances, refreshing the upper bound.
InputExt reset therefore reloads InputExt's CDO, not base Input's CDO. Both
parent section passes write to each same receiver's EXISTING values; missing
keys retain storage. Default blocks are excluded from the instance pass.
Original Input PostEditChange inherits Modify, whose transactional gate is
false for authenticated native Input flags4000. Constructor reload counters
are not repurposed for reset observations.

## Cache changes are not Flush requests

`RuntimeNativeInputConfig::cacheDirty` belongs to the journaled config cache.
Reset section overlays change that bit but never request file publication.
An explicit SaveConfig/Flush snapshots the then-current cache into separate
pending file bytes and clears logical cache dirtiness. A later ResetConfig in
the SAME root can dirty the cache without replacing that earlier Flush snapshot.
A later explicit SET/Flush snapshots the updated cache. Commit publishes only
the pending explicit snapshot; rollback restores both config and actual native
CDO/instance payload ownership after VM reference undo.

The original DefUser has Engine.Input but no Extension.InputExt section. The
first Training path therefore performs zero SetString calls yet genuinely
reloads InputExt CDO/live storage from the current User cache, retaining the five
SET bindings established earlier in Possess. No source config file is written.

## Reached evidence

The corrected handler passes the original-owned two-viewport integration:
9632 checks/15 refusal controls, actual CDO-first/two-instance reload, the five
prior bindings, modified ISOLATED DefUser duplicate/case overlays, cache-only
commit, same-root SET-before/reset/SET-after Flush ordering, write/forward-work
cuts, and full checkpoint/lookup/revision/GC/file rollback. Pure native544
provider311/42, section packet146/12 and read-only source193/21 also pass.

The real first-Training discovery now advances past native544 and stops in
DeusExPlayer.RestoreSkillPoints at PC23/native1024, with265657 instructions and
37823 writes. Full save/state/revision/hash/RNG/client/lookup/Actor/GC rollback
passes and42 owned sources remain unchanged. The discovery exits2 to identify
an unsupported boundary; it does NOT certify successful startup.

The next actual operation is Extension.ExtensionObject.StringToName, native1024
flags401, a String parameter flags400880 and Name result flags580. RestoreSkillPoints
uses it for SKTemp_SkillPointsAvail/Total, then accesses actual FlagBase services
1120/1112/1121. Do not return a fabricated None name, skip skill restoration or
treat constructor/default flag absence as a prepared native flag-storage service.

Continuation: the subsequent bounded-name batch now implements native1024 and
both actual Training runs reach the following PlayerPawnExt.FlagBase read
at RestoreSkillPoints PC55/opcode1 (265660 instructions/37824 writes). Its
constructor-owned graph remains missing and explicitly refuses instead of
manufacturing None. See ORIGINAL-STRING-TO-NAME.md and ORIGINAL-PLAYER-FLAGBASE.md
for the current repeated-run verification and exact next constructor contract.

Full desktop, strict ARM64 and Android-native builds pass on the corrected handler.
Core SET regression passes9548 checks/6 refusals. CTest executes241 of290 registered
tests, with49 explicit owned-data skips and zero failures in199.98s. The final
expected-native1024 diagnostic repeats twice with identical counts/boundary and
complete rollback; no successful startup is claimed. The final audit confirms
1843 frozen compiler inputs, two deployment scripts and45 original sources
unchanged, with no live jobs. No new APK or live Quest crate acceptance is included.
