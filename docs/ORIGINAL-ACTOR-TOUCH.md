# Original Actor touch-slot protocol

Static read-only retail audit, detached protocol and typed runtime adapter,
2026-10-19, limited to the movement prerequisites of first `00_Training`.
No commercial DLL/EXE was loaded or executed. Owned Engine.dll
SHA256, unchanged during inspection:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

This supplies `native/quest_actor_touch.h` and the actual qualified-field/event
adapter in `native/portable_retail_actor_touch.inl`. Its entry points are still
rollback-only diagnostics, not publishing Level.CheckEncroachment,
Move266/SetRotation299, startup, crate gravity, destruction or usable surviving
loot. Independent tests/build acceptance are separate from the source contract
described here.

## Identities, complete bodies and original state

The entire normal bodies, not just the empty-slot path, were read in
`artifacts/training-level-encroachment-20261019/touching-body.asm`:

| Original identity | Thunk / full normal body |
| --- | --- |
| Export808, `AActor::BeginTouch(AActor*)` | `1030177B` / `10369570..103695C5` |
| Internal one-sided helper `(AActor*,AActor*)` | `10369630..103698B4` |
| Export1006, `AActor::EndTouch(AActor*,int)` | `10302365` / `10369970..10369A48` |

EndTouch's backedge at `10369A7B..7D` belongs to its second slot scan; the
normal return is still `10369A48`. The intervening catch/log/rethrow fragments
are not a native slot-undo mechanism. Static reproduction can use:

```
llvm-objdump --disassemble --x86-asm-syntax=intel --no-show-raw-insn \
  --start-address=0x10369570 --stop-address=0x10369a7e Engine.dll
```

The genuine declared storage is `Engine.Actor.Touching`, ObjectProperty[4],
flags2, constrained to Engine.Actor, at native offsets `+E8..F4`. `Role` is the
raw Byte at `+30`; exact zero, not sign/ordering, selects the first eviction
pass. Pawn ancestry uses exact `APawn::PrivateStaticClass` at `1058C660`.
Pawn `bIsPlayer` is `+318` mask02, NOT the receiver-local GetPlayerPawn helper
and NOT script native720's Level scan. The existing `ORIGINAL-MAKE-NOISE.md`
pins that authored Pawn bit independently. UObject.IsValid import is IAT
`10599C9C`; event FindFunctionChecked is `10599A88`, followed by actual receiver
ProcessEvent virtual10h. Touch/UnTouch use original name globals `10583704` /
`10583700`. There is no replacement render/overlap registry in these helpers.

## Complete one-sided insertion and replacement

Let S be the first helper argument and C the second. Original `1036964E..6B4`
asserts S and C nonnull and different before slot work. The detached adapter
refuses invalid inputs before invoking any provider.

1. At `103696B4..6DD`, set selected=-1 and read S slots0,1,2,3 in ascending
   order. Each None overwrites selected with its index, so the LAST empty
   slot wins. Any nonnull pointer equal to C returns true immediately, without
   inspecting later slots, a Role, a class, or any event. If an empty index was
   found, proceed straight to insertion below.
2. Only if initially full, `103696E6..9734` scans ALL FOUR indices. For each:
   read the current slot pointer, require its actual UObject validity, then
   REREAD the SAME slot before reading Role. If that second pointer's Role
   byte is zero, call `S.EndTouch(thatPointer,0)` and set selected to this
   index AFTER the call. Continue even after successful eviction. The LAST
   qualifying Role0 index therefore wins. There is no new empty-slot scan.
   A callback may change any later entry; the next index reads that live state.
   A now-null/invalid later pointer is not permission to skip it or repair the
   array: portable unsafe-provider refusal must unwind the whole caller root.
3. If selected is still -1, `10369740..69` tests incoming C's actual Pawn
   ancestry. If not Pawn, return true with no insertion. For a Pawn,
   `1036976B..7AD` scans slots0..3, reading each pointer for its Pawn ancestry.
   On the FIRST non-Pawn, REREAD the slot (`10369799`) and call EndTouch with
   that current pointer, set selected, then stop this scan. No later slots are
   inspected after this first candidate. A duplicate or an empty slot created
   by any callback is not rescanned into a different insertion decision.
4. Only when all four entries were Pawn and no earlier selection exists,
   `103697B3..BE` tests incoming C.bIsPlayer. False returns true with no insert.
   True enables the final `103697C4..981D` priority scan. Read each slot for
   Pawn ancestry. If Pawn, REREAD that slot (`103697F7`) before its bIsPlayer
   bit test; a player Pawn is skipped. A non-Pawn OR nonplayer Pawn is eligible.
   On the FIRST eligible index, REREAD again (`10369809`) before EndTouch,
   set selected and stop. If every peer is player Pawn, return true unchanged.
5. At `10369820..985A`, when selected>=0, write C to S.Touching[selected]
   BEFORE dispatching S.Touch(C). Do not revalidate selected as empty, rescan
   for C, compact slots, or choose another index after EndTouch callbacks.
   After Touch returns, REREAD only that exact selected slot. Return false
   if its current value is not C, otherwise true. C surviving elsewhere in the
   array does not change that exact-slot result. Only this postinsert mismatch
   is a normal false return; duplicate and full/no-victim paths return true.

The repeated raw reads above are intentional protocol, not optional
optimization. Role0 uses the pointer from its second read for both Role and
EndTouch, with NO third slot read before EndTouch. Pawn priority and final
player priority have their distinct rereads described above. Runtime providers
normally execute sealed CPU-owned reads without game callbacks between these
loads, but detached protocol tests can expose an accidental cached snapshot.

## BeginTouch and EndTouch callback ordering

BeginTouch (`10369596..95B2`) calls OneSide(S,C), and only if its returned int
is nonzero calls OneSide(C,S). The second result is ignored. Full/no-replacement
on S still returns true, so the reciprocal helper may insert when S cannot;
do not invent guaranteed symmetry, a collision/deletion filter, or a rollback
on an ordinary false helper result.

EndTouch starts with a distinctness assertion. The original body has no null
input safety check; the portable adapter additionally refuses null pointers
instead of emulating unsafe raw dereferences. At `103699B7..99F7`, scan S slots
0..3 for the FIRST C, clear that slot to None, and immediately dispatch
S.UnTouch(C) unless the supplied signed integer suppressSelf is ANY nonzero
value. Stop that first scan even if duplicate C slots remain. If no S match
exists, no S event is sent, but the reciprocal phase still executes.

At `10369A02..9A35`, AFTER that possible synchronous callback, scan C's CURRENT
slots0..3 for the FIRST S, clear it, dispatch C.UnTouch(S), then stop. This peer
event is unconditional on suppressSelf and on whether S had a match. Reentrant
callbacks can repopulate/change slots before this phase. There is no bDeleteMe
gate, touch-event history bit, all-duplicates loop, sorting, compaction, or
postevent clearing of additional slots. Actual ProcessEvent owns original
event suppression/probe/begun/deletion behavior; the native relationship writes
must not be skipped merely because an event is presently suppressed.

## Detached provider and ownership boundary

`OriginalActorTouchQueries` exposes live readSlot/writeSlot, requireValid,
Role Byte, actual Pawn membership, Pawn.bIsPlayer, Touch/UnTouch event dispatch,
and an optional enclosing work hook. Every ACTIVE provider invocation charges
that hook once; missing active providers refuse without consulting inactive
functions. Slot loops are bounded at four, but reentrant events still need the
actual caller's shared root work/depth/memory/generation admission. There is
no private per-touch transaction, local visited list or reset work allowance.

The three public functions are EndOriginalActorTouch, AddOriginalActorTouchSide
and BeginOriginalActorTouch. They propagate provider/work exceptions and cannot
undo external provider state themselves. Their runtime adapter must authenticate
the genuine actors, exact declared Touching[4]/Role/Pawn flag schemas and native
event declarations, journal all writes before callbacks, and keep all nested
events/movement/hash/tag/RNG/publication effects in the SAME outer VM root.
Refusal atomicity is a portable safety guarantee, not inferred original fault
cleanup. A normal boolean/int result does not undo prior EndTouch/Touch events.

Current `PortableActorVmHost::UnTouch` is an older, different helper: it skips
bDeleteMe sides, clears all duplicates and gates notifications with
touchEventSent. It cannot implement this protocol unchanged. Touching values
already live in journaled scriptValues; host Touch()/Before swaps and saved
qualified property slots can preserve them without inventing a second slot
registry. Exact native touching must NOT consume or set touchEventSent as its
relationship authority. Existing legacy lifecycle history/save support is a
separate compatibility concern, not permission to normalize native slot state.

## Actual first-map adapter boundary

The adapter authenticates current-Level actor identities, the exact declared
Touching Object[4], Role ENetRole Byte and Pawn.bIsPlayer Bool schemas, and real
Actor ancestry. Current-Level ownership/validity admission is deliberately
stronger portable policy, not a complete emulation of retail UObject.IsValid.
It writes through the existing Execution.WriteReference journal and shared
write/work/memory allowances. There is no replacement relationship registry,
touchEventSent authority, private transaction or forced begun-play/possession.

Real state/class callback selection occurs before ProcessEvent suppression,
as with original FindFunctionChecked. Selected overrides may have locals and
different parameter names; their single Actor parameter packet is checked.
The actual host CallEvent still owns probe/begun/deletion suppression. Thus a
cold map can update native relationship slots and attempt the real callback
entry without executing dormant gameplay bytecode. This does not demonstrate
an executed callback body in a begun world.

Qualified Engine.Actor.Touching writes bypass the generic animation-clock
reconstruction in ReferenceFor: a relationship-only write must not replace an
existing animation clock. The skip is exact-qualified, not a leaf-name skip;
ordinary animation fields and unrelated shadow properties keep their behavior.
The cold-map rollback tests alone do not establish preservation of a nontrivial
live animation clock during a successful write.

Diagnostic BeginTouch, EndTouch and TouchSide return bounded detached
postcommand slot snapshots. IsOverlapping returns a predicate with its original
command index. Output storage is admitted before copying actor paths and no
native pointers escape. Every command shares the existing outer VM root and
the entire preview is rolled back on success or refusal. This does not yet
enable first-Training movement or live crate physics.

## Executed protocol and actual-map checks

The standalone optimized host suite passes127578 checks, including24609
independent interpreter cases,20308 exhaustive slot layouts,3888 role/class
policies,18 callback mutation controls and360 provider/work failure prefixes.
It passes380 explicit refusals and three caller-owned MOCK rollback roots.
The interpreter does not call the production touch kernel. Its executed
callback mutations are detached-provider fixtures, not real begun-world
UnrealScript callback bodies.

The actual38-package first-map regression passes seven rollback-only touch
batches:34 live touch commands and14 overlaps (five true/nine false). Its real
census contains1308 actors,22 Pawns, four authored bIsPlayer actors and zero
Role0 actors. Native Pawn and bIsPlayer replacement controls execute once
each, and full/no-insertion returns execute twice. Role0 replacement is
therefore covered by the independent protocol suite, not invented actual-map
data. The model expects27 Touch/12 UnTouch dispatch attempts; real cold-world
bytecode remains suppressed. A separate operations=1 refusal after a first
slot write confirms that even dormant Touch consumes actual callback entry.
There are no forced role values, possession, begun flags or class substitutes.

Success and late/partial-write/output-budget failures restore the full actual
slot/pose/hash/tag/save/revision/lookup/client/GC/RNG root. An actual
Doctor1.PlayWaiting command, without clock injection, additionally supplies a
real saved animation clock. One successful cold touch/untouch preview and two
late/partial-write refusals restore its entire seeded clock/save/lookup/
revision/GC state. This is after-root rollback evidence, not a witness of
unchanged clock state during a successful in-root write. The original
preanimation checkpoint is restored afterwards.

The final extended owned collision suite passes951463 checks/68 explicit
refusals (initial run before the animated-rollback addition:951401/66). Its strict
startup remains at Engine.Mover.BeginPlay PC166, slot126,
00_Training.DeusExMover16: unsupported native266. No other campaign map is
loaded. Strict ARM64 runtime/tests, the host build, Android native link and
coherent CTest143 executed/167 registered pass, with24 explicit owned-data
skips and zero failures. Evidence: `artifacts/training-touch-owned-final-20261019.log`,
`artifacts/training-touch-ctest-final-20261019.log` and
`artifacts/training-touch-unit-20261019.log`. The earlier animated-test link
attempt encountered Windows' expected running-EXE lock, not a source error;
after the old run completed, relinking and final execution passed.
