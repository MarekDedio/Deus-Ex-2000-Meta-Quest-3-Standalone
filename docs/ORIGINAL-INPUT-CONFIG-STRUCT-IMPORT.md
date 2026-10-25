# Native input config: Name/String/Struct import (2026-10-25)

First Training only. This implements import protocols required by the original
Input's forty fixed Alias records; it does not prepare InputExt's native
UClass/UStruct/property/default/instance graph or bind PlayerPawn FExec. Live
crate destruction, usable loot and automatic world gravity remain unfinished.
No account/character UI or new APK is part of this checkpoint.

## Read-only original evidence

Owned Core.dll SHA256 remains
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Only external static disassembly was used; no commercial DLL/EXE was loaded or
executed. VAs below identify the original PE32 profile, not portable layout.

- UNameProperty.ImportText thunk101022C5 ->101674D0..1016755B always calls
  ReadToken with capacity1024/dotted0. Only success constructs FName with
  FNAME_Add1, writes it and returns the token cursor. It does not return a
  string spelling in place of a genuine name identity.
- UStrProperty.ImportText thunk101026DA ->10167980..10167AE0 tests flags bit2.
  When clear, it copies the complete CString and returns the ORIGINAL input
  pointer. When set it ReadTokens with capacity4096/dotted0, copies the parsed
  CString and returns the token cursor; token failure preserves storage.
- UStructProperty.ImportText thunk10101AAA ->101698B0..10169BEB requires an
  immediate `(`, reads a literal field name into64 UTF16 units (at most63),
  requires `=`, and performs FNAME_Find0. It traverses the actual referenced
  Struct's Property children and inherited structs, including duplicate name
  matches rather than stopping at the first match. A matching field requires
  a valid array index, nonzero dimension*element size and its real Port virtual.
  Member ImportText receives flags OR2 and its actual element destination.
- A failed member returns null without erasing earlier writes. Unknown or
  unimportable values use the original delimiter/nesting scanner. Neither
  field names nor separators are generally whitespace-trimmed. Unknown field
  lookup does not intern a name.

Two observable original quirks are preserved, not repaired by a modern parser:
the inner `[` subscript branch at10169937 does not advance past `[` before its
digit scan and therefore emits its bad-`]` warning; the unknown-value quote
branch10169B1E tests the current quote immediately and advances just one unit.
It is not an ordinary quote-aware balanced-value parser. The enclosing
Aliases[0..39] indexing occurs separately in LoadConfig, as audited in
ORIGINAL-INPUT-ENUM-IDENTITY.md, and does not take that broken inner branch.

Port vtable slot98 resolves to UProperty.Port10101DCA ->101655C0..101655EB
for both native NameProperty and StrProperty. It tests nonzero dimension*
element size, category/flags and the original intrinsic name exclusion29B.
The import protocol delegates this operation to actual metadata authority;
it does not substitute a generic true result in production.

## Implemented scope and limits

`quest_native_config_import.h` now provides typed Name and String import.
ReadToken retains1024 as its existing default and admits only bounded buffers
of2..4096 units; the separate4096 string buffer does not loosen Name, Bool or
Class import. Native name creation is an explicit callback to the real arena.

`quest_native_struct_import.h` implements the general complete normal control
flow with callbacks for actual Struct/Property/FName identities, live traversal,
Port, typed member writes and warnings. It creates none of those capabilities.
Every traversal/scanner phase borrows shared work. Portable bounds additionally
refuse input over16384 units, more than128 struct/syntax nesting levels,
8192 field visits or a member cursor beyond admitted CString storage. Provider
exceptions propagate; only the caller's real transaction can undo prefix writes.

The existing production Engine default path now uses the typed flags0 String
import for every configured ServerActors/ServerPackages inner value. It
preserves original full CString/cursor semantics; these are existing genuine
native array properties, not a fabricated Alias graph. Input construction
remains separate and unsupported until its complete metadata/config authority
is prepared.

## Verification

Commercial-free controls pass96 checks/26 refusals. The existing typed-config
import regression remains20348/10. The owned data check passes1070 controls,
131 complete enums,510 key queries and40 actual User.ini Alias imports through
an explicitly audited SCHEMA FIXTURE with real FName storage. It is not
registered UProperty/UStruct or actual native Input initialization. All original
data retain their size, mtime and hash.

Coherent desktop build/shared CTest pass:198 executed/237 registered,39 explicit
owned-data skips, zero failures in70.13s. Strict ARM64 helper/owned/previous
config-test checks pass; Android native libraries link successfully. The actual
owned first-Training Engine collision/config/default/field/lookup regression
passes15156 checks/31 refusals, including227 shared Cylinder selections,
ordered config, warm/cold save and loader/GC lifetime. Normal startup repeat2
retains220285 instructions/25439 writes; aimed two-Fire repeat2 retains228837/
26047 and48 observations/two cycles/two returned notifies/four real traces/two
crate HP changes/nine births. The lower crate still reaches10->4->-2/removal,
six fragments and a surviving Lockpick. Both retain the named bound Input
FExec refusal, full save/state/revision/hash/RNG/client/lookup/Actor/GC undo and
all41 unchanged originals. The established explicit diagnostic native-work
option16777216 is unchanged, not a raised production limit. No other campaign
map was loaded; no partial startup was published.

All build/test handles are terminal before commit. Ignored local evidence:
`artifacts/input-config-*-20261025.log`. These separate controls do not prove
initialized InputExt or live crate physics; no APK was generated.
