# Original conversation binding: first Training prerequisite

Source checkpoint2026-10-20. The original wrapper/list protocol now has actual
native2100/2101/2102 runtime providers, non-Actor ConListItem construction,
typed mutable GC edges and transactional deletion/index/lookup retirement.
Actual first-map positive/rebind/rollback tests cover2101/2102; possessed-player
2100 is not yet exercised. First Training startup passes the previous Plant2
native2101 boundary and next refuses the spawned CheckEncroachment at
PickupDistributor0. Startup and playable crates are still incomplete. No new
APK or headset acceptance is claimed for this source/runtime prerequisite.

## Actual runtime lifecycle checkpoint

`native/portable_conversation_bind.inl` supplies the original providers in the
same PortableActorVmHost/Execution root. It admits the real preloaded source
linker's ConversationList, including actual class subclasses; it is NOT a new
general package-loading implementation. An unavailable source linker refuses;
an absent/wrong-class object in an existing linker yields original null-load
behavior. The native Level slot scan retains live counts and no bDeleteMe
filter. The original function declarations/qualified scalar field schemas are
validated before their corresponding operations.

ConListItem is neither an Actor birth, Window nor runtime export. Its actual
class flags/Within, source/default copy, package Outer, concrete-class unique
counter, zero constructor fields, Name and native index are retained. Initial
flags0 become RF_Public4 through the protocol. Actor head, node con/Next and
Conversation.ownerRefCount use physical native storage with root snapshots;
they do not become script overlays, serialize dangling node paths or reset an
unrelated NPC animation clock. Mutable GC edges replace old links rather than
accumulating in the immutable authored reference graph. The node inventory is
weak, not an accidental package/root ownership list.

Purge immediately removes node path aliases and both native weak-index chains,
retires its real allocation-slot lease, clears native links and makes the slot
available to subsequent allocations in the SAME root. The physically retained
old object is exclusively a rooted transaction before-image, not a live node
waiting for ordinary GC identity retirement. At the outer boundary GC reclaims
committed retired before-images. Rollback restores old aliases/hash precedence,
publication links, physical fields/counts/flags and actual leases; provisional
objects/indices are invalidated. Native deletion has no script destructor.

`ObjectIndexPool::Transaction` uses a finite4096-entry root journal (65568 bytes
on64-bit), tracks all subsequent pool Acquires, permits constructor unwind and
restores free-list links/head/live count/slot frontier without allocation. It
does not copy the104k-slot object pool or increase capacity policy. Existing
pre-root objects must stay alive until the transaction ends; GC is forbidden
inside roots. The pool may retain ordinary vector backing capacity after a
refused growth; slot identity/state, not allocator capacity, is rolled back.

`ObjectLookup::Index::Extract/Restore` retains detached map nodes and original
name/identity neighbor positions. Tokens authenticate an exclusive owner and
require reverse mutation/LIFO restoration, including later publications using
the same numeric slot. Canonical/folded aliases use extracted owning map nodes.
The host reverses later publications before each retirement, preserving exact
precedence without allocating or copying the approximately12MB live index.
The new owner authority costs one fixed uint64 per index, not per object.

Constructor refusal also uncovered a dependency GC bug: GC::Alloc had linked
and counted memory before constructing, but its exception cleanup only freed
the memory. The checked-in GCConstructorRollback.patch now unlinks the exact
failed allocation and restores statistics even if successful nested children
precede it. No incomplete object's destructor is invoked. Dependency setup
applies the patch with forward/reverse checks and preserves unrelated edits.

Checkpoint/rebind and per-map purge/archive are NOT finished. Capture, applying
a checkpoint and map unload explicitly refuse while live native bindings/count
contributions exist. State-presence/dirty queries report them. Empty bindings
write no serialized invented state. Shutdown retires weak identities and drops
the inventory before collection. These safeguards are not save/travel support.

Actual source test `portable_conversation_bind_test` passes156871 checks/17
refusal controls with all39 source packages/map bytes and timestamps unchanged.
An independent470-entry source oracle predicts Jaime's4 bindings and two
troopers' separate2-node chains sharing the SAME conversations. Tests cover
actual class/package/flags/header/indices/lookup, repeated fresh-name purge,
balanced counts, late write/native-work refusal restoring exact old graph,
names/lookup/revision/GC and a real Doctor native clock. Archive/load/unload
refusals preserve the live graph. Only Maps/00_Training.dx is loaded.

Independent helper tests pass302701/23 for index leases,216422/59 for weak
lookup and1905/69 for GC construction; detached protocol remains15314/63.
Strict ARM64 syntax and coherent Android native compilation/link pass.
Final coherent shared CTest passes158 executed/183 listed with25 explicit
owned-data skips and zero failures in45.54s. The new owned conversation test
is one such default skip and is executed separately above; skipped tests are
not gameplay/headset evidence. The independent compact-layout regression
passes686275 checks, including the explicit fixed8-byte authority cost and
unchanged32-bit head/per-node savings. First-map FindBase regression passes
168495/25; the isolated original crate-destruction fixture remains347/two,
not live melee or surviving loot.
Repeated original startup with explicit1048576 native-work allowance reaches
PostPostBeginPlay slot1195/PickupDistributor0, Engine.Inventory.PostBeginPlay
PC60/opcode11, then refuses unsupported original spawned CheckEncroachment.
It completes1193 PPP visits, uses436956 native work,532910 instructions and
39672 writes. Both attempts restore the complete save/hash/clock/RNG/client/
lookup/Actor/GC state and preserve all40 sources. Exit2 is an observed refused
startup, NOT successful startup. The default native-work allowance is unchanged.

Evidence: artifacts/training-conversation-runtime-final-owned-20261020.log,
training-conversation-runtime-primitives-20261020.log,
training-conversation-runtime-startup-20261020.log and
training-conversation-runtime-final-arm-20261020.log. Final host/Android/shared
suite logs use the same prefix with final-host/final-android/final-ctest.

## Next first-map startup dependency

Original SpawnActor export thunk103016DB jumps to body10394F60; static evidence
is artifacts/spawn-frame-audit-20261011/original-level-spawnactor-disassembly.log
and verification.md. The native+34 input is zero for current script/direct-world
births. After PostBeginPlay, copy CURRENT Rotation first and CURRENT Location
second, then call retained native Level.CheckEncroachment(birth,pose,0). Do not
prefilter using the old three-flag guard or reuse the initial Spawn arguments.
The complete real provider already exists as MoveLevelEncroachment in
portable_actor_move.inl and owns its original admission/callback/touch policy.

The false result continues SetInitialState, eligible FindBase and PPP. True
calls real Level.DestroyActor(birth,0), ignores that return and returns None.
The old native279 path still refuses active retail Destroy ordering, so a true
result cannot be faked as deleted/None or skipped. Connect the same original
check in the ordinary begun-world branch too: eventual crate contents use it,
not just the startup probe. Native Mover Spawned, SpawnNotify and birth-tick
queue behavior remain separately unsupported where encountered.

## Primary evidence

Original DLLs were inspected as PE bytes/disassembly only, never loaded or
executed. Bounded full normal bodies, deletion/constructor helpers, import maps,
Core allocation/unique-name bodies and source fingerprints are in ignored
`artifacts/training-con-bind-audit-20261023/`. The directory date is inherited
from the audit artifact, not a claim that today's source inspection is later.

- DeusEx.dll SHA256 `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04`.
- ConSys.dll SHA256 `F20C4D8904D17057318999C18673D82E2CCC3F7C1041EB3C0327EB731B3A8EFA`.
- Core.dll SHA256 `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

Decoration2101 thunk1000E9D0→1000EA50, Player2100→10014320 and
ScriptedPawn2102→10019140 have equivalent normal wrapper control flow.
ConSys.BindConversations is10010B70..10010CEB; PurgeBoundConversations is
10010CF0..10010D34. Each actual .u native declaration has flags0x401, native
index2100/2101/2102, no parameters/results and zero logical script bytes.

## Wrapper authority

Scan the receiver's native XLevel Actor array in ascending LIVE slot order,
skipping nulls and selecting the first actual DeusExLevelInfo UClass/subclass.
Do not read mutable Actor.Level, cache the count before the loop, use the engine
current mission, or add a bDeleteMe filter. If absent, emit the original missing
info diagnostic and leave existing bindings alone. A negative MissionNumber
also returns without reading the package or altering bindings.

ConversationPackage is a String. Compare it to `DeusExConversations` with
original C-locale case-insensitive TCHAR comparison. Equal selects prefix
`DeusExCon`; all other values, including empty, remain verbatim. Format
`<prefix>Text.ConList_Mission%02d`. Two digits are MINIMUM width, not a maximum.
Call StaticLoadObject with exact ConversationList class, Outer=None,
filename=None, loadFlags2 and sandbox=None. Only a nonnull result calls binding;
a missing loaded list does not purge existing nodes.

The native wrapper's three128-TCHAR stack buffers are bounded explicitly in
the portable protocol. Would-overflow input refuses rather than reproducing
memory corruption. General text/work/link limits are portable safety policy,
not extra original gameplay filters.

## Actual binding and destruction order

1. Capture ConversationList.conversations BEFORE purge.
2. Traverse Actor.ConListItems. For each old node, load its conversation,
   decrement ownerRefCount with DWORD wrap, capture Next, then invoke its
   scalar deleting destructor with argument1. This is real destruction, not
   just unlinking/GC. After the loop, store Actor.ConListItems=None even when
   initially empty.
3. Walk the captured source ConItem list ONCE, reading each live Next only
   after matching/allocation/stores for that entry.
4. Bark classification is case-sensitive substring `_Bark` in the
   Conversation.conName FName text. It is NOT UObject.Name/export path.
   Root review corrected an initial audit note:10010B9C reads `[conversation+28]`,
   whereas Core10157ED2/DA confirms UObject.Name+20/Class+24. ConObject adds no
   fields; conName is the first Conversation field. Actual source often names
   exports `Conversation1954` while conName contains the meaningful bark name.
5. Accept iff `(owner CI-equals BarkBindName AND isBark)` OR
   `(owner CI-equals BindName AND (BarkBindName is empty OR not isBark))`.
   Empty names/owners are not skipped. Preserve lazy read order, including the
   original repeated bark-empty comparison. One source entry creates at most
   one binding; duplicate source entries are not deduplicated.
6. Increment the accepted conversation's ownerRefCount BEFORE allocation.
   Allocate native exact ConListItem class with Outer=ConversationList.GetOuter,
   Name=None, flags0 and template=None. The constructor zeroes con/Next.
7. Capture new node flags; write con, then capturedFlags|4 (RF_Public), then
   Next=current Actor head, then Actor head=new node. Accepted source order is
   reversed in the final bound chain. Every rebind purges and creates fresh
   nodes; it does not reuse old nodes or reset the class naming counter.

ConSys's deleting thunk10002ED0 calls the destructor then UObject::operator
delete(pointer,size0x30). Core.StaticAllocateObject101579C0 calls original
MakeUniqueObjectName for Name=None. Unique naming uses actual Outer and the
concrete class's independently journaled counter, not a per-Actor/per-list ID.

## Exact source metadata

The static inspector validates declaration identity through real UClass.Children
and UField.Next membership, actual property UClass and declaring Outer, class
ancestry, subtype constraints and defaults/instance overrides.

| Qualified field | Concrete type | Flags / constraint |
| --- | --- | --- |
| Engine.Actor.BindName / BarkBindName | StrProperty | 0x400001 |
| Engine.Actor.ConListItems | ObjectProperty | 0x2000, Core.Object |
| ConSys.ConListItem.con / Next | ObjectProperty | 0, Conversation / ConListItem |
| ConSys.ConItem.ConObject / Next | ObjectProperty | 0, ConObject / ConItem |
| ConSys.ConversationList.conversations | ObjectProperty | 0, ConItem |
| ConSys.Conversation.conName | NameProperty | 0 |
| ConSys.Conversation.conOwnerName | StrProperty | 0x400000 |
| ConSys.Conversation.ownerRefCount | IntProperty | 0 |
| DeusEx.DeusExLevelInfo.MissionNumber | IntProperty | 1 |
| DeusEx.DeusExLevelInfo.ConversationPackage | StrProperty | 0x400001 |

All listed fields are scalar. ConListItem classFlags0x112, Within=Core.Object,
does NOT include CLASS_Transient0x8. Core's automatic RF_Transient0x4000 path
therefore does not apply to this class. The caller's explicit RF_Public4 must
not be confused with transient. ConListItems PROPERTY is transient independently
of the newly allocated object's flags. The pinned guide's explicit transient,
level-package Outer, mission-list scan and two owner-name passes are not the
original GOTY implementation and are not used as authority here.

## Implemented protocol and tests

`native/quest_conversation_bind.h` coordinates the full wrapper/binding bodies
using borrowed, typed provider callbacks and opaque CPU-owner identities.
It owns no objects or transaction. Count/link/flag stores, actual destruction,
allocation/index/name state and failures must all belong to the SAME caller
root. Exceptions propagate without a partial local rollback. Both components
charge the borrowed work ledger; their composition gets no fresh child budget.

The protocol test checks the independent matching truth table, empty/CI/C-string
rules, case-sensitive bark substring, lazy/fresh reads, captured source/flags,
duplicate entries, shared conversations across two Actors, counter wrap,
purge/rebind direction and creation/deletion order. Every provider-failure
boundary has caller-owned full rollback controls, with exact N/N-1 work,
missing providers, null inputs, cycles, text/UTF transport and tighter bounds.
Wrapper cases include missing info, negative mission, failed list load, direct
path formatting, native live-slot growth and composed same-root accounting.
Its allocation/deletion/loader providers are MOCK, not real runtime proof.

`desktop/conversation_binding_inspect.cpp` reads only eight allowed source
packages, including ONLY Maps/00_Training.dx. It selects the native-format
direct list, not ConMissionList or the presentation dialogue index. The actual
1308-Actor first map selects DeusExLevelInfo0/mission0/default ConversationPackage
and `DeusExConText.ConList_Mission00`, whose470 entries retain original order.
It checks27 inputs (22 first-map actors plus5 class-default controls), running
the full protocol twice against real static fields and MOCK nodes/counters.
Independent accepted/reversed-list predictions and rebind balances agree.
Plant2 and authored-empty Jaime0/1 accept0; Jaime2 accepts4, JosephManderley0
accepts3, WaltonSimons0/mib1 accept15 each, and each MunitionsTrooper accepts2.
No other map is read, and the shared mission package is not other-map integration.
Eleven owned package/map/DLL SHA256 fingerprints remain unchanged.

Strict host protocol execution passes15314 checks/1513 matching cases/63
explicit refusal controls. Strict NDK27 ARM64 syntax passes for both new
sources. The actual-source inspector completes with real declaration-chain
validation and all27 two-pass compositions. Final shared CTest passes157
executed/181 listed,24 explicit owned-data skips, zero failures in36.66s.
Skipped original-data/headset cases are not claimed as executed acceptance.
No Android APK is built for this helper-only checkpoint.

Evidence logs: `artifacts/training-conversation-bind-final-host-20261020.log`,
`training-conversation-bind-protocol-final-test-20261020.log`,
`training-conversation-bind-final-arm-20261020.log`,
`training-conversation-bind-actual-source-20261020.log` and
`training-conversation-bind-final-ctest-20261020.log`.

## Remaining acceptance

- Actual runtime allocation/memory-bound refusal coverage and possessed-player
  native2100 execution. Synthetic allocator/GC failures do not prove these.
- Checkpoint/rebind and per-map purge/archive of actual bindings and count
  contributions; never save dangling constructed paths or retain dead chains.
- Complete genuine first-map world/player startup, then live held-weapon crate
  destruction, surviving usable contents, support/falling and whole-map route
  acceptance on Quest. The explicit guards above are not completed features.

These are the next implementation steps, not a reason to skip conversations
or force startup flags. The long-term campaign objective stays intact; other
actual-map integrations remain deferred until first Training fully works.
