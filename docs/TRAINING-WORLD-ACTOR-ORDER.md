# First Training native world Actor order

Only the entire first `Maps/00_Training.dx` is the current actual-map target.
Other-map integrations and map test sweeps are deferred. Shared engine checks
below support that Training path; they are not full tutorial acceptance.

## Original behavior and ownership

`ORIGINAL-WORLD-ACTOR-ORDER.md` records the static original LoadMap evidence.
The portable coordinator now follows that initial stable partition: retain
slots0/1 unconditionally, then nonnull static actors, then nonnull dynamic
actors, preserving ascending old-slot order within each group. The native
first-dynamic split is stored before the dynamic scan. This is not the later
exported `CompactActors` operation and does not filter pending-delete actors.

The actual runtime uses the loader-owned current Level, exact declaring
`Engine.Actor.bStatic` schema and existing caller-owned work/memory limits.
Native slots and adapter reverse indices change together; source export
ordinals, UObject indices, lookup/hash publication and collision identities
do not change. The complete before-image is retained in the same gameplay
root, before any slot replacement. An interruption restores the old slots,
split, reverse indices and change marker without allocating or calling actors.

Deletion after this full before-image must not append a sparse undo record
using the newly reordered slot number. Earlier sparse deletion records remain
necessary and are restored after the full snapshot. The owned Training test
uses an actual crate that moves to a slot with a different old occupant to
distinguish correct composition from a superficially successful rollback.

## Portable checkpoint representation

Portable runtime envelope17 contains script codec12 when native Actor order
is present. The section owns exact Level identity, optional native split and
all ordered null/authored-export/birth slots. Birth manifest membership can be
explicitly absent; nonmember identity is not confused with an appended slot.
These are portable continuation formats, not claims about original archives.

Preflight authenticates local source ULevel membership, Actor classes,
reserved slots, canonical birth identities and explicit removal records for
omitted actors. Publication stages complete replacement containers first.
Older portable saves reconstruct immutable original source membership, not
the currently compacted or reordered live prefix. Absence resets the split.
Their retained birth-export tail also keeps the validated old tail-index
order, independent of the codec's lexicographically sorted manifest.
Same-current-map source-path aliases reuse the loader's exact spelling only
after filesystem equivalence proves they identify the same actual file.

Door motion restoration resolves the saved membership and immutable authored
keys, not the old world's removed flag. Quickbelt restoration uses saved
lifecycle/property values or immutable authored/frozen-birth defaults, not
old live inventory overlays. These checks do not waive existing native
deferred/linker/conversation/collision archive fences.

## Verification and remaining gate

Generated coordinator checks cover order, reserved nulls, births, exact work
and allocation refusals. Codec checks cover independent bytes, legacy bytes,
permutations, malformed identities/tags, bounds and truncation. The owned
test is restricted to first Training and observes actual runtime mutation,
same-root native crate deletion, rollback, cold checkpoint replacement and
unchanged original sources. This native deletion diagnostic is not weapon
damage, surviving usable loot or falling-crate gameplay acceptance.

The startup probe observes the partition after all reached PPP callbacks and
continues to an explicit, non-publishing client/player/detail-setup refusal.
Login, possession, later original world Tick and complete Training gameplay
remain required. No backend-only APK is represented as a playable update.

Verified from frozen sources on2026-10-22 so far:

- Generated coordinator:1166 cases/69369 checks;56 provider,56 shared-work,
  5 allocation-admission and2 write-prefix refusals.
- Script codec:8824 checks/7176 rejection controls, including independent
  native-order wire bytes and unchanged legacy representations.
- Strict ARM64 C++20 syntax/type checks with `-Wall -Wextra -Werror` pass for
  the runtime and all five changed/new desktop test/probe sources.
- Android native target `deusex_quest` builds and links successfully.
- Generated actual runtime Spawn integration:10217 checks/169 refusal controls,
  including the unchanged interleaved/double-digit birth publication test and
  actual v17 cold/warm static/live/removed birth restoration, null slots,
  nonmember sentinel, real partition rollback and allocator continuation.

The final exact-source owned Training suite passes211113 checks/28 refusals, including genuine partition
then crate deletion, removed-current door motion and selected crowbar restore.
Final exact-source startup attempts reached ClientSetup after all1335 PPP visits and exact
1364-to1334 slot partition/split1183, with full rollback/all40 originals intact.
They deliberately exit2: startup is unsupported, not successful. Shared work
is513983 cold/513611 warm of the same1048576 diagnostic allowance.
The coherent desktop build passes. Final shared CTest passes164 executed/194
registered,30 explicit owned-data skips, zero failures in49.98s. Owned-data
skips are not headset/map evidence. The separate original first-Training crate
diagnostic passes347 checks/two genuine contents refusals; its isolated
Game=None setup still deletes the new Lockpick, not surviving usable loot.
The separate actual linker detachment regression passes38456 checks/160
refusal prefixes; deferred deletion passes429/21 with18 writes/20563 shared
work. All their original-source/complete rollback checks pass.
The actual first-map movement regression passes122706 checks/21 refusals,
including the real upper/lower crate sweep, native Move/MoveSmooth and genuine
Doctor animation-clock preservation. These are not automatic falling tests.
The final shared CTest includes the expanded explicit-birth controls.
Logs are under `artifacts/training-world-order-*-20261022.log`.
