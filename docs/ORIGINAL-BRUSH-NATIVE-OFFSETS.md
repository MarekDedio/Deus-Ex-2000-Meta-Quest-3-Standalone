# Original Brush property names and native offsets

Read-only static audit, 2026-10-16, of the user-owned Deus Ex GOTY files in
`D:/Steam/steamapps/common/Deus Ex/System`. No original DLL or EXE was loaded
or executed, and no original file was changed. The native offsets below are
evidence locations in the original PE32 program, not portable object offsets.

| Original file | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

## Result and scope of proof

For this original build, **native Brush+32C is Engine.Brush.MainScale;
Brush+340 is Engine.Brush.PostScale; Brush+354 is TempScale**. The original
Brush.ToWorld applies PostScale first, then Rotation, then MainScale. This is
not a conclusion from a reference UE header, the usual meaning of the names,
renderer scales, or an assumed C++ member order. It follows jointly from the
actual package's linked field order, the original Core property-link rules,
the actual registered Actor superclass size, and the original exported Brush
copy constructor's complete native field-copy sequence.

There is no direct name-to-offset registration literal for MainScale or
PostScale in Engine.dll: neither ASCII nor UTF16 name occurs in that DLL.
The serialized UProperty records also do **not** contain a native Offset.
Consequently the name-to-offset result is a layout derivation from the
following original evidence, not a purported discovery of such a literal.

## Actual Engine.u field identities and order

Engine.Brush is Class export reference816, base Engine.Actor, outer None,
payload file offset `0x1A19A`, length177. Its actual Children reference is3159.
Following each field's actual NextField, without sorting export indices or
names, gives:

| Reference | Fully qualified field | Declared property | Flags | Referenced type | Next | Payload file offset / length |
| --- | --- | --- | --- | --- | --- | --- |
| 3159 | Engine.Brush.CsgOper | ByteProperty | `1` | Engine.Brush.ECsgOper | 3158 | `311A7` /16 |
| 3158 | Engine.Brush.UnusedLightMesh | ObjectProperty | `2` | Core.Object | 3155 | `31199` /14 |
| 3155 | Engine.Brush.PostPivot | StructProperty | `0` | Core.Object.Vector | 3153 | `31170` /14 |
| 3153 | Engine.Brush.MainScale | StructProperty | `1` | Core.Object.Scale | 3150 | `31154` /15 |
| 3150 | Engine.Brush.PostScale | StructProperty | `1` | Core.Object.Scale | 3042 | `3112C` /15 |
| 3042 | Engine.Brush.TempScale | StructProperty | `0` | Core.Object.Scale | 3135 | `2F786` /14 |
| 3135 | Engine.Brush.BrushColor | StructProperty | `1` | Core.Object.Color | 3040 | `30F2D` /15 |
| 3040 | Engine.Brush.PolyFlags | IntProperty | `1` | none | 3038 | `2F747` /14 |
| 3038 | Engine.Brush.bColored | BoolProperty | `1` | none | 2923 | `2F714` /14 |
| 2923 | Engine.Brush.ECsgOper | Enum, not a property | n/a | n/a | 0 | `27F5F` /14 |

All nine properties have ArrayDim1 and actual Outer Engine.Brush. MainScale
and PostScale have Category Brush. Their15-byte payloads are fully consumed
by tagged-default terminator, UField links, ArrayDim, PropertyFlags,
Category and referenced Struct; there is no remaining serialized offset.
TempScale uses the same actual Core.Object.Scale type, not another scale
struct whose native size could change this sequence.

Core.Object.Scale is Core.u Struct export reference611, payload `406D`.
Its linked children are Enum reference49, then Scale330, SheerRate331,
SheerAxis327. The Enum contributes no property bytes. Actual properties are:

| Field | Declared type | Dim | Flags | Referenced type | Payload / length |
| --- | --- | --- | --- | --- | --- |
| Core.Object.Scale.Scale | StructProperty | 1 | `4001` | Core.Object.Vector | `27FC` /14 |
| Core.Object.Scale.SheerRate | FloatProperty | 1 | `4001` | none | `280A` /13 |
| Core.Object.Scale.SheerAxis | ByteProperty | 1 | `4001` | Core.Object.Scale.ESheerAxis | `27C2` /13 |

This produces Vector at0, Float atC, Byte at10, rounded struct size14 hex
(20 bytes). The independent original native copies below copy exactly five
DWORDs for each of the three Scale members.

## Actual native superclass and instance-size registration

Engine IAT `10599DE0` imports
`UClass::UClass(ENativeConstructor, unsigned long, unsigned long, UClass*,
UClass*, FGuid, wchar_t const*, wchar_t const*, wchar_t const*, unsigned long,
void(*)(void*), void(UObject::*)())`.

* The Actor initializer `1035ED30..1035ED96` pushes instance size `0x318`
  at `1035ED70`, sets this to actual exported AActor.PrivateStaticClass
  `1057CDF0` at `1035ED79`, and calls that constructor at `1035ED81`.
* The Brush initializer `1037A1B0..1037A216` passes that actual Actor UClass
  at `1037A1E7`, pushes instance size `0x374` at `1037A1F0`, sets this to
  actual ABrush.PrivateStaticClass `10582A28` at `1037A1F9`, and calls the
  same constructor at `1037A201`. It passes original ABrush's internal
  constructor thunk `10302284` at `1037A1BB`.
* Core's named native UClass constructor thunk `10102D9C` targets
  `10128630`. It forwards the supplied native size through its native
  UStruct constructor call `1012865C`, thunk `1010126C` to `10125EF0`.
  The latter stores that size into UStruct.PropertiesSize+3C at
  `10125F47`; superclass is stored at+28 at `10125F1A`.
* Actual UStruct.GetPropertiesSize, thunk `101026E4` to `1010F7C0`, returns
  the value at+3C. This is not a guessed field size from a copy's last write.

## Original Core field linking

UStruct.Link is thunk `10101933` to `10126060`. On its relayout branch,
`1012609D..101260CC` initializes the cursor from the actual superclass's
GetPropertiesSize, rounded to4. It then follows actual Children+38 and
NextField+2C, checks each child's Outer is this struct and that it is a
UProperty, and calls that property's Link. After each property the cursor
becomes `Offset+ElementSize*ArrayDim` at `10126102..10126110`. Final struct
size is rounded to4 at `10126118..10126123`; an Enum does not advance it.

Original property Link implementations supply the necessary sizes/alignment:

* UByteProperty.Link `10102E6E` to `101656F0`: ElementSize1 at `10165720`,
  Offset=current owner's GetPropertiesSize at `10165729..1016572F`.
* UObjectProperty.Link `10102EFA` to `10166A90`: ElementSize4 at `10166AC0`,
  Offset=(owner cursor+3)&~3 at `10166AC9..10166AD5`.
* UIntProperty.Link `10102BA3` to `10165C60`: ElementSize4, same4 alignment
  at `10165C90..10165CA5`.
* UStructProperty.Link `1010223E` to `10168FF0`: preloads its actual Struct,
  copies Struct.PropertiesSize as ElementSize at `10169029..1016902F`,
  selects alignment1/2/4 from its size at `10169032..10169044`, and stores
  aligned owner cursor as Offset at `10169045..10169056`.
* UBoolProperty.Link `10101230` to `10166060` packs only an eligible previous
  Bool; the preceding PolyFlags is Int, so bColored takes the next aligned
  DWORD and bit1 (`101660E4..10166105`).

UStruct.Link also has a no-relayout branch at `1012613C` which preserves
previous property offsets around Link. That branch is not independent
name-to-native-offset evidence, and this audit does not present it as such.
The positive evidence is the full authored layout calculation together with
the independently named native copy below, including both base and final
registered sizes and all surrounding property types.

## Independent exported native copy corroboration

The actual export `ABrush::ABrush(ABrush const&)`, thunk `10303535`, targets
`1032FEB0..1032FF6A`. After calling its Actor copy at `1032FEBB`, it copies
this complete original Brush field sequence:

| Derived named field | Actual native offset | Original copy instructions |
| --- | --- | --- |
| CsgOper | `318` | BYTE `1032FEC0..1032FECC` |
| UnusedLightMesh | `31C` | DWORD `1032FED2..1032FED8` |
| PostPivot | `320` | three DWORDs `1032FEDE..1032FF02` |
| MainScale | `32C` | five DWORDs `1032FEE6..1032FF05` |
| PostScale | `340` | five DWORDs `1032FF07..1032FF18` |
| TempScale | `354` | five DWORDs `1032FF1A..1032FF2B` |
| BrushColor | `368` | DWORD `1032FF2D..1032FF34` |
| PolyFlags | `36C` | DWORD `1032FF3A..1032FF46` |
| bColored | `370`, bit1 | masked DWORD `1032FF40..1032FF63` |

It explicitly installs ABrush's actual vtable `10426C9C` at `1032FF55`.
The sequence occupies the registered Brush size374, starting at registered
Actor size318. There is no unaccounted inserted member, exchanged Scale
slot, or omitted TempScale needed to make it agree with actual metadata.

ABrush.InitPosRotScale, export thunk `10301E79` to `10379FC0`, independently
copies GMath.UnitScale (GMath+48) to+32C at `1037A00F..1037A027` and+340 at
`1037A02E..1037A046`, each five DWORDs. This supports the FScale block sizes
but is not, by itself, authority for their property names.

## ToWorld use and portable bridge boundary

Actual ABrush.ToWorld `103023AB` to `1030F310` applies block+340 through
Core FCoords*=FScale at `1030F38F..1030F39D`, then actual Rotation, then
block+32C's inlined FScale operations. Therefore the detached builder's
`scale340` input must be qualified Engine.Brush.PostScale, and `scale32c`
must be qualified Engine.Brush.MainScale. TempScale is not substituted;
DrawScale is not read by this original ToWorld body.

The scalar and arithmetic schedule is recorded in
`ORIGINAL-BRUSH-COORDINATES.md`. A live portable provider must still verify
actual Actor/Brush native dispatch identity and the qualified reflected
properties' ownership, types, dimensions and Core.Object.Scale schema;
it must not reinterpret portable object memory at these PE32 offsets or
search by an unqualified suffix. This audit supplies the missing original
name/offset authority. It does not claim a live provider, world startup,
Move/FindBase/falling, an APK build, or headset acceptance was completed.
