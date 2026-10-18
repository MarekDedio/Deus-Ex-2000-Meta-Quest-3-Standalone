# Collision hash actor fields and persistent property archives

Bounded read-only static audit, 2026-10-18. No original DLL or EXE was loaded
or executed. This supplements [the collision hash contract](ORIGINAL-COLLISION-HASH.md)
and establishes the property filter for two genuine Actor fields. It is not
proof of original savegame compatibility, complete world startup, movement or
playable crate physics.

## Source identities and method

User-owned files under `D:/Steam/steamapps/common/Deus Ex/System` were read as
bytes and disassembled with Android NDK27 LLVM tools. SHA256 values rechecked
after inspection:

| File | SHA256 |
| --- | --- |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Addresses below are preferred PE32 virtual addresses, not portable object
offsets. An export thunk's relative JMP and raw PE vtable words independently
identify each body; objdump's nearest unrelated symbol heading is not evidence
of a body name. Local ignored output is in
`artifacts/collision-hash-save-audit-20261018/`.

The original Engine.u Actor child chain independently yields:

| Qualified field | Export reference | Type | Dimension | Flags | Next |
| --- | --- | --- | --- | --- | --- |
| Engine.Actor.CollisionTag | 4756 | IntProperty | 1 | `0x2002` | 4753 |
| Engine.Actor.ColLocation | 4743 | StructProperty | 1 | `0x0002` | 10 |

ColLocation's referenced import is -7, `Core.Object.Vector`: import -7 is
Struct Vector, Outer -3; -3 is Class Object, Outer -1; -1 is Package Core.
Neither field is an invented portable property. Native hash methods access
these fields at Actor+FC and Actor+138 respectively, as separately audited.

## Exact UProperty filter

`UProperty.ShouldSerializeValue(FArchive&)` is export thunk `10101AE6`, JMP
to body `10112810..10112844`. Its complete decision is:

```text
if (PropertyFlags & 0x1000) return false;
if ((PropertyFlags & 0x2000) && Archive.IsPersistent()) return false;
return true;
```

The first two instructions load property+3C and test AH mask10; the next test
is AH mask20. The conditional archive load is at `1011281D..10112826`.
Thus these are masks1000/2000, not masks10/20. No Const-bit2, Config-bit4000,
loading/saving or default-object test occurs in this helper.

`UProperty.Serialize` thunk `1010147E` reaches `10165020..101650B8` and
archives the scalar property flags at property+3C (`10165063..1016506D`).
This independently ties that word to original metadata rather than guessing
its meaning from the flags on these two fields.

Exported FArchive accessors independently establish the archive slots:

| Accessor | Export thunk | Body | Returned slot |
| --- | --- | --- | --- |
| IsLoading | `10101C62` | `10106BC0..10106BC3` | +0C |
| IsSaving | `101013A2` | `10106BE0..10106BE3` | +10 |
| IsTrans | `10101762` | `10106C00..10106C03` | +14 |
| IsPersistent | `10101253` | `10106C40..10106C43` | +18 |

Consequences are therefore proven conditionally on the actual archive state:
CollisionTag is ineligible when IsPersistent is nonzero. ColLocation passes
this filter even when the archive is persistent. Const bit2 by itself does
not prohibit persistent native property serialization.

## The filter actually governs object and class property serialization

`UObject.Serialize` thunk `1010178F` reaches `10150210..101504C5`.
At `10150421` it selects the object's actual Class. Except for the special
Core.Class metaclass receiver case, when `(IsLoading || IsSaving) && !IsTrans`,
`10150444..10150449` calls actual Class virtual+74 with `(Archive,this,Class)`.
Otherwise `1015049B..1015049F` calls virtual+70 with `(Archive,this)`.
Raw UClass vtable `101758F4` binds +74 to exported
`UStruct.SerializeTaggedProperties` thunk `10102108`, and +70 to exported
`UStruct.SerializeBin` thunk `1010285B`. This is not an assumption that every
FArchive, transaction or collector uses the persistent tagged path.

`UStruct.SerializeBin` body `10126610..10126723` independently inlines the
same masks1000/2000 and archive+18 condition at `1012666F..10126681`. Eligible
properties then visit their fixed array elements and call actual property
virtual+78 SerializeItem at `1012669C`. Nonpersistent binary archives can
therefore include a2000 field; the filter is not an unconditional ban in every
in-memory archive.

`UStruct.SerializeTaggedProperties` body `10126730..10126ED5` requires loading
or saving (`10126754..1012678B`) and separates its loading/saving branches:

- Loading: after ordinary name/type/dimension checks, `10126AD7..10126AED`
  applies the same filter. An ineligible tag's payload is skipped via
  `10126B45..10126BA5`, rather than written through SerializeItem. Special
  legacy type-conversion branches precede this check; this audit does not
  generalize those branches to arbitrary malformed/type-mismatched archives.
- Saving: `10126C55..10126C6E` applies the same filter before array iteration
  and before constructing/emitting the property tag. Eligible values then
  compare against available class-default data through actual virtual+6C
  Identical at `10126CD7`. Equal values skip emission. Missing/insufficient
  defaults supply a null comparison pointer; this audit does not substitute
  a guessed universal bytewise comparison for each property's Identical.
  Unequal values proceed through tag sizing and actual item serialization
  at `10126D87` / `10126E60`.

The optional third UClass argument supplies its default byte array at
class+4D4/+4D8 (`1012678E..101267AA`). `UClass.Serialize` thunk `101011F9`
reaches `101280D0..101283B0`; both its loading call `10128257..10128263`
and saving call `10128334..10128344` invoke +74 on the class's default array
with the actual superclass as comparison defaults. Its neither-loading-nor-
saving branch uses +70 at `10128391..1012839D`. There is no additional blanket
Const-bit2 exclusion in these default-property calls.

For ColLocation, raw UStructProperty vtable `10176344` binds Identical+6C
to `101012C6` and SerializeItem+78 to `10103058`. The latter reaches
`101691B0..101691D0`: preload the actual referenced UStruct, then invoke its
SerializeBin+70 with the archive and supplied data. UFloatProperty's
SerializeItem thunk `10102838` reaches `101667B0..101667C0`, which sends four
data bytes to archive Serialize+4. This shows actual typed struct/item
serialization, not a rendered Vector or a raw Actor-memory dump.

## Narrow portable native-mirror / checkpoint policy

The following is an implementation recommendation, not a claim that the
current port already implements these bridges or reads original savegames:

1. Keep exact declared native mirrors for CollisionTag and ColLocation, with
   actual Actor/ULevel/generation authority. Native hash writes and qualified
   script reads must see one value. Do not leave a second generic overlay that
   can silently shadow the native mirror. An immutable/default read remains a
   separate actual-default operation, not a query into live process storage.
   Restrict ordinary compiled-property writes to the exact Const declarations
   while keeping audited native mutation available through its own journaled
   service. This narrow portable validation policy is not a claim that original
   runtime bytecode assignment independently enforces every compiler Const rule.
2. Journal both mirrors with the SAME root as hash links, global query tag,
   process initialization/tables, RNG and actor effects. This temporary rollback
   journal is not a persistent property archive: restoring transient tags after
   a failed transaction is required, not forbidden by the persistent filter.
3. A port-only persistent actor checkpoint may capture authoritative ColLocation
   under its real qualified property key, after exact scalar Vector schema and
   owner validation. Its flags do not prohibit this. If held in typed native
   storage, the collector must deliberately capture that value even without a
   generic scriptValues overlay, and load must stage/validate/publish back to
   the same storage atomically. Duplicated overlay/native records must refuse.
   Capturing every value rather than original delta suppression remains an
   explicit portable wire-format choice, not byte-compatible retail output.
4. Generic persistent saved CollisionTag actor/default/frozen-birth overlays
   must not become native authority. Rejecting such unsupported portable
   records before publication is a conservative port policy; the original
   well-typed tagged loader skips their bytes rather than necessarily throwing.
   Do not claim the stricter portable rejection is a recovered retail error.
5. Process CollisionTag, initialized flag and permutation tables are not
   persistent actor fields. Do not encode them by overloading CollisionTag,
   ColLocation or another script property. Likewise Level.Hash is not archived
   by the audited Level serializer. A versioned portable engine/replay
   checkpoint, if later required to retain complete deterministic process state,
   would be a separate documented format with symbolic object remapping,
   membership/order and native lifecycle validation, never original savegame
   compatibility inferred from this property filter.

In particular, allowing a ColLocation record does not establish a valid hash
restore protocol. Re-enabling the original hash registers from current live
primitive bounds and overwrites ColLocation on ordinary Add. It does not
reconstruct stale links from an archived ColLocation. Current-map hash
membership, process-table/RNG history and cold versus warm restore ordering
must be specified/tested separately before active hash/save support is claimed.

## Related birth/destruction registration boundary

The following additional static evidence prevents an active native hash from
silently losing born or destroyed actors. It does not complete SpawnActor or
DestroyActor gameplay semantics.

`ULevel.SpawnActor` body `10394E50` publishes the newly allocated Actor in its
actual native Level slot, then initializes Tag, Level/XLevel, tick/role fields,
clears Brush and writes Location/OldLocation/Rotation. At `103953B7..103953CF`
it tests born bCollideActors mask1 and actual Level.Hash+F8 nonnull, then calls
Hash virtual+8 AddActor. This precedes SetOwner `103954E4`, Instigator
`103954EE`, InitExecution `103954F6`, native Spawned `103954FD` and all script
birth callbacks. Registering after script callbacks would change the contract.

`ULevel.DestroyActor` thunk `10302568` reaches body `10395BA0`. After initial
guards and optional active-state probe handling, it clears its own Base via
SetBase(None,true), then (when StandingCount is nonzero) scans live Level slots
and clears actors whose Base is self (`10395E01..10395E51`). Only afterward,
`10395E91..10395EA7` calls actual Hash.RemoveActor if Hash exists and current
bCollideActors mask1 is set. With Hash present it then calls virtual+20
CheckActorNotReferenced at `10395EB6` even when bCollideActors was clear.
Raw FCollisionHash vtable `1042A044` independently maps +8 to Add `10302027`,
+0C to Remove `1030199C`, and +20 to `10301979`, the audited retail no-op.

The Destroyed event is later: exported ENGINE_Destroyed at `1058372C` supplies
its FName for ProcessEvent at `10395ED9`. More touching/ownership callbacks and
early-return checks follow. Native actor slot clearing is at `1039608F` and
the pending-delete bit80 is set only at `10396096..1039609C`. The earlier
optional state probe uses literal native FName index13D; this audit does not
independently establish that index's text spelling and does not label it as
an additional specific event.

The port's existing pinned lifecycle helper sets bDeleteMe before its collision
remove. It cannot simply replace that pinned remove with retail RemoveActor:
retail RemoveActor skips pending-delete actors and would leave their links.
Until the complete original lifecycle transaction is integrated, explicitly
refuse unsupported active-hash lifecycle paths before mutation. A noncollidable
GameInfo birth is not permission to omit hash registration on other births or
to skip later changes of collision state. No full DestroyActor ordering,
touching, network or active-state compatibility is claimed here.

## Reproduction and evidence limits

For example, in PowerShell (these tools inspect files; they never load Core):

```powershell
$dump = 'D:/Android/Sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-objdump.exe'
$core = 'D:/Steam/steamapps/common/Deus Ex/System/Core.dll'
& $dump -d --x86-asm-syntax=intel --start-address=0x10101AE6 --stop-address=0x10101AEB $core
& $dump -d --x86-asm-syntax=intel --start-address=0x10112810 --stop-address=0x10112847 $core
& $dump -d --x86-asm-syntax=intel --start-address=0x10106BC0 --stop-address=0x10106C44 $core
& $dump -d --x86-asm-syntax=intel --start-address=0x10126610 --stop-address=0x10126ED8 $core
& $dump -d --x86-asm-syntax=intel --start-address=0x10150210 --stop-address=0x101504C8 $core
& $dump -d --x86-asm-syntax=intel --start-address=0x101280D0 --stop-address=0x101283B3 $core
```

Ignored evidence retains complete bounded bodies for the property filter,
tagged/binary serialization, UObject/UClass dispatch, property metadata,
Struct/Float item serialization, archive accessors, Spawn hash boundary and
DestroyActor. The pre-existing read-only Engine.u child-chain parser was rerun
to produce `engine-actor-properties.log`; raw PE section mapping supplied
vtable words and import ancestry.

This establishes the exact numeric filter and its real property-archive callers.
It does not audit every FArchive subclass, which archive mode every SaveGame
entry path uses, all legacy type conversions, object-allocation/default-zero
behavior on cold save loading, native hash reconstruction, or complete retail
savegame format. No runtime source, original installation file, APK, save or
headset state was changed by this audit.
