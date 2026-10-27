# Next original Input field/default phase (2026-10-27)

First Training only. This is an implementation contract from read-only owned
data/disassembly, not a claim that native Input fields/defaults already exist.
The preceding two-UClass registration phase is separate. No account or
character-creation UI is needed. Live VR weapons, crate destruction, collectible
loot and automatic gravity still require genuine startup/action/frame wiring.

## Original authority and complete schema

Engine.dll103930C0..10393344 is StaticInitInput, NOT the empty class static
constructor. The owned hashes remain those in ORIGINAL-PLAYER-INPUT-CONSOLE.md.
Only static PE/package inspection is permitted; never execute/load those DLLs.
All offsets/sizes here are original PE32 logical layout, not host ABI addresses.

| Object | Actual Class / Outer | Dimension, element size, offset | Final property flags / category |
| --- | --- | --- | --- |
| Engine.Input.Alias | Core.Struct / actual Input Class | native allocation6C, Super null, logical size10hex | UObject flags0; not a Property |
| Alias.Alias | Core.NameProperty / Alias Struct | 1,4,0 | 4000 / None |
| Alias.Command | Core.StrProperty / Alias Struct | 1,0C,4 | 404000 / None |
| Engine.Input.Aliases | Core.StructProperty / Input Class | 40 decimal,10hex,2C | 404000 / Aliases |
| Each raw-key field | Core.StrProperty / Input Class | 1,0C,2AC+0C*i | 404000 / RawKeys |

All258 Property objects request UObject flags4. Alias UStruct requests flags0
(Engine1039311A), unlike the property allocations1039315C/1AE/1FA/288.
StructProperty must retain an actual referenced-Struct pointer to Alias.

Actual Engine.Actor.EInputKey export2290 has255 unique names, no NAME_None,
first IK_None and last IK_OEMClear. StaticInitInput tests source FName identity
against NAME_None at1039327E, NOT index0 or spelling IK_None. It therefore
constructs255 raw fields; combined schema is one Struct plus258 Properties.
Do not silently omit key0. Its suffix None becomes NAME_None; native allocation
then calls MakeUniqueObjectName (Core10157BB9..10157BD6). Reuse actual class
unique-name counters and Outer collision checks, not a guessed StrProperty0.

Creation order is not final list order. Alias creates Alias then Command;
Children/PropertyLink/ConfigLink become Command->Alias, ConstructorLink only
Command. Input creates Aliases then keys ascending0..254; Children/PropertyLink/
ConfigLink become key254->...->key0->Aliases, then eligible inherited fields.
All own Input properties are constructor-linked. InputExt adds no own fields
and reuses inherited identities. Alias itself is not inserted into Input.Children.

Link0 preserves declared offsets. StrProperty.Link adds Constructor400000;
StructProperty.Link adds it because Alias.ConstructorLink is nonempty
(Core10168FF0..10169080). NameProperty retains4000. Do not infer linked flags
from the initializer's4000 argument alone.

## Five additional real pre-config default blocks

Require the actual Engine collision/default domain and its prepared Field,
Property and Subsystem defaults, registered Input classes, actual Actor Enum
and object lookup. Reuse all existing Class identities. In particular,
EngineFieldClasses already registers Core.NameProperty; it has no prepared CDO.

1. Core.Struct6C: prepared Field defaults plus zero extension.
2. Core.NameProperty5C: exact same-size Property default copy.
3. Core.StructProperty64: Property copy plus zero extension.
4. Engine.InputFB0: Subsystem copy plus zero extension; typed40 Alias records
   and255 empty binding strings before config.
5. Extension.InputExtFB0: actual parent Input default copy, including deep
   copies of constructor-linked strings, not shared mutable Alias/binding data.

Property DEFAULT ArrayDim stays0. Actual property-instance construction sets1.
Do not invoke instance constructors over defaults. Input default Enum/Viewport
remain null: StaticInitInput uses a local Enum for declarations, not default.Enum.
The schema domain must retain that real Enum separately as authority.

Core101520D0/10152300 establish parent-copy/zero-extension;101521B1..10152200
deep-copy constructor-linked fields. Native Struct instance constructor
10125F60..10125FCD supplies Name->FriendlyName and null Super; Input then sets
logical size16 decimal. No fabricated serialized StructDescriptor is warranted.
NameProperty's independently verified tuple/default/Link evidence is also in
ORIGINAL-ENGINE-DEFAULT-LINK.md; reuse its collision-domain Class identity.

## Production integration seam

Use a separate authenticated RuntimeNativeInputDomain owning the schema and
five defaults. Keep collision's closed11-default/4-field contract unchanged.
Represent Alias as a genuine native-UStruct payload with logical size,
FriendlyName, Children and linked heads. Add a referenced-Struct pointer to
RuntimeNativeProperty; its existing inner is ArrayProperty.Inner, not this edge.
Use real NameString Alias identities and typed fixed-size Alias/binding storage,
not pointer arithmetic over original32-bit offsets or an unordered alias map.

Extend default validation for exact owning-domain members with actual
Class/default-owner/Super-default checks. Leave collision instance validation
closed; add separate native-input Struct/Property authority. Mark new Struct,
default/schema/Enum GC edges and count allocations, capacities and strings once
in retained budgets. Publish259 schema objects atomically through the existing
weak lookup/publication journal; defaults are not named lookup aliases. Stage
unique-name counters alongside allocations and all throwing validation.

Header/string/cast/property/default access must recognize that authenticated
native graph before serialized-source fallback. Describe/ClassProperty/
OwnsProperty currently rely on serialized descriptors; Alias member ownership
must follow its actual native Struct, not a forged ClassDescriptor. Do not just
append new default-kind enum values: existing kind>=Engine checks would then
misclassify Input payloads as Engine values. Use explicit kind/domain checks.

Expose detached indices/lists/typed-default snapshots for tests. No need to
invent public VM struct layouts or grant generic native construction authority.

## Config and instance boundary

Nothing unknown prevents implementing this full field/link/pre-config-default
batch. Track defaultsPrepared separately from configApplied. It is not all of
StaticInitInput until its final actual CDO LoadConfig(1,None,None) is handled.

Use the explicitly bound User.ini, existing ordered INI and typed Name/String/
Struct imports, actual Property traversal and Port semantics. Import Aliases[i]
individually. Parent config precedes descendant copying; ordinary Config fields
use the current Class section and missing scalars preserve storage. Implement
or explicitly retain pending LoadConfig propagation, never silently omit it.

Actual InputExt instances still require audited constructor/default-reload
order, Extension name bootstrap, actual Enum, Transient Outer, viewport lifetime
and backedges, followed by the separately ordered ResetInput/platform service.
Neither field/default snapshots nor registration alone prove those services.
