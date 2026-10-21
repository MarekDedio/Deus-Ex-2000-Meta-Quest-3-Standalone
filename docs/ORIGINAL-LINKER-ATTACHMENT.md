# Original UObject linker attachment and fresh allocation (2026-10-21)

Read-only static prerequisite for the first `00_Training.dx` destruction path.
No map or runtime was loaded by this audit. No original commercial DLL/EXE was
loaded or executed. Preferred PE32 virtual addresses and original offsets below
identify the owned binary profile; they are not portable C++ layouts/pointers.

Owned `D:/Steam/steamapps/common/Deus Ex/System/Core.dll` SHA256 before and after
inspection remained
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
The external MinGW `objdump.exe` parsed bounded static disassembly and the PE
export table. No exported original function was resolved or invoked.

## Fresh allocation owns the absent binding

The PE export name table identifies:

| Actual export | Export-table index | Thunk -> body |
| --- | ---: | --- |
| `UObject::UObject()` | 178 | `101021C6 -> 1014F340` |
| `UObject::UObject(EInPlaceConstructor,...)` | 176 | `1010295A -> 1014F420` |
| `UObject::UObject(ENativeConstructor,...)` | 177 | `1010339B -> 1014F4C0` |
| `UObject::StaticAllocateObject(...)` | 1252 | `10102EC3 -> 101579C0` |
| `UObject::StaticConstructObject(...)` | 1287 | `10101771 -> 10157FF0` |
| `UObject::SetLinker(ULinkerLoad*,int)` | 1234 | `10101631 -> 1014FB20` |

The default constructor's entire body, `1014F340..1014F348`, only installs
the UObject vtable and returns `this`. It does NOT initialize Linker or
LinkerIndex. The in-place constructor calls StaticAllocateObject at `1014F469`.
Consequently a vtable assignment or an absent reflected tag is not an authority
for a fresh null binding.

StaticAllocateObject performs class/Outer/name lookup before allocation. On
the missing-object path, `10157CB3..10157CE2` uses the supplied destination or
allocates the actual class size, then joins the common header preparation at
`10157E9E`. The following stores are direct, before InitProperties/AddObject:

- `10157EB3` produces `FFFFFFFF`; `10157EB6` stores Object.Index+4 = -1.
- `10157EBB` stores LinkerIndex+14 = -1 from that same register.
- `10157EB9` produces zero; `10157EC7` stores Linker+10 = null.
- The adjacent stores separately establish HashNext+8, StateFrame+0C, Outer+18,
  ObjectFlags+1C, Name+20 and actual Class+24. No serialized export ordinal is
  substituted for the new object's absent linker index.

`10157EF1` calls InitProperties through `1010102D -> 101520D0`.
Its copy/extension preparation starts after the 28-hex-byte UObject header:
`1015211A` sets the baseline to28; `1015214C..10152176` copies source+28 to
destination+28; `10152185..101521A7` zeroes only the remaining tail. Thus the
class-default/template payload copy does not overwrite the Linker+10 or
LinkerIndex+14 initialization. AddObject follows at `10157EFF`.

StaticConstructObject calls this allocator at `1015804D`, then, on a nonnull
result, the actual class InternalConstructor+4E0 at `1015805C`. This is the
fresh-allocation baseline, not blanket permission to ignore an unaudited
subclass constructor that changes native state.

The ENative registration constructor is a distinct path. It initially writes
Index/LinkerIndex=-1 at `1014F4E6/1014F4E9` and Linker=null at `1014F50C`, but
later `1014F54D` reuses +14 for the pending registration chain. Its intermediate
minus1 value must NOT be generalized into the final state of every native
registered class. Runtime births must be tied to their actual fresh allocation
contract, not copied from the registration path or inferred from missing tags.

## CreateExport publishes a source-local, zero-based attachment

The internal CreateExport entry is `101012FD -> 1012E230`, also reached by the
positive export branch of IndexToObject. Its first argument is already a
zero-based source export index:

- `1012E24B` loads the argument; `1012E252` computes argument*9;
  `1012E256` reads the linker's ExportMap data pointer at +84; and
  `1012E269` selects data + argument*36 (24 hex).
- The selected record's live Object pointer is at +1C. `1012E26C..1012E272`
  returns the already populated pointer without constructing another object.
- The missing-object path resolves nonzero serialized ObjOuter through
  IndexToObject, while zero ObjOuter selects actual LinkerRoot+28 at
  `1012E33E`. This is separate from the actual Linker pointer itself.
- `1012E36A` calls StaticConstructObject. `1012E375` first writes the returned
  object to the retained ExportMap record's Object pointer.
- `1012E36F` reloads the original argument; `1012E378/1012E379` push that exact
  index and the actual linker `this`; `1012E37C` calls SetLinker on the returned
  object. No +1 is applied to this native LinkerIndex.

The distinction from serialized object references is independently visible in
IndexToObject (`10102C11 -> 1012E550`): its positive-reference branch subtracts1
at `1012E581`, checks that zero-based index against ExportMap count+88, then
passes it to CreateExport at `1012E5BE`. Therefore source-local export ordinal,
positive serialized reference, combined portable export-vector position and
global UObject index are four distinct values.

## Detachment and the later reused cache

SetLinker `1014FB20..1014FBC0` first reads the actual old Linker+10. For a
nonnull old binding, it reads LinkerIndex+14 and retains the exact old
ExportMap[index].Object slot address at `1014FB54..1014FB60`. It reads/asserts
nonnull at `1014FB64..1014FB80`, reads/asserts equality with this object at
`1014FB83..1014FB9B`, and clears that retained slot at `1014FB9E`. It then
stores the supplied new linker at `1014FBAA` and new index at `1014FBB0`.
This method does not populate a new ExportMap slot; CreateExport did that
separately before calling it.

Base UObject.Destroy calls SetLinker(null,-1) at `1014FA97..1014FA9B` AFTER
ExitProperties. Only afterwards does `1014FAA0..1014FAAA` read the current
Outer and overwrite the same +14 with its actual UObject index, or write0 at
`1014FAC3` for a null Outer. The original destructor later uses this cached
index for hash removal; see ORIGINAL-OBJECT-HASH-ORDER.md. ConditionalDestroy
does not itself release the object's UObject index or erase hash membership.

Portable ownership therefore needs a real stable loader-owned export attachment
and independently mutable linker/index storage. SourcePath/exportIndex remain
immutable provenance. A Package LinkerRoot is not a Linker identity; a null
binding must come from explicit fresh-construction ownership, not an absent
tag. Rollback must restore both changed export slots and object native fields.

This audit establishes those authorities only. It does not implement portable
linker state, deferred actor deletion, complete startup, usable crate loot,
gravity, archive support or headset acceptance. Other maps are outside scope.

## Reproduction (static tools only)

```powershell
$auditCore = 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
$auditObjdump = 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe'
Get-FileHash -Algorithm SHA256 -LiteralPath $auditCore
& $auditObjdump -p $auditCore
& $auditObjdump -d -Mintel --start-address=0x1014f340 --stop-address=0x1014f349 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10157c51 --stop-address=0x10157f86 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10157ff0 --stop-address=0x10158078 $auditCore
& $auditObjdump -d -Mintel --start-address=0x10152117 --stop-address=0x101521a9 $auditCore
& $auditObjdump -d -Mintel --start-address=0x1012e230 --stop-address=0x1012e428 $auditCore
& $auditObjdump -d -Mintel --start-address=0x1012e550 --stop-address=0x1012e5d6 $auditCore
& $auditObjdump -d -Mintel --start-address=0x1014fb20 --stop-address=0x1014fbc3 $auditCore
& $auditObjdump -d -Mintel --start-address=0x1014fa50 --stop-address=0x1014fad4 $auditCore
Get-FileHash -Algorithm SHA256 -LiteralPath $auditCore
```
