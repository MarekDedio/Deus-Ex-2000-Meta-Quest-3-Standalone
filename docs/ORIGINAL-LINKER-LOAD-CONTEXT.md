# Original linker load context: first Training prerequisite

Static-only audit,2026-10-22, for ONLY the owned `Maps/00_Training.dx`.
No commercial DLL/EXE was loaded or executed, no other actual map was read,
and no runtime/compiler input was changed by this audit. This document records
a missing loader prerequisite; it does not implement context filtering or
claim successful player Login, startup, crate loot, gravity or a new APK.

## Source identity and method

The following SHA256 values were checked before and after the static reads:

| Owned source | SHA256 |
| --- | --- |
| `System/Core.dll` | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| `Maps/00_Training.dx` | `BB66D95AAFC6AAE4179F8444E98661F3D5D27DAB06D13D7DDE7F4B5F4E24068D` |

Preferred PE32 addresses below belong to that Core profile, not portable C++
offsets or executable host pointers. The read-only tool was
`C:/ProgramData/mingw64/mingw64/bin/objdump.exe`, with `-d -Mintel` over the
bounded address intervals below and `-s` over the identifying string bytes.
Existing `artifacts/world-native-startup-20261011/core-exports-imports.log`
independently identifies the exported global addresses. The first-map census
used a read-only PowerShell byte reader for its v68 name/export tables and
the actual serialized `MyLevel` Actor reference array; no runtime map load or
current production actor-order helper supplied the census oracle.

## Actual context construction

Core native linker construction at `1011DD90..1011DF15` establishes the
linker's `+A0` context word. The source assertion path at `101A83D4` is
`D:\prj\Clean\Core\Src\UnLinker.h`; its nearby assertions identify the
`LinkerRoot` and `InFilename` constructor inputs. The relevant instructions
are:

| Address | Original effect |
| --- | --- |
| `1011DE86` | Store context `+A0=0` |
| `1011DECA..1011DEDA` | If `GIsEditor` is nonzero, store `0x00040000` |
| `1011DEDD..1011DEED` | If `GIsClient` is nonzero, OR `0x00010000` |
| `1011DEF0..1011DF00` | If `GIsServer` is nonzero, OR `0x00020000` |

The globals are independently exported: `GIsEditor=101E5544`,
`GIsClient=101E5548`, `GIsServer=101E554C`. Thus simultaneous client/server
contexts combine by OR; they do not require an export to satisfy both bits.
This bounded audit does not recover the original launcher's assignment of
every global. A standalone Quest embedding must own and explicitly declare
its selected noneditor context instead of inheriting a desktop/editor guess.

The vendored `third_party/SurrealEngine/SurrealEngine/Package/ObjectFlags.h`
names these low load bits `LoadForClient`, `LoadForServer`, `LoadForEdit`.
The distinct high `NotForClient/Server/Edit` bits are `0x00100000`,
`0x00200000`, `0x00400000`; they are not the mask tested below.

## CreateExport: cached object first, then positive OR admission

The full `ULinkerLoad::CreateExport` body starts at `1012E230`. Its unwind
handler at `1012E44F` passes the independently read UTF16 literal at
`10177B80`, exactly `ULinkerLoad::CreateExport`.

1. `1012E24B..1012E269` selects the requested export record using the source
   index and36-byte in-memory record stride, from linker export array `+84`.
2. `1012E26C..1012E272` reads cached export UObject `+1C`. If already nonnull,
   jump directly to the return at `1012E425`. The context gate is NOT an
   unconditional invalidation of a previously constructed object.
3. For an absent cached object, `1012E278` reads that export's actual
   ObjectFlags at record `+10`; `1012E27B` reads actual linker context `+A0`.
4. `1012E281..1012E283` tests their intersection. A zero intersection jumps to
   `1012E425`, returning the still-null cached object. No class resolution,
   UObject construction or source-export attachment has occurred.
5. A nonzero intersection continues at `1012E289` into the original class,
   outer and construction path. `1012E34C..1012E36A` subsequently masks flags
   with `0x067F0025`, ORs `0x01000200`, and calls the original allocation path.
   Those later operations are not replacements for the earlier admission gate.

Therefore a fresh export with no selected positive load-context bit returns
None. A `NotForServer`-only filter is not the original rule, nor is a camera
name/type blacklist. A zero-flag generated export does not become admissible
merely because a test knows its class. Generated context controls must provide
genuine admitted flags where that explicit loader mode is selected.

This audit establishes this reached CreateExport branch, not the whole
`VerifyImport`, recursive dependency, class-native replacement or linker
creation pipeline. In particular, it does not authorize swallowing malformed
imports or treating every unresolved class as a legitimate context-excluded
None. Those remain distinct results needing source/provider authority.

## First-Training source census

The unchanged map contains3347 source exports. Positive load-context bits are
`0x00040000` for1991 exports and `0x00070000` for1356 exports. The actual
serialized `MyLevel` Actor array has1337 slots:1308 nonnull source references
and29 serialized None references. These are raw data counts, not a claim that
all1308 objects belong in a noneditor runtime world.

| Context mask tested against source ObjectFlags | Admitted nonnull source Actor references | Excluded nonnull source Actor references |
| --- | ---: | ---: |
| Client `0x00010000` |312|996|
| Server `0x00020000` |312|996|
| Client OR Server `0x00030000` |312|996|
| Editor `0x00040000` |1308|0|

The996 excluded first-map Actor references include the editor Brush records
and the following four Camera records. Reserved slot1 itself references the
editor-only `Brush1540` (export1687,ObjectFlags`0x02340001`); it must resolve
to a native null in a fresh noneditor context, not be compacted away during
source decoding. Later original partition retains reserved positions.

| Source Camera | One-based export reference | Zero-based source Actor slot | ObjectFlags | Payload offset/bytes |
| --- | ---: | ---: | --- | --- |
| Camera4 |3343|1331|`0x02340000`|4366144/122|
| Camera5 |3344|1332|`0x02340000`|4366266/102|
| Camera6 |3345|1333|`0x02340000`|4366368/152|
| Camera7 |3346|1334|`0x02340000`|4366520/102|

Each Camera has HasStack`0x02000000`, NotForClient`0x00100000`,
NotForServer`0x00200000`, and LoadForEdit`0x00040000`, with NO positive
client/server load bit. Their fresh CreateExport result is consequently None
for any of the noneditor contexts in the table. This is why admitting editor
Cameras to PawnList/normal startup can corrupt original GameInfo.Login's
player-reuse branch; rejecting their later DeusExPlayer cast is not a fix.

Positive controls are `LevelInfo0` (export1,slot0), `PlayerStart1`
(export1145,slot86), `CrateBreakableMedGeneral1` (export1661,slot910), and
`CrateBreakableMedGeneral2` (export1613,slot1054): all have source flags
`0x02070001` and pass client, server and editor masks. These controls prevent
an indiscriminate camera/Actor removal or accidental inverted mask.

## Current implementation seams and proposed integration, not completed work

The table decoder in `native/surreal_portable_package_tables.cpp` correctly
preserves each raw ObjFlags/reference. `BuildPortableReflectionGraph` is a
metadata inventory and must remain able to inspect context-excluded records.
It must not be mistaken for the cache of actually constructed UObjects.

Current runtime allocation is eager and dense:

- `PopulateRuntime` near1972 and both `InitializePortableRuntime` overloads
  near11409/11433 allocate/index source reflection objects before any original
  context admission. The multi-package slice uses `slice.first+localIndex`.
- `LoadPortableRuntimeMap` near13955 allocates every map reflection object,
  publishes qualified/VM indexes, then resolves source references by path.
  `BindAuthoredActorSlots` near13874 resolves nonzero source references by
  `first+reference-1` and gives every resolved Actor native membership.
- `MakeRuntimeLinker`/`BindRuntimeLinkerExport` near1928 assume actual object
  cache slots align with unchanged source-export positions. Default/object
  reference resolution, object lookup, callbacks, meshes/render actors and
  GC ownership all consume those same currently materialized wrappers.
- `ScriptSaveSchema` near10494 and
  `native/portable_stage_actor_order.inl` currently assume dense retained
  authored export objects. Explicit v12 order, legacy reset, birth append
  indices and cold staged replacement must share the selected loader profile.

A correct next integration must separate immutable source metadata/provenance
from actual admitted cached UObject identities. It needs one explicit
load-context authority, positive OR selection, cached-first behavior and
generation/rollback ownership. Preserve source export indices, raw Level
references/counts and null holes; do not erase/reindex source records. If
metadata wrappers remain for audit, they cannot masquerade as constructed
context-excluded objects in native linker cache, lookup, PawnList, dispatch,
saved references, collision or renderer enumeration.

Typed source references to excluded exports must obtain authentic None where
the original loader returns None, while malformed/unavailable dependencies
remain named refusals. Required BSP Model/native geometry must not be erased
because an editor Brush Actor is absent; that dependency chain needs its own
exact reference/provider treatment. Native actor order and saves must validate
admitted authored membership with the SAME profile on preflight, warm/cold
restore and legacy reset, without rewriting original bytes or spoofing
`worldRemoved`/`bDeleteMe` as context exclusion.

Verification should first cover generated cached/noncached and OR-mask cases,
then refresh the above ONLY-first-Training positive/negative census and actual
startup/Login observations with full save/order/hash/lookup/GC/RNG rollback and
source fingerprints. Earlier all-export1308-Actor diagnostics retain their
historical context; they are not proof of correct noneditor construction.
Shared dependencies needed by this first map remain in scope. Other actual
map integration, world Tick, live combat/loot/gravity and headset acceptance
remain separate unfinished milestones.
