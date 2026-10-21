# Original deferred Actor ownership (2026-10-21)

Static-only audit for the first `Maps/00_Training.dx` destruction path. No
commercial DLL/EXE was loaded or executed, no runtime was initialized, and no
other map was inspected. Original PE32 addresses/offsets below are evidence,
not portable C++ pointers or layouts. This audit does not implement cleanup,
crate destruction, usable loot, falling, saves, world Tick or a playable APK.

## Source identities

Originals under `D:/Steam/steamapps/common/Deus Ex` retain these SHA256 values:

| File | SHA256 |
| --- | --- |
| System/Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| System/Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| System/Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| Maps/00_Training.dx | `BB66D95AAFC6AAE4179F8444E98661F3D5D27DAB06D13D7DDE7F4B5F4E24068D` |

An external MinGW `objdump.exe` parsed bounded static disassembly/export data.
An independent bounded PowerShell byte parser read the first map's name/export
tables and native Level payload; it did not construct or execute game objects.

## Level.FirstDeleted is serialized, not inferred transient None

The ULevel default constructor export `10301807` jumps to
`10345D30..10345E96`. Its InternalConstructor export `10303AE9` jumps to
`10345D10..1D`, which checks its supplied destination and enters that constructor.
The constructor establishes native arrays/URL/map structures and vtables but
contains no explicit write to Level `+100`. Consequently that constructor's
body is NOT an independent authority for an unconditional null FirstDeleted.

More importantly, actual `ULevel.Serialize`, export thunk `10301343`, enters
`1039D560..1039D7FF` and **unconditionally archives FirstDeleted**. The relevant
normal prefix is:

| Address | Original operation |
| --- | --- |
| `1039D58F` | Call `ULevelBase.Serialize` through `10302644 -> 1039C5F0` |
| `1039D594..5A8` | Convert native Double time to a Float temporary and archive Model `+98` via archive virtual `+18` |
| `1039D5AB..5B3` | Archive ReachSpecs array `+8C` through `10303A80 -> 103A1320` |
| `1039D5BD..5C5` | Archive the Float time temporary with four raw bytes |
| `1039D5C8..5D3` | Pass address `Level+100` to archive virtual `+18` |
| `1039D5D6..5EA` | Archive sixteen successive UObject-reference slots beginning `Level+9C`; slot1 archives `+100` again |

The `+100` address operation is not guarded by IsLoading/IsSaving or by an
absent reflected property. A map loader must decode the actual object reference
in this native body and bind the corresponding live identity. An unsupported
nonnull queue cannot safely be replaced by None. The later sixteen-reference
loop revisits the SAME native address at zero-based index1: `9C + 1*4 = 100`.
EDI retains Level at `1039D584`, ESI starts at zero at `1039D586`, and
`1039D5DD` selects `[EDI+ESI*4+9C]`. Therefore the second serialized value is
the final loaded FirstDeleted authority. The original does not assert equality
between these two encodings; a loader must validate both and preserve the last
store, including conflicting valid references. It must also read/validate the
remaining fifteen references rather than accept a truncated prefix. A future archive/rebind phase
must represent the queue, rather than reconstructing it from `bDeleteMe` flags.

The ReachSpecs decoder needed to reach that reference is independently tied to
the original implementation, not solely to a third-party format description:

- Array serializer `103A1320..103A145A` archives a compact count and invokes
  `10302266 -> 103A1990..103A1A24` for each record.
- Each record archives I32 distance, UObject reference Start, UObject reference
  End, I32 radius, I32 height, I32 reach flags, U8 pruned, in that exact order.
  Native record size `1C` is not its serialized size because object references
  use archive object-index encoding.

### Exact first-Training native payload

The independently parsed owned map is package version68/licensee0, with3744
names,3347 exports and181 imports. Its sole `MyLevel` is export reference3347,
Class import-179, Outer0, flags00070001, serialized offset4366622 and size3906.

| Native payload observation | Absolute file offset / value |
| --- | --- |
| Tagged-property terminator | `4366622`, decoded Name None |
| Native actor count/capacity | `1337 / 1337` |
| URL strings | `deusex`, empty host, `Index.dx`, empty portal |
| Model reference | `1180`, actual Model36 export |
| ReachSpecs count | `60`; each Start/End reference verified within local export range |
| Serialized Float time | `4370505`, bytes `0D EE 24 43`, approximately164.9299 |
| Preliminary FirstDeleted archive | `4370509`, compact byte `00`, None |
| **Final FirstDeleted archive (loop slot1)** | **`4370511`, compact byte `00`, actual loaded None** |
| Sixteen following references | zero except zero-based slot6 =3145; slot1 aliases FirstDeleted |
| Remaining map count / exact export end | zero at4370527; end4370528 |

Thus a first-Training loader may establish an actually decoded None head from
this source. These exact offsets are audit witnesses, NOT production hardcoded
offsets or permission to skip preceding schema/count/reference validation.

## Actor.Deleted initialization comes from actual property preparation

The independent original Engine.u field-chain inventory records:

```
Actor.Deleted ref=4757 type=ObjectProperty dim=1 flags=00000002 next=4756 subtype=2
```

`subtype2` is the actual Actor class reference in that source inventory. The
native pointer is Actor `+F8`, independently corroborated by destruction and
cleanup instructions below. This is a scalar const Object property, not an
invented reverse ownership vector or the Actor-array hole itself.

Actor's InternalConstructor export `10302DAB -> 10323530..3D` checks the
destination and calls default constructor `10303431 -> 10323550..103235C5`.
That complete default constructor calls UObject's constructor, prepares its
vector/plane member subobjects and installs the Actor vtable. It does NOT
explicitly write Actor `+F8`. Its absence of a store does not prove a null
Deleted link.

Fresh allocation instead obtains payload from real InitProperties before the
selected InternalConstructor. See ORIGINAL-LINKER-ATTACHMENT.md for the actual
StaticAllocateObject/StaticConstructObject sequence. Core `101520D0` chooses
an explicit template or actual class defaults at `10152117..10152146`, copies
the payload beyond the28-hex-byte UObject header at `10152149..10152176`, and
zero-extends only a remaining tail at `10152185..101521A7`. It does not replace
all existing Actor members with zero. Original SpawnActor supplies its retained
template/default argument to StaticConstructObject at `10395211/1039521A`.

For loaded map actors, AActor.Serialize export `1030166D` enters
`10369AD0..10369B12` and calls imported UObject.Serialize at `10369AFC`; it has
no additional Actor-specific native-body field archive. Actual Deleted payload
therefore belongs to the genuine typed property/default/tag preparation. A
portable binding must authenticate the exact declaring Actor.Deleted schema
and its initialized typed value, preserving applicable authored/template data;
neither a missing tag nor the default-constructor body justifies a fabricated
None. The source Actor constructor result is finite evidence, not permission
to bypass an unaudited subclass constructor override.

## Exact deferred insertion and later cleanup boundary

ORIGINAL-DESTROY-ACTOR.md records the entire ordinary DestroyActor coordinator.
After slot removal, late pending-delete, audio and ignored ConditionalDestroy
result, `103960B7..C8` rereads editor state. In ordinary game mode:

1. Read **live** `Level.FirstDeleted` (`+100`).
2. Store that captured pointer into source `Actor.Deleted` (`+F8`).
3. Publish source as `Level.FirstDeleted` (`103960CD..D9`), then return true.

No active-tick, newborn, class, pending-delete or queue-length predicate guards
this insertion. In editor the separate branch invokes Level virtual `+98`
with argument1 instead. No ordinary-game cleanup call belongs inside that
DestroyActor tail. UObject lookup and object indices remain alive at insertion.

Original ULevel's actual vtable at10428AFC has entry+98 at10428B94 =10301811,
which jumps to `CleanupDestroyed` body103965A0. The bounded normal ULevel.Tick
body (`10301104 -> 103A51A0`) reaches `103A5B91..5B99` after restoring its
temporary global state and resetting Level tick fields; it pushes0, sets the
actual Level receiver and calls virtual+98. The normal Tick return immediately
follows at103A5BAF. This is an actual later ordinary cleanup boundary, not a
new callback to add arbitrarily inside native DestroyActor. This audit does
not assert exhaustive identification of every editor/travel cleanup caller.

CleanupDestroyed itself is distinct from immediate ConditionalDestroy:

- At103965CC..5DC, ordinary nonforced cleanup first invokes Level virtual+7C.
  This additional selected service is not established as a no-op here.
- It reads FirstDeleted at103965DF and counts the live Actor.Deleted chain at
  103965F1..10396600. If count is below128 and force is0, it returns without
  deleting the retained queue. A small retained queue is original behavior,
  not evidence of a missing per-frame unconditional scalar destructor.
- At count>=128 or force!=0, it enters actual class-reference cleanup for
  native live actors via each actual UClass virtual+78, then outside editor
  processes FirstDeleted at10396680 onward. It asserts pending-delete, advances
  the head to the retained Actor.Deleted successor (`103966A8..6B4`), performs
  further LevelInfo/AI-related native work, and invokes the actual source's
  scalar-deleting destructor virtual+0C with argument1 (`103967A7..7AD`).
- That is not permission to substitute ConListItem retirement, free an Actor
  index at ConditionalDestroy time, compact every hole, or skip native cleanup
  services. Full selected-service/destructor/hash/index lifetime integration
  remains separate required work.

## Portable ownership constraints

An implementation can faithfully retain the ordinary insertion while later
cleanup remains explicitly unsupported: bind actual loader/construction
storage, journal both pointer writes in the caller root, GC-mark both native
edges, and preserve UObject lookup/index lifetime. Capture, restoration,
map replacement/unload must represent that native state or refuse before
mutating/truncating anything. A newly spawned detached actor can enter the
queue without changing an authored linker export, so linker-change guards
alone do not cover deferred ownership. Whole-runtime shutdown is a separate
owner operation, not implemented per-map CleanupDestroyed.

The source observations above justify no broader map integration. Final
acceptance still requires the entire first Training map to start and play,
actual held-weapon crate damage/destruction, surviving usable contents and
gravity/support/falling, followed by a playable Quest build and visual checks.
