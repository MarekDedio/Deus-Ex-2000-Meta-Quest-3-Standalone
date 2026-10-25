# Original DeusEx.DeusExLevelInfo Tick prerequisites

Read-only owned GOTY audit, 2026-10-25. This authenticates the next native
boundary reached by the first Training dynamic phase after exact Trigger
admission. It does not admit this class, skip an actor, complete a frame,
or establish a tested APK. No compiler input was edited or build started;
the current global compiler-source freeze remains in force.

Only external objdump/static bytes were used for native code, plus a
read-only BinaryReader for the owned package tables/Class links. No commercial
DLL/EXE was loaded or executed. Preferred-image addresses are not portable
layout. Owned source SHA256 hashes:

| Input | SHA256 |
| --- | --- |
|DeusEx.dll|B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04|
|DeusEx.u|257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480|
|Engine.dll|0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4|
|Engine.u|1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA|

## Actual authored native identity, not the name LevelInfo

Owned DeusEx.u v68 contains the actual Class export
`DeusEx.DeusExLevelInfo`, zero-based export70 / original reference71:

```
ObjClass=0, ObjOuter=0, ObjFlags=040F0004
ObjBase=-520
payload126 bytes at302011
UField base=-520, next=0, source=20333, children=14154
friendly name=DeusExLevelInfo
```

RF_Native04000000 is set on that exact Class export, so the current
nearest-native-class gate correctly stops there. This is not an inherited
ClassFlags inference. Import-520 is actual Core.Class `Info`, outer-2;
import-2 is Core.Package `Engine`. The actual direct superclass is therefore
**Engine.Info**, not Engine.LevelInfo.

Owned Engine.u independently gives native Info reference91, ObjClass0,
ObjOuter0, ObjFlags040F0004, ObjBase2, payload91 bytes at85749. Its Class
links are base2, next0, source5435, children0. Actor reference2 has
ObjFlags040F0004, ObjBase-3, payload1236 bytes at61225; payload base-3.
Thus the relevant actual native chain is:

```
DeusEx.DeusExLevelInfo -> Engine.Info -> Engine.Actor -> Core.Object
```

For comparison only, actual Engine.LevelInfo is separate reference3,
ObjBase/payload base16, payload148 bytes at62461. This audit does not borrow
that class's identity, responsibilities, methods or vtable from its name.

## Exported vtable and constructor corroboration

The owned DeusEx.dll name/EAT table (zero-based indexes, not ordinals) gives:

| Name index | Exact export | RVA / preferred address |
| --- | --- | --- |
|99|`??_7ADeusExLevelInfo@@6B@`|276B4 /100276B4|
|7|`??0ADeusExLevelInfo@@QAE@XZ`|13380 /10013380|
|174|`?InternalConstructor@ADeusExLevelInfo@@SAXPAX@Z`|9E40 /10009E40|
|195|`?PrivateStaticClass@ADeusExLevelInfo@@0VUClass@@A`|3A0D8 /1003A0D8|

The default constructor10013380..100133E7 installs literal100276B4 into
the actor vptr at100133D4. InternalConstructor10009E40 tests its object
argument for null and sends a nonnull object to10013380 at10009E48.
These are actual constructor/export identities, not a vtable guessed from
the field contents.

The default constructor first calls IAT10027290 at1001339D, which the actual
Engine import table identifies as `AActor::AActor()`. It temporarily installs
the imported AInfo vtable from IAT100272A4 at100133A3..100133A8, before
constructing its additional members and installing its own vtable. That
IAT entry imports Engine's `??_7AInfo@@6B@`, independently consistent with
the owned Class ancestry.

## All four requested services resolve to AActor through real IAT thunks

Static reads of the actual100276B4 vtable give:

| Service | Actual vtable cell | DWORD thunk | Thunk instruction | Imported Engine AActor symbol |
| --- | --- | --- | --- | --- |
|ProcessEvent+10|100276C4|1001AF98|`JMP [10027268]`|`?ProcessEvent@AActor@@UAEXPAVUFunction@@PAX1@Z`|
|ProcessState+14|100276C8|1001AF92|`JMP [10027264]`|`?ProcessState@AActor@@UAEXM@Z`|
|Tick+64|10027718|1001AF50|`JMP [10027238]`|`?Tick@AActor@@UAEHMW4ELevelTick@@@Z`|
|performPhysics+8C|10027740|1001AFAA|`JMP [10027294]`|`?performPhysics@AActor@@UAEXM@Z`|

The raw thunk bytes were disassembled independently. These are unbound
import thunks, not local ADeusExLevelInfo override bodies. The import table's
actual DLL name is Engine.dll and first IAT address100271E8. Independent
reads of the unbound IAT words and their Hint/Name strings give:

| IAT cell | Raw Hint/Name RVA | Hint/name identity |
| --- | --- | --- |
|10027238|0002CDBA|hint1667, AActor.Tick|
|10027264|0002CFA6|hint1443, AActor.ProcessState|
|10027268|0002CFC6|hint1441, AActor.ProcessEvent|
|10027294|0002D122|hint2056, AActor.performPhysics|

Engine's independently inspected names/EAT and direct JMP thunks resolve
those symbols to the already audited actual AActor bodies:

| Engine name index | RVA / thunk | Body |
| --- | --- | --- |
|1441 ProcessEvent|2D74 /10302D74|10369100|
|1443 ProcessState|1F00 /10301F00|103E5B60|
|1667 Tick|3DAA /10303DAA|103A1AA0|
|2056 performPhysics|3242 /10303242|103C9770|

Actual AInfo itself has exported vtable Engine name647, RVA1272D0 /
104272D0. Its four corresponding entries are the same Engine AActor
thunks10302D74,10301F00,10303DAA,10303242. This is corroboration, not a
replacement for resolving the actual DeusExLevelInfo vtable's IAT entries.

Therefore no distinct native DeusExLevelInfo override exists for any of the
four requested services in these owned binaries. Actual selected script
Tick/state/events inside those bodies still belong to this real actor and
must not be omitted or replaced by a successful empty provider.

## Bounded consequence and remaining integration

The observed error
`Original Actor Tick reached an unimplemented native virtual override: DeusEx.DeusExLevelInfo`
identifies the current allowlist gap; the wording does not establish that
the class actually overrides native Tick. The static evidence above supports
adding **exact DeusEx.DeusExLevelInfo** to the nearest-RF_Native boundary gate
in a separately thawed implementation increment.

It does NOT authorize every Engine.Info/Engine.LevelInfo-derived native
class, arbitrary classes ending LevelInfo, a different nearer native child,
native teardown or any unrelated virtual slots. It does not authorize
account/login UI or infer world startup was already complete. Other native
boundaries and reached unsupported physics/callback services must continue
to refuse atomically in the same production frame root.

A future implementation needs actual serialized ancestry/selected-script
fixtures, unknown-native refusal, whole-root late/write/work failure checks,
and another unfiltered first Training observation. The current audit changes
only this new document. Full dynamic phase completion, active mover success,
crate/loot/gravity gameplay, original-map visual acceptance and device testing
remain separate evidence, not implied by vtable equivalence.
