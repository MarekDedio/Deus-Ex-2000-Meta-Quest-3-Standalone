# First Training live loader attachment

Current source increment,2026-10-21. Actual-map scope is only `00_Training.dx`.
This continues the real destruction prerequisite from
TRAINING-PROPERTY-TEARDOWN.md; it is not completed Training, crate gameplay,
surviving usable loot, gravity or headset acceptance.

## Ownership and original operation

The portable loader now owns a stable native service per loaded source, with
its real mutable ExportMap object pointers. It is distinct from the Package
root UObject, the combined runtime export vector, UObject index leases and
immutable source-file/export provenance. No original ULinker class/vtable is
constructed or executed. Initial script and map loaders bind each actually
materialized export at its exact zero-based source-table position. Fresh Spawn
and checkpoint-reconstructed births explicitly start without an attachment.

Original SetLinker retains the old loader/index, reads the selected live slot
twice for the original nonnull/equality assertions, clears that retained slot,
then writes the new loader and index. UObject.Destroy subsequently caches the
current Outer's real UObject index in that same native index field. Source
paths/ordinals, UObject lookup/index and unrelated payload/clocks remain intact.
Nonnull reassignment to another loader remains an explicit unsupported service;
the actual base-destruction operation requests `(None,-1)`.

Object-header before-images and per-export-slot before-images share the caller
transaction. Slot rollback is allocation-free and occurs before provisional
birth retirement. Raw native loader/index stores borrow the caller native-work
ledger (like native Level slot stores), not a private transaction or allowance;
actual flag/payload stores continue using the VM write journal. Owning loader
storage and actual journal/reserve capacity are charged to the existing state
limit. New native-header before-image fields include an alignment allowance,
preflighted before allocating the caller's journal node and retained until its
outer disposition. Shared loader ownership is counted only once, not cloned
per touched object. No memory or work limit is raised.

The initial dense duplicate export-pointer array passed isolated teardown
tests but regressed real startup's combined memory limit. It was rejected.
The replacement preserves every source-local slot with a view into the loader's
already-owned baseline vector OBJECT (not its relocatable data buffer), plus
explicit mutable changed-slot nodes. A stored null does not fall through to
the baseline. Combined runtime ownership is never overwritten by detachment.
All sparse nodes/reserve peaks are budgeted; no source/actor is dropped and no
query cap or16MiB combined-state limit is relaxed.

The live ExportMap participates in GC rooting. Map retirement invalidates the
loader lease and clears its slots before releasing the map; stable shared
storage cannot expose a recycled vector-entry identity or dangling live lease.
The unchanged combined runtime roots still retain conditionally destroyed
objects until the real deferred-deletion/cleanup service is implemented.

Changed native attachment state explicitly refuses checkpoint capture/rebind
and map replacement until a faithful native archive exists. Timer/level-clock
publication classifiers include native header changes, so unrelated Timer
overlays cannot accidentally hide destruction as a timer-only update.

## Verification boundary

ReadPortableRuntimeObjectLinker exposes the actual native attachment, reused
index, original slot and UObject/Outer identities without inferring linkage
from an absent script tag. ProbePortableActorConditionalDestroy runs actual
native destruction twice in one rollback-only root, capturing even interrupted
intermediate states; the second completed call must take the destroyed-flag
return0 path. This is not a published DestroyActor or gameplay deletion.

The owned test uses only first Training plus shared script dependencies and
authentic Package/lookup ownership. It checks distributor/crates, original
source-table indices/flags, all VM write prefixes, every warmed native-work
prefix, and exact native attachment/full-state rollback. Its intermediate
observations distinguish export clear, loader clear, index=-1 and Outer-index
cache. The compact implementation passes38456 checks/160 refusal prefixes on
the actual distributor and both crates, with14/8/8 VM writes and52/39/39 warmed
native-work prefixes respectively. All39 original source files retain bytes,
size and timestamps; full native attachment/save/revision/lookup/Actor/GC/RNG
rollback passes. Existing real property teardown checks pass1164/33.

Two compact-build actual first-map startup attempts pass the earlier memory
regression and reach the next explicit deferred-deletion ownership boundary
at PickupDistributor.PostPostBeginPlay PC244/slot1195. Both execute533311
instructions/39754 writes and all26 Spawn observations, including the real
NanoKey clear path. Native work is448610 cold/448238 warm of the unchanged
1048576 allowance. Complete state rollback and all40 source fingerprints pass.
The probe still exits2: startup did NOT complete. Android native link and strict
ARM64 runtime/new-test syntax pass. The final memory-accounted coherent desktop
build passes. Shared CTest executes162/189, with27 explicit owned-data skips,
zero failures in65.10s. Separately executed actual first-map movement passes
122706/21, metadata11130/102, config9977/48 and conversation156871/17. The
isolated crate fixture remains347/two: Game=None still removes its Lockpick,
so it is explicitly NOT surviving loot. These skips are not claimed as map or
headset evidence. Final exact-build startup and teardown repeat the same
counts/boundary above with full new header before-image accounting enabled.
Logs use `artifacts/training-linker-*-final-20261021.log`, including additional
`*-budget-final-*` actual metadata/config/conversation/crate repeats.

Deferred Level.FirstDeleted/Actor.Deleted ownership and eventual native cleanup,
unhash/index retirement and archives remain separate required work. No
backend-only APK is packaged for this increment.

ORIGINAL-LINKER-ATTACHMENT.md records primary static allocation/CreateExport/
SetLinker authority. ORIGINAL-DEFERRED-ACTOR-OWNERSHIP.md records the next actual
serialized FirstDeleted/typed Deleted baseline and later cleanup requirements;
it does not implement that next provider or justify an unconditional null head.
