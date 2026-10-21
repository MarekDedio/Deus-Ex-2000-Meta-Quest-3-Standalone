# First Training startup storage and final Level alias

Work in progress,2026-10-21. ONLY the entire first `Maps/00_Training.dx` is the
current actual-map integration/test target. This is a prerequisite for original
crate destruction, surviving usable loot and support/gravity, not those playable
features or a new APK.

## Lossless caller-root storage

The previous real startup refused a new native conversation-list node at
UNATCOTroop0's PostPostBeginPlay, slot1254/PC6, with a3192-byte request. Neither
the shared16MiB state limit nor any operation/object ceiling is increased.

Native linker-header before-images now exist only for objects whose header
actually mutates, rather than owning another shared loader pointer/index/marker
in every ordinary script/clock before-image. All three mutation paths (export
clear marker, linker reset, index/cache store) record the first old header under
the SAME VM root. Export-slot undo remains separate. Actual vector capacity and
old-plus-new reserve peaks are admitted before allocation. Rollback restores
owning headers before provisional identities retire; timer/clock publication
classification compares those actual sparse before-images.

Conversation binding now uses an optional paged UObject-index journal: the
same4096 operation ceiling, a fixed pointer table,64-row pages on demand, and a
partial final page. Fixed-mode BeginTransaction keeps its eager contract.
Borrowed allocation admission charges actual state/table/pages to the existing
root ledger. Preparing the next page BEFORE a new UObject's admission exposes
the combined page/object/default-copy peak. Conversation, Window and Actor birth
paths prepare capacity without consuming an ordinal; Acquire/old Retire still
self-prepare. Lease moves, releases, commit and rollback allocate nothing and
do not invoke the admission callback. No identity, source observation, script
callback, conversation node or rollback right is dropped.

## Correct final serialized FirstDeleted authority

Independent original static disassembly identified that ULevel.Serialize first
archives native `+100`, then its sixteen-reference loop at `+9C+i*4` archives
that SAME address again at index1. The loader must use the final store, not
assert that the two encoded references are equal. Both and all remaining
references are validated; truncated native tails refuse. See
ORIGINAL-DEFERRED-ACTOR-OWNERSHIP.md for addresses and source identities.

The production decoder now returns the final alias and its actual source
offset. First Training's final None is at4370511; its preliminary None remains
at4370509. No map name/offset is hardcoded in the decoder. Generated tests cover
both conflicting-alias directions, variable compact lengths, all sixteen
reference bounds and every byte-prefix truncation. Existing Model-only prefix
consumers remain unchanged. Eight synthetic runtime fixture builders now supply
the authentic Float/preliminary-head/sixteen-pointer tail instead of stopping
after ReachSpecs count0; runtime validation is not weakened to accept them.

## Verification and current frontier

Focused host build and strict ARM64 syntax checks pass. The focused CTest set
passes9/9, including the five initially failing incomplete fixtures. The index
lease suite passes338395 checks/40 refusal controls. The decoder passes437/141
generated and986/141 with the independently fingerprinted owned first map.

Both repeated-startup attempts now complete1335 PostPostBeginPlay visits and
reach the same explicit CompactActorSlots/save-ordering refusal after533643
instructions/39784 writes; native work489697 cold/489325 warm of1048576 is not
exhausted. All26 original Spawn observations, including NanoKey, agree. Full
save/state/revision/hash/RNG/client/lookup/Actor/GC rollback and all40 unchanged
original source files pass. Exit2 is an observed unfinished startup, not a
passing startup test or publication.

Actual first-map linker teardown passes38456/160, deferred ownership429/21
(18 VM writes/20563 work) and conversation binding156871/17. Their exact native
ordering, lookup/index lifetime, late failure/rollback/GC and archive-refusal
controls pass; original sources remain unchanged. Android native compilation
and link pass. The coherent full desktop rebuild passes. Shared CTest executes
163 of192 registered tests in65.79s with zero failures and29 explicit
owned-data skips (not actual-map/headset evidence). The actual first-map
movement regression passes122706/21, and isolated crate dispatch347/two
contents refusals still passes: real Game=None relevance removes those born
Lockpicks, so this test is NOT surviving loot. The original property teardown
regression passes1164/33, including every original payload-write prefix and
full native state/source immutability checks.

The next original dependency is LoadMap's stable static/dynamic partition,
preserving slots0/1 and recording native first-dynamic index. It is NOT the
separate exported CompactActors operation. Existing authored-prefix/appended-
birth saves cannot represent that order; see ORIGINAL-WORLD-ACTOR-ORDER.md for
the distinct exact original operations and proposed owning requirements.
Later client/player/detail setup,
native checkpoint/cleanup and actual held-weapon loot/gravity acceptance remain
unfinished. No other map is integrated/tested and no backend-only APK is made.

Evidence logs: `artifacts/training-startup-storage-*-20261021.log`. The earlier
`training-deferred-host-all-20261021.log` build was explicitly superseded after
confirming incomplete-fixture failures, not counted as a successful full build.
