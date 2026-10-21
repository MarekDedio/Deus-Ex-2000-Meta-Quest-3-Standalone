# Original world Actor ordering (2026-10-21)

Read-only static prerequisite for completing ONLY `Maps/00_Training.dx`.
No original commercial DLL/EXE was loaded or executed, no map/runtime was
initialized, no other map was tested, and no compiler input was changed by
this audit. Addresses and byte offsets below describe the original PE32
profile, not portable pointers or C++ layouts. This is not an implementation
or acceptance claim for startup, saves, Tick, crate loot, gravity or a new APK.

## Source authority and reproduction

The following owned sources under `D:/Steam/steamapps/common/Deus Ex/System`
were fingerprinted during this review and remain unchanged:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Primary static evidence already exists in ignored artifacts:

- `artifacts/world-startup-contract-20261011/original-loadmap.asm`, especially
  original addresses1038DC65..1038E053 (partition begins at1038DCD5).
- `artifacts/world-native-startup-20261011/original-level-tick-body.asm`,
  including reads of the native split at103A5620,103A56A0 and103A5793.
- `artifacts/collision-hash-save-audit-20261018/engine-actor-properties.log`,
  actual Engine.Actor Children/Next metadata inventory.

The external MinGW `objdump.exe` was also used to independently reread bounded
static instructions and the Engine export table. Reproduce without invoking
any original function:

```powershell
$orderObjdump = 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe'
$orderEngine = 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
Get-FileHash -Algorithm SHA256 -LiteralPath $orderEngine
& $orderObjdump -p $orderEngine
& $orderObjdump -d -Mintel --start-address=0x1038dcd5 --stop-address=0x1038df47 $orderEngine
& $orderObjdump -d -Mintel --start-address=0x10302248 --stop-address=0x1030224d $orderEngine
& $orderObjdump -d -Mintel --start-address=0x103963e0 --stop-address=0x1039650e $orderEngine
Get-FileHash -Algorithm SHA256 -LiteralPath $orderEngine
```

UGameEngine.LoadMap is export thunk103033A5 ->1038C1F0. Exported
ULevel.CompactActors is a DIFFERENT operation: export-name-table index846,
ordinal847, thunk10302248 ->103963E0. The initial LoadMap partition is inlined
in LoadMap; it does not call that exported CompactActors body.

## Initial LoadMap stable static/dynamic partition

WORLD-BOOTSTRAP-CONTRACT.md records the preceding fresh/restored-world phases.
Both paths converge on the live PostPostBeginPlay loop1038DC65..1038DC9C.
Only after that loop finishes does LoadMap enter1038DCD5..1038DF46.

Actual native Actor-array storage is Level+2C, count Level+30 and capacity
Level+34. The phase constructs a distinct temporary pointer array before
replacing those live contents. Original pointer element size is4 bytes.

| Step | Primary instructions |
| --- | --- |
| Initialize temporary array, append old slot0 | 1038DCD5..1038DD08 |
| Append old slot1 | 1038DD0A..1038DD46 |
| Ascending scan from old slot2, append nonnull actors with Actor+28 bit1 set | 1038DD49..1038DDA3 |
| Store temporary count into actual Level+110 | 1038DDA5..1038DDB0 |
| Ascending scan from old slot2, append nonnull actors with Actor+28 bit1 clear | 1038DDB6..1038DE0B |
| Original undo-aware remove-all, append output-sized array storage, copy retained pointers | 1038DE0D..1038DED5 |
| Destroy temporary array storage and continue | 1038DED7..1038DF46 |

The result is the concatenation:

```
[old slot0, old slot1]
  + [nonnull static actors in ascending old slots2..end]
  + [nonnull nonstatic actors in ascending old slots2..end]
```

The native split Level+110 is the output count **after** the reserved pair and
static pass, **before** the dynamic pass. Subsequent Tick uses that exact
native field as its dynamic start, independently corroborating its role. This
audit does not invent a reflected property name, a second networking split,
or an archive representation for that native field.

Important exact behaviors:

- Slots0 and1 are copied unconditionally. Neither is subjected to the static
  test or null filtering. Original code assumes at least two accessible slots;
  a bounded portable adapter must refuse invalid input instead of reading
  absent storage. Slot0's separate LevelInfo authority is established by the
  genuine world loader/startup, not by this pointer-copy fragment.
- Null holes at slots2 onward disappear. Nonnull actors remain selected solely
  by the static bit. There is NO bDeleteMe, bHidden, active, class, render,
  runtime-birth or worldRemoved predicate here.
- Selection is stable within each group. No pointer sort, unstable partition,
  swap-with-last or export-order reconstruction reproduces this sequence.
- Both scans reload actual Level actor-array count and storage as written in
  the original body. There are no Actor gameplay callbacks in these scans;
  actual GUndo and allocator services remain distinct native providers, not
  permission to make arbitrary mid-pass mutations.
- PPP births are already present in the old live Actor array. The partition
  does not distinguish them from authored actors. A static birth can move
  before authored nonstatic actors; dynamic births retain their relative order
  after preceding old dynamic actors. Removing the authored/birth boundary is
  an original result, not an accidentally unstable implementation.
- Duplicates, if admitted by an invalid earlier loader, are not deduplicated
  by this fragment. Earlier native identity/slot validation is separate.
- The partition copies native pointers only. It performs no Actor callbacks,
  ConditionalDestroy, queue cleanup, scalar deletion, UObject lookup unhash or
  object-index release. It neither changes immutable package export ordinals
  nor republishes the UObject object-hash insertion order.
- The bounded partition interval contains no collision Hash.RemoveActor or
  AddActor call. A portable slot permutation must not rebuild an identity-keyed
  collision hash merely to make its iteration order match the new slots.

### Named static-bit authority and its boundary

The immediate primary branch is `test byte ptr [Actor+28],1` at1038DD63 and
1038DDCB. It is the authored Actor flag DWORD after the28-hex-byte UObject
header, NOT UObject.ObjectFlags at+1C. Existing independently inspected
Engine.u metadata records the exact declaring field:

```
Actor.bStatic ref4873, BoolProperty, dimension1, flags00000003, Next76
Actor.bHidden ref76, BoolProperty, dimension1, flags00000021, Next4868
Actor.bNoDelete ref4868, BoolProperty, dimension1, flags00000003
```

The actual Children/Next chain puts bStatic first in the consecutive Actor
Bool group; ORIGINAL-ACTOR-MOVE.md and ORIGINAL-DESTROY-ACTOR.md document its
corroboration with native bit1. That is finite named-field/source evidence,
not permission for a portable adapter to interpret arbitrary same-named fields
or read a nonexistent serialized native bitmask. The Core Bool-property Link
and Serialize details in ORIGINAL-DESTROY-ACTOR.md explicitly distinguish
computed native packing from .u payloads. This review did NOT run a new source
test or reconstruct the entire native Bool-link layout independently. The
runtime consumer must authenticate exact Engine.Actor.bStatic declaring
identity/type/dimension/flags and use that live field, or retain a clear refusal
when native field authority is unavailable. A leaf-name Bool/default false is
not an adequate replacement for the selected original branch.

## Later exported CompactActors: dynamic suffix only

The complete normal exported body103963E0..1039650D is not a new partition:

1. At1039640A..10396410 initialize destination cursor and ascending source
   cursor to actual Level+110.
2. While source cursor is below the live count, read the live source slot.
   Null slots are skipped. A nonnull actor whose Actor+28 bit80 is clear is
   copied to the current destination, then both cursors advance.
3. A nonnull actor with bit80 set enters a diagnostic logging branch at
   10396430..1039644C, then only the source cursor advances. This is not the
   actor's deleting destructor or a new DestroyActor callback.
4. Once the scan finishes, if the destination count differs from old count,
   invoke original undo-aware native array removal for the trailing range
   [destination, old count) at10396452..103964F7.

Thus surviving dynamic actors keep stable order. Reserved/static slots before
the split are left untouched, and this body does not rewrite Level+110.
The source bit80 is independently identified as bDeleteMe by the actual Actor
Bool group and ORIGINAL-DESTROY-ACTOR.md. No source actor is reclassified by
bStatic in this later service. It does not purge FirstDeleted, release UObject
indices, or replace the distinct CleanupDestroyed ownership contract.

## Partition is not completed startup

After1038DF46 the original LoadMap body still performs other native work.
The bounded observations include native client/local-player setup through
1038DF54..1038E037 and Level detail-change virtual+D4 at1038E053. Their
complete selected-service semantics, real Login/SetPlayer/possession and later
world Tick remain separate acceptance requirements. Completing a stable slot
partition must not silently accept those unimplemented services or claim a
running original tutorial.

## Proposed portable ownership requirements, NOT original format claims

The following are implementation consequences, not recovered original archive
bytes or completed code:

- Store an actual owned native split with the current native Level/map
  generation. Reordering is a world-root mutation: keep a bounded exact
  before-image of affected slots/count/split and cached adapter indices.
  Existing append-count truncation or individual deletion-hole journals alone
  do not restore a full permutation. Restore old edges/order before retiring
  provisional births; no callbacks or allocating work belong in rollback.
- Update cached `worldActorIndex` consistently with live slots without changing
  source export ordinal, actual UObject object index, lookup identity or hash
  publication sequence. Native slot order affects future ascending world
  scans and iteration/tie behavior; it cannot be represented as a render-only
  permutation.
- The current portable save format's authored-slot prefix plus contiguous birth
  tail cannot represent this original partition. A future owning continuation
  format must explicitly represent native slot order, nulls, native split and
  identities of authored actors and births, or publication/archive must remain
  refused. Do not infer original save semantics from the port's old format.
- Subsequent destruction can create new holes; later CompactActors has its
  separate dynamic-suffix rules. Preserve reserved slots and do not recreate
  the live order from tags, classes, a sorted name list or a birth manifest.
- Proposed controls must exercise interleaved static/dynamic actors, null slot1,
  holes after2, authored and PPP-born actors in both groups, pending-delete
  nonnull slots, current live static values, exact refusal-prefix rollback,
  unchanged object/hash identities and cold restoration of the full native
  order/split. Generated controls are not actual first-Training acceptance.

Only the original first Training map is the current actual-map integration
target. These shared prerequisites do not justify other-map integration or
testing before the user's first-Training milestone is complete.
