# Original UObject conditional destruction and first Training storage

Read-only primary-source audit,2026-10-21. Prerequisite for the real first
`00_Training` PickupDistributor and destructible crates; not an implementation,
successful startup, surviving usable loot, gravity or headset result. No other
map is loaded. Original commercial DLL/EXE files are never loaded/executed.

Implementation checkpoint: TRAINING-PROPERTY-TEARDOWN.md now records actual
prepared ConstructorLink/selected payload teardown and native ObjectFlags/
RenderInterface storage. The storage-gap discussion below is the audit-time
baseline, not a claim that this subsequent source work is absent. Live linker
detachment and deferred deletion remain unfinished; no complete DestroyActor,
usable loot, gravity or new playable APK is claimed.

Owned originals under `D:/Steam/steamapps/common/Deus Ex/System` retain:

- Core.dll SHA256 `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
- Engine.dll SHA256 `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
- Core.u SHA256 `4FB22EE4E3A6868DFABB2B46B15FAD8E5094F5FDA5927A14FF11670A744EC36C`.
- Engine.u SHA256 `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
- DeusEx.u SHA256 `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480`.

Ignored bounded disassembly and a standalone table-only field census are in
`artifacts/conditional-destroy-audit-20261021/`. LLVM18 from Android NDK27
disassembles exact preferred-VA ranges. These addresses identify the original
PE32 profile, not portable pointer offsets. Export thunks are identified by the
existing original Core/Engine export/import inventories, not nearest objdump
labels. Direct vtable reads map the actual PE section RVA to its raw file range.

## ConditionalDestroy: exact flag mutation and diagnostic

Exported thunk10102270 reaches body1014FDB0..1014FE9F.

1. Read actual object index+4. Exactly-1 returns0 before reading flags.
2. Read ObjectFlags+1C; bit00800000 already set returns0.
3. Write `(oldFlags & 7F7FFFFF) | 00800000` at1014FDF7..1014FE01. This also
   CLEARS the old DebugDestroy80000000 marker. Merely ORing RF_Destroyed would
   incorrectly let a preexisting marker hide a native override missing Super.
4. Invoke selected native virtual Destroy at+24, not UnrealScript Destroyed.
5. Read flags again. Missing DebugDestroy80000000 enters GetFullName/Logf
   diagnostic1014FE12..2D, then joins the ordinary return1 at1014FE30. It is
   neither a native failure result nor an assertion/extra teardown branch.

The log's precise engine error-device behavior is external; this body still
does not change its true return or perform a replacement destruction.
Provider exceptions must remain exceptions owned by the enclosing VM root.

## AActor.Destroy, UObject.Destroy and immediate lookup lifetime

Native AActor.Destroy10301F8C ->10368F60..10368FBC reads auxiliary+314. Nonnull
calls that object's ConditionalDestroy, then clears the pointer; after that it
calls base UObject.Destroy. Actual Engine.Actor.RenderInterface corroborates
the auxiliary declaration: ObjectProperty dimension1, flags00002000,
constraint Engine.RenderIterator. A reflected None or renderer absence alone
is not an authenticated native field binding. A real adapter must authenticate
the declaration/live storage and explicitly refuse unsupported nonnull native
RenderIterator teardown.

Direct original vtable-slot+24 evidence:

- Actor vtable RVA1268DC, Destroy10301F8C.
- Keypoint vtable RVA127020, Destroy10301F8C; this covers PickupDistributor's
  native Engine.Keypoint ancestor, not a universal native-subclass assumption.
- Decoration and DeusExDecoration evidence remains in ORIGINAL-DESTROY-ACTOR.md.

UObject.Destroy101025F4 ->1014FA50..1014FAD3:

1. OR DebugDestroy80000000.
2. Call ExitProperties(object,actualClass).
3. Call SetLinker(None,-1).
4. Read current Outer; cache its actual object index, or0, in object+14.

No object-index lease release, GObjHash unlink or destructor runs at this point.
Those happen later in actual destructor/cleanup. Preserve native identity and
deferred-deletion GC ownership; ConListItem scalar retirement is not a substitute.
See ORIGINAL-OBJECT-HASH-ORDER.md for later cached-key unlink/index ordering.

## ExitProperties is ConstructorLink, not generic field clearing

Thunk10102603 reaches full normal body10152250..101522F7:

- Load actual Class.ConstructorLink+68.
- For each reached property, call its native virtual DestroyValue+94 with
  objectData+property.Offset(+48).
- Read that property's ConstructorLinkNext+54 after the call; continue toNone.

The original UStruct.Link10126060..1012654E establishes this chain in actual
property-iterator order: own Children/Next then Super.Children, filtered by
property flag00400000. Exact linking writes are10126466..1012647A and null
termination101264A9..B3. PropertyLink and ConfigLink are distinct lists; no
qualified-path sorting or nearest-name deduplication is equivalent. See
ORIGINAL-ENGINE-DEFAULT-LINK.md for the complete established link authority.

Selected DestroyValue bodies:

- UProperty101029B4 ->10165590: immediate `ret4`, genuinely no effect for that
  selected base implementation. This is not permission to omit subclass work.
- UStrProperty101031C5 ->10167BA0..10167BC0: for actual ArrayDim(+34) positive,
  invoke FString destructor on each ascending element, stride ElementSize(+38).
  FString destructor10101339 ->101075A0..10107636 frees owning data and ends
  with Data/Num/Max all0. Empty means an empty live string, not fallback to the
  object's immutable package/default text.
- UStructProperty10101212 ->10169CF0..10169D46: first traverse referenced
  Struct.ConstructorLink. For EACH inner field, process all ascending outer
  StructProperty.ArrayDim elements using its ElementSize stride; call that
  inner field's selected DestroyValue at the inner Offset. Then advance the
  inner ConstructorLink. This is field-outer/array-element-inner ordering.
  Nonconstructor struct members remain untouched.
- UArrayProperty10101CD0 ->10168BB0..10168C0A: if actual Inner.flags contains
  00400000, destroy all existing ascending elements using Inner.ElementSize;
  then free outer Data and zero Data/Max/Num regardless of the inner flag.
  This body does not loop a fixed outer ArrayDim. It is distinct from fixed-
  array/map destruction, whose providers remain outside this bounded audit.

These operations destruct owning dynamic payloads, not arbitrary actor scalar
state, class-default maps, Name/Object fields or native animation/timer clocks.

## Actual first-Training native profiles need nested struct teardown

The standalone census reads only Core.u,Engine.u,DeusEx.u using existing
bounded portable package-table/property decoders. It authenticates actual
Children/Next and declaring Outer, then visits actual ancestor classes. Source
bytes are fingerprinted before/after. It initializes no runtime or original
code and loads no map.

PickupDistributor ->Engine.Keypoint ->Engine.Actor ->Core.Object has:

- `DeusEx.PickupDistributor.NanoKeyData`: StructProperty dimension8,
  flags00408001, subtype`DeusEx.PickupDistributor.SNanoKeyInitStruct`.
- That struct's declaration order: ScriptedPawnTag Name, KeyID Name,
  Description StrProperty dimension1 flags00408001, SkinColor Byte. Only
  Description participates in its ConstructorLink. Destruction empties all8
  descriptions and preserves both Names and the Byte.
- Inherited Actor.BindName, BarkBindName, FamiliarName, UnfamiliarName:
  StrProperty dimension1, flags00400001/00400001/00408001/00408001.

CrateBreakableMedGeneral ->Containers ->DeusExDecoration ->Engine.Decoration
->Engine.Actor ->Core.Object adds ItemArticle and ItemName, each StrProperty
dimension1 flags00408000, followed by the Actor four strings:6 string slots.

Thus even the current PickupDistributor boundary cannot be faithfully solved
by a string-only top-level loop or by erasing all script overlays.

## Runtime storage implications, not completed providers

Current RuntimeObject holds sourcePath/exportIndex/instanceProperties as
immutable authored provenance, separate live scriptValues and frozen birth
defaults. Normal classes receive authenticated field membership but no actual
native constructor-link graph. nativePropertyHeads[2]/nativePropertyNext[2]
currently exist only for the closed native Engine default profiles. Their
null value on an ordinary Actor class is NOT proof that ExitProperties is empty.

A real adapter needs the following coherent storage boundaries:

- Prepare/authenticate the exact actual constructor-link graph, recursively
  including Struct constructors and selected native DestroyValue dispatch.
- Journal dynamic instance payload before-image and materialize empty owned
  string/array payloads at exact qualified slots. Removing an overlay would
  resurrect immutable package/frozen-default text; clearing all overlays would
  wrongly alter nonconstructor fields. Do not use generic property writes that
  restart clocks or collision when no such native service occurs.
- Keep source metadata/provenance intact, but model actual live linker export
  attachments separately and journal their slot changes. SetLinker(None,-1)
  is thunk10101631 ->1014FB20..1014FBC0: when old Linker+10 exists, verify its
  ExportMap[index].Object is nonnull and equals this, null that pointer, then
  store new linker/index. Object+14 becomes the later outer-index cache only
  AFTER detaching; source exportIndex is not that mutable storage.
- Preserve UObject lookup/index until actual destructor/cleanup. New Level
  FirstDeleted / Actor.Deleted edges must participate in GC and rollback.
- Synchronize the one ObjectFlags authority and script header reads. A native
  flags-only mutation must not be hidden by stale ObjectFlags overlays.
- Checkpoint/rebind must represent dynamic teardown/linker/deferred ownership
  or explicitly refuse; an old checkpoint format must not report it restored.

These are required real providers, not a justified no-op teardown or a claim
that full DestroyActor, loot, gravity, save/reload or Training is complete.

## UObject.Modify with authentic standalone GUndoNone

Core export Modify101014D8 reaches normal body10150170..101501C2. It loads
exported GUndo101E3318. Only a nonnull pointer AND ObjectFlags lowbit1 invokes
Undo.virtual+0(source). Otherwise the body returns without any field/flag
write. There is no dirty flag, lifecycle flag or native property teardown here.

Actor AND Keypoint vtable+1C contain1040C734, a JMP through Engine IAT10599AE8
to exported UObject.Modify. Therefore established standalone GUndoNone admits
a genuine no-effect Modify provider; absence must come from the actual native
standalone service, not a missing reflected property. Transaction journaling
for later mutations is separate from this original Modify operation.
