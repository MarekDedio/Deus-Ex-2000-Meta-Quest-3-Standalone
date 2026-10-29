# Original System services and nested Training SET

This batch implements the native System constructor/root prefix and command
discriminator, not successful viewport startup or live VR crate gameplay.
Original commercial packages and DLLs are read-only data; no commercial native
code is loaded or executed.

## Actual identity and lifetime

`Core.System` has native size `0x6c`, Super `Core.Subsystem`, Within `Core.Object`,
Class flags `0x16`, and registration Object flags `0x04084004`. Its actual
indexed Class owns a distinct unnamed CDO and six linked Config properties.
The two dynamic-array inner properties are separate indexed objects with their
real ArrayProperty Outer, not direct Class children.

The linked order is Suppress, Paths, CacheExt, CachePath, SavePath,
PurgeCacheDays. Property/Config lists include all six; ConstructorLink omits
the integer and preserves inherited tails. The array inners are excluded from
these Class lists. Shared Str/Array/NameProperty defaults are reused;
IntProperty's formerly absent CDO is prepared from the actual Property CDO.

Registration imports Core.System config into its CDO once, then the instance
deep-copies those typed values. The native NoInit constructor preserves them.
The Transient-Outer instance has flags zero and a genuine GC root, independent
of the current map/client. Shutdown releases it. This is not an account/login
feature: original GameInfo.Login is the engine's player-spawn operation.

## Configuration and atomic publication

Scalar import follows original GetString1024/ImportText behavior. Dynamic
Paths preserve full source strings, duplicates and empty entries. Suppress
imports actual Name identities in source order, retaining zero/None cells for
ordinary token failures. A missing section preserves prior arrays; a present
section with no matching key clears the corresponding array.

All System allocations, Class links/defaults, namespaces and weak lookup
records are detached until validation succeeds. A real name-arena stage owns
prospective compare/spelling indices, a pre-reserved Names copy, delta map
nodes and empty replacement buckets. No global name is published on a late
configuration or budget refusal. The same nonthrowing suffix commits names,
Class/default links, namespaces, lookup and the owning GC domain. Process-wide
committed arena growth remains charged even after runtime shutdown.

The separate additive dependency friend patch is registered after the existing
FindCompareIndex patch. Isolated fixtures cover fresh setup, older-checkout
upgrade, idempotence, unrelated edits and explicit conflict refusal.

Name capacity is reserved from matching Suppress entries plus the bounded own
object/category allowance, not every unrelated INI entry. Embedded typed-value
headers and nonexistent unnamed-CDO namespace aliases are not double-charged.

## Native UI storage and scoped temporary memory

All original UI WriteReference calls, write counts, type/ownership checks and
escape validation remain. For ordinary native Window/Console scalar or None
slots, a complete bit-exact comparison can avoid allocating a redundant
overlay when the current value is already identical. Existing overrides are
not erased. A copied nonzero or nonnull default still needs an explicit clear;
signed zero, NaN payloads, Name spelling and inactive payloads are compared.
Header, nonempty Object and composite stores stay on their original path.

SET command buffers and each Input config import have separate data-sized
scratch reservations. Initial loading, constructor reload and transactional
reload each supply an explicit scratch owner; persistent configuration never
retains borrowed callbacks. The file backend admits actual held-descriptor
payload sizes before allocation, under the same lock and identity/stamp/
growth/truncation checks. This is not permission to assume a cached target
length or to increase any existing resource cap.

Repeated Input stores in one root reuse a physical cell reference keyed by
actual owner/field/member/index. COW storage remains stable for that root.
Every VM WriteReference and distinct before-value still occurs; this does not
coalesce logical writes or discard intermediate undo state. The finite cache,
unique reference allocations and lookup work share existing budgets and are
released with the host journal after reference undo.

Toggle diagnostic observations distinguish forward native work from total
reported work, which also includes rollback. Exhaustion tests cut the measured
forward prefix and retain their failure/rollback assertions.

## Command route

The actual UEngine offer immediately before Core.StaticExec is GSys/USystem,
not GExec. System recognizes MEMSTAT, CONFIGHASH, EXIT, QUIT, RELAUNCH and DEBUG.
Their unimplemented bodies explicitly refuse; unknown commands genuinely
decline only with the real constructed/rooted provider present.

Core.StaticExec offers MEM, DUMPNATIVES and GET before SET. Those matched
unimplemented bodies refuse. A SET hit enters the existing typed Input/CDO/
config service within the same host and Execution. A host-owned result avoids
a dangling stack pointer at eventual config-file publication. Diagnostic
output is returned through PlayerPawn.ConsoleCommand, and per-call result
fields are reset. No direct unknown-Input-to-SET shortcut is used.

Unknown commands after SET still explicitly refuse the missing remainder of
Core.StaticExec. OBJ, GTIME, FMemCache, later GExec and optional engine-provider
bodies are not fabricated. This also does not implement appInit's later
Suppress name-entry flag writes, full Input.Init/Reset/ReadInput, post-Possess
travel callbacks, OpenWindow/render setup or live world publication.

## Verification scope

Pure tests cover config grammar/order, typed imports, real name staging,
resource refusal and atomic commit. The owned-data System integration checks
actual Class/Outer/indices/headers/layout/defaults, all linked heads/tails,
independent original config oracles, a late overflow after name import without
published-name leaks, +13 GC objects/+10 lookup entries, cached source binding,
map/save replacement retention, unknown/recognized commands and teardown.

These tests do not establish live mutation independence where no typed System
mutator exists, successful full engine startup, on-device performance, or
playable crates/loot/gravity. See STATUS.md for final batch verification and
the next actual startup boundary.
