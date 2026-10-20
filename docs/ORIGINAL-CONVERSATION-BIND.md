# Original conversation binding: first Training prerequisite

Source checkpoint2026-10-20. This implements the complete detached original
wrapper and list-binding protocol plus a static first-map source audit. It does
NOT yet register native2100/2101/2102 or construct actual new ConListItem
UObjects. The real startup boundary remains Plant2/PostPostBeginPlay/native2101
observed in TRAINING-FIND-BASE.md. No replacement APK or finished crate-physics
claim follows from these protocol tests.

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

## Required before native registration/publication

- An exact non-Actor ConListItem allocation domain: real class/defaults,
  package Outer, original naming counter, native object indices/weak lookup,
  initialized fields/flags, memory accounting and full root undo. Actor Spawn
  or the Extension Window allocator must not impersonate this type.
- Original deletion/identity retirement at purge, while still permitting full
  outer rollback. Rebinding must not leak dead chains or retain old indices.
- Real qualified mutable head/con/Next GC edges. String-valued Object overlays
  alone are not GC roots; appending stale links to immutable roots is incorrect.
- Explicit transient-property, ownerRefCount, package-object lifetime, map
  unload and save/restore/rebind policy. Do not save dangling constructed paths
  or let package-owned nodes survive map teardown accidentally.
- Actual first-Training positive runtime bindings, repeated purge/rebind,
  allocation/write/work/name refusal and GC/save/lookup/clock/RNG rollback,
  followed by real complete startup and live crate/usable-loot/gravity tests.

These are the next implementation steps, not a reason to skip conversations
or force startup flags. The long-term campaign objective stays intact; other
actual-map integrations remain deferred until first Training fully works.
