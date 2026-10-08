# Original authored struct values

This offline source update connects serialized `Core.Struct` declarations to
the scoped actor interpreter and current-map checkpoint schema. It is not
inventory spawning, world startup, AI or full campaign execution. The installed
Quest test APK remains unchanged.

## Declaration and wire format

`LoadPortableStructDescriptor` requires an actual qualified `Core.Struct`
export. It validates the complete UObject/UField/UStruct record, retains source
metadata and both raw/normalized script bytes, and rejects invalid references,
outer cycles, oversized/truncated payloads and trailing bytes. Class and State
suffix decoding retain their previous behavior.

The runtime builds a schema lazily from base declarations followed by the
original Children/Next property order. Every child must belong to its declared
struct; names and qualified identities must be unique after case folding.
This is the order used by the pinned `UStructProperty::LoadStructMemberValue`.
It does not use alphabetic map ordering, native pointers, host `sizeof` or
guessed memory offsets to decode values.

`quest_authored_struct_value.h` reads bounded little-endian scalar/nested member
streams. Compact object/class and name indices resolve against the package
that supplied the **value**, which may differ from the struct declaration's
package. A struct tag must match the declaration's UObject/export `Name`, not
the independently serialized `FriendlyName`; comparison is case-insensitive.
Null references, signed integer counts and exact byte/float representations are retained.
Boolean members follow the pin's byte-equals-one contract. `StrProperty`
members use the pinned package-version string format; name text is not limited
to ASCII path identifiers. Nonfinite floats and extra/truncated bytes fail.

Imported object references also check the source import's declared `ClassName`
against the loaded target's real class ancestry, including the pinned special
case for Class. The import's ClassPackage is not substituted for that original
unqualified comparison. Ambiguous literal dotted/NUL import name segments fail
explicitly; they cannot alias a different nested target through a flattened
display path. Export Names and struct tag Names are not globally restricted
to dot-free text.

Vector and Rotator remain dedicated VM value kinds, but their field names,
kinds and wire order now come from their actual declarations. Ordinary structs
use canonical case-folded named members. PointRegion is no longer the sole
hardcoded ordinary struct schema.

## InventoryItem and persistence

The original `DeusEx.ScriptedPawn.InventoryItem` declares `Inventory` as
`class<Engine.Inventory>` and `Count` as a signed integer. The original
`InitialInventory` is an eight-slot fixed array of these values. Map overrides
and inherited defaults resolve independently per slot using the actual source
package. No inventory entries are synthesized by reading this property.

Actual compiled `AddInitialInventory` can write these members transactionally.
Its actual available-slot predicate is `Inventory == None && Count <= 0`.
The scoped interpreter now implements the original pre/post integer increment
and decrement natives (163--166), including detached return snapshots and
shared write-budget/rollback accounting; the helper needs postincrement165
for its real slot-search loop.
When the selected authored NPC has no such slot, the integration test preserves
the real helper's false return and unchanged data, then uses an explicitly
generated, isolated empty-slot v4 overlay for positive write controls. That
fixture is not a change to the NPC's defaults or evidence of spawned gameplay.
Reflected writes validate nested object/class constraints before changing live
storage. An unrelated class, actor instance or unavailable identity cannot
be stored in the Inventory member. A failure after one member write rolls back
the complete caller transaction.

Save preparation validates every nested field against the original declaration,
including class ancestry. Missing/extra/wrong-kind/case-colliding fields fail
before application. Valid mixed-case field names normalize canonically. These
values fit the existing structural codec: property-only records retain the
v4 envelope, state composition uses v5, and legacy restoration removes overlays.
No new save envelope is introduced. Existing 16 MiB combined save and travel
guards still apply.

## Explicit bounds and unsupported behavior

Schema construction bounds depth, declaration count, child chains, cumulative
field count and aggregate retained schema/descriptor bytes (64 MiB per scoped
host). Descriptor retention includes both raw and normalized script capacities
and identity storage, checked before normalization and again before caching.
The runtime-wide lazy descriptor cache independently shares a 64 MiB bound
across hosts. These are conservative retained-storage bounds, not a claim that
transient input/decoder allocations are absent. Schema path, declaration Name,
field identities and reference constraint strings are charged too.
The pure codec validates a schema before constructing values and bounds input,
nodes, depth, retained structure/text and callback-expanded strings.

`ReadPortableActorScriptPropertySlots` provides a read-only contiguous fixed-array
inspection with one scoped host, preserving the original source/default/overlay
rules for each slot. It rejects empty/over-1024 requests and overflow/out-of-range
spans before reading. Result values share a 32 MiB/32,768-node budget; they do not
receive a fresh budget per slot. The existing single-slot API is unchanged.

Nested fixed-array members, dynamic arrays, maps and unsupported property kinds
are not flattened or silently skipped. In particular the pin's legacy
`StringProperty` has no struct-member loader override; it is not confused with
`StrProperty`. Top-level fixed arrays remain independently indexed properties.

This change does not implement MetaCast, dynamic Spawn, Inventory lifecycle,
level-wide startup, AI managers, latent behavior or per-map script archives.
Unsupported required behavior must still fail atomically. Successful helper
execution or save roundtrips are not evidence of working campaign gameplay.

## Verification commands

From the project directory, with the existing desktop toolchain configured:

```powershell
cmake -S desktop -B desktop/build
cmake --build desktop/build --parallel 4
ctest --test-dir desktop/build --output-on-failure
.\desktop\build\portable_struct_descriptor_test.exe --audit-original 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex' --inventory-only
.\desktop\build\portable_actor_script_test.exe 'D:\Steam\steamapps\common\Deus Ex'
.\desktop\build\portable_runtime_state_test.exe --game-root 'D:\Steam\steamapps\common\Deus Ex'
```

The first controls are synthetic and asset-free. The original-data commands
read the user's installation and create isolated temporary saves; none of that
commercial data is checked in. The focused inventory option is additional:
the normal actor invocation retains the full metadata/dispatch/helper/state
integration suite.

Current verification: reviewed final host build/CTest pass (32 ordinary passes,
two optional original-root tests skipped there). Pure values pass 437 checks/121
rejections, descriptors 500/137 plus 33/24 import-provenance controls, and extended
VM controls 354/75. The separate original metadata audit and final reviewed-source
runtime/save/map integration completed exit 0, including the last read-batch
budget/test-fixture edits. The full final-source actor suite completed with its
terminal PASS. All eight slots of 117 ScriptedPawns (936 slots total) matched
independent original values across Training, Liberty Island and Intro, including
123 map and 86 inherited nonempty entries. Actual helper member writes, v4/v5
inventory composition, 18 malformed-schema rejections and exact
InitializeInventory MetaCast13 PC123 refusal with full StartUp rollback passed.
The full metadata/layout, human/robot/bird, reference/Level, animation/save,
SetPhysics and state controls also passed. Actual ordinary StartUp still stops
at unsupported conversion57 in InitializeHomeBase PC35; begun-play BeginState
still stops at required AIEndEvent715 in SetDistress PC46. Those atomic failures
are not campaign startup. A case-colliding save control separately asserts encode refusal
and precisely patches one generated field's name bytes for read-side refusal,
instead of aborting the suite while creating its fixture. Earlier failed
fixture/native165 and interrupted superseded runs remain diagnostic evidence only.
