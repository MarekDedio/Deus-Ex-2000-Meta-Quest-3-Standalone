# Indexed original UEnum prerequisite for native Input (2026-10-25)

Only first `00_Training.dx` integration remains in scope. This work binds the
original indexed Enum identity and retained source Names needed by UInput's
constructor. It does not construct/configure InputExt, complete its FExec
binding, finish player startup or make live crate destruction/loot/gravity
playable. No account/character-creation UI or APK is part of this checkpoint.

## Original native registration evidence

Read-only PE inspection of the owned Core.dll, SHA256
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`:

| Item | Original preferred PE32 VA / argument |
| --- | --- |
| UEnum.PrivateStaticClass | 1029D6D0 |
| StaticClass thunk/body | 10101DB1 ->10110900..10110905 returns1029D6D0 |
| Registration initializer/UClass ctor call | 10159AC0 /10159B0B ->10102D9C |
| Size | 40 hex, pushed at10159AFD |
| Super | actual Core.Field101F2B18, pushed at10159AF4 |
| Within | actual Core.Struct101F2618, pushed at10159AEF |
| Supplied flags / native constructor baseline | 0 /12 |
| UObject flags | 04084004 at10159ACC |
| Package/config/name literals | Core101A7EF4, System101A82C8, Enum101B0002 |
| Internal/StaticConstructor callbacks | 10103409 /10101041 |

The class-name literal starts after the native U at101B0000. The established
native UClass constructor ORs12, binds the actual superclass, Within and
Core.Class metaclass. This is a class registration, not permission to allocate
a blank default block or replay native constructors over serialized Enum data.
No commercial DLL or EXE was loaded/executed.

The native Input ctor's already-audited lookup is
`StaticFindObjectChecked(UEnum, AActor::PrivateStaticClass, "EInputKey",0)`.
Its class and Outer are object identities. Engine.Console contains a different
255-entry enum with the same leaf name; ANY_PACKAGE/basename selection is not
equivalent. See ORIGINAL-PLAYER-INPUT-CONSOLE.md and
ORIGINAL-INPUT-KEY-QUERIES.md for the actual query implementation/data proof.

## Implemented production scope

The explicit native metadata domain now contains thirteen records, adding
Core.Enum to the previous twelve without moving existing property-class slots.
Registration's original atomic/idempotent class publication, self-metaclass,
actual indexed superclass/Within edges and serialized import binding apply to
Enums as well. No additional class default or native Input object is created.

Actual Enum source bodies are decoded while their package is already being
loaded, for script bundles, the single-package diagnostic loader and later
map exports. The immutable Names vector preserves original FName handles,
order, duplicates and None. Its UField links also serve existing field-chain
traversal. A separate eight-MiB aggregate source-cache budget admits descriptor,
path and owning vector capacities, with raw decoder scratch admitted against
the remaining budget before allocation. This is not process RSS or the global
FName arena. Map retirement refunds its own descriptor capacities and does not
remove the static Engine.Actor enum.

`ReadPortableRuntimeEnum` returns a detached diagnostic snapshot only for an
actual source export with the registered Core.Enum Class, its real indexed
UStruct-derived Outer and previously cached Names. It refuses missing metadata
preparation, non-Enum/basename substitutions and retired map objects. It does
not load a package, intern a name, manufacture a UObject, begin a world, change
a checkpoint or provide an Input capability. A future internal Input service
must retain the real Enum pointer and use its Names directly, not copy detached
snapshots per key query.

## Verification

Targeted owned metadata integration passes11619 checks/117 refusals, including
159 exact source enums across16 packages, distinct Actor/Console identities,
detached reads, warm restore/GC/map lifetime, a generated Enum appended only
to an isolated first-Training backing copy, map lookup retirement/reload, and
cached reads after deleting only an isolated Engine.u copy. Owned originals
retain their fingerprints. The native config-class regression passes9983/48;
actual first-Training payload teardown passes1164/33 with all39 owned sources
unchanged. These are separate controls, not a combined live physics driver.

Actual object lookup passes577 checks/32 refusals. It executes the genuine
non-exact UEnum-class/Actor-Outer query used by UInput's constructor and finds
the real Actor enum, distinct from the same-named Console enum. The full
source population contains6321 exports plus22 native UClasses/four package
roots. Older fixture expectations omitted StructProperty and assumed restore
could succeed without a real Level/backing source; those fixtures now check
explicit refusal and full preservation. A separate positive control loads
real first Training from its actual owned source root, retains a real native
viewport, performs two complete warm checkpoint restores and verifies static
enum/lookup/session identity. The production restore guard was not weakened.

Strict ARM64 checks pass for the runtime, metadata/lookup/config/teardown and
generated nested-struct tests. Android native libraries link successfully.
The coherent desktop rebuild and shared CTest pass:197 executed/236 registered,
39 explicit owned-data skips, zero failures in58.24s. Normal Training startup
repeat2 still observes220285 instructions/25439 writes; aimed two-Fire repeat2
still observes228837/26047. Both preserve the prior boundary at bound
PlayerPawn.ConsoleCommand/FExec, complete undo and all41 unchanged owned
sources. The weapon diagnostic retains48 observations/two cycles/two returned
notifies/four real traces/two crate HP changes/nine births, including the
10->4->-2/removal, six fragments and surviving Lockpick. No partial player or
world is published. Commands explicitly use the established diagnostic
`--native-work 16777216`, not a raised production limit. The separate original
player/HUD regression also passes8301 controls, retaining its authentic bound
FExec refusal and separate unbound TextLogWindow construction boundary. Every
build/test process is terminal before commit. No new APK was generated.

Ignored local evidence: `artifacts/input-enum-*-20261025.log`. Failed earlier
lookup-fixture runs remain separate from the final577/32 control; a copied
System-only root cannot resolve the real Training map during restore, so the
positive control now explicitly uses its actual owned source root. No restore
provider bypass or invented map was added to production.

These checks do not complete configured native Input registration, dynamic
Alias/key property metadata, inherited defaults/config import, the InputExt
constructor, viewport binding, FExec dispatch or the eventual reset/event
service. Those remain integration work before live player/weapon/world physics.

## Additional next-class registration evidence

Read-only Extension.dll inspection (the same93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C
owned hash already recorded in ORIGINAL-PLAYER-INPUT-CONSOLE.md) confirms
InputExt's own header, not a replacement base Input: initializer1002CF10,
native UClass ctor call1002CF64, PrivateStaticClass100A2E00, sizeFB0,
supplied flags0C and UObject flags04084004. Within is the imported actual
Core.Object (IAT10059004 -> name record10076D32); Super is the imported actual
Engine.Input (IAT10059328 ->100789E2). The name starts at100986F6 after native
X, config literal User10098708, package literal10097C9C; the instance callback
is1001C670 and the inherited static callback comes through IAT1005932C.
This supplies the next native-class registration evidence, not an implemented
class/default/instance or permission to skip its config/constructor behavior.

The fixed-array LoadConfig branch is separately visible at Core101512AE..
1015132B: loop i through actual Property.ArrayDim, use the literal `%s[%i]`
(101AE004) for dimensions greater than1, call GConfig.GetString with1024-unit
destination, and only on success invoke actual Property.ImportText with flags0
and destination base+Offset+ElementSize*i. The single-element branch preserves
the original property-name key. This is why Aliases[0..39] needs genuine fixed
StructProperty metadata/import rather than treating it as one scalar or an
unordered alias dictionary. Complete Struct/Name import semantics still need
their own implementation/evidence; this observation alone does not provide it.
