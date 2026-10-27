# Original Input field/default phase (2026-10-27)

Follow-up: explicit User.ini configuration of these real defaults is now
implemented and separately tested in ORIGINAL-INPUT-CONFIG-DEFAULTS.md. The
pre-config statements below describe this earlier field/default phase alone.

First Training only. The genuine native field/link/pre-config-default graph is
implemented and verified against the read-only owned evidence below. It is
NOT configured Input, an instance, FExec or ReadInput. The preceding two-UClass
registration phase is separate. No account or
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
Key239 is genuinely UK_Unknown10F, not IK_Unknown10F. Strip the first three
UTF16 units exactly as the original does; do not require or normalize IK_.
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

## Implemented production integration

The separate authenticated RuntimeNativeInputDomain owns the schema and five
defaults without expanding collision's closed11-default/4-field contract.
Alias has a genuine native-UStruct payload with logical size, FriendlyName,
Children and linked heads. RuntimeNativeProperty has a distinct referencedStruct
edge; its existing inner remains ArrayProperty.Inner. Real NameString Alias
identities and typed fixed-size Alias/binding storage replace neither with
serialized descriptors. These values use typed native schema checks,
not pointer arithmetic over original32-bit offsets or an unordered alias map.

Default validation admits exact owning-domain members with actual Class/default-
owner/Super-default checks; collision instance validation stays closed. Separate
native-input Struct/Property authority, GC edges and retained budgets cover the
new graph. Class/default/Children/heads/counter changes and259 schema lookup
objects publish only after all throwing validation. Empty replacement namespace
bucket arrays stage separately; existing/delta nodes transfer without cloning
only during commit. Refusal does not reserve or change live namespace capacity.
Defaults are not named lookup aliases. Names created during real constructors
belong to the embedding's process-permanent FName arena, not a rollback journal.

Header/string/cast/property/default access recognizes the authenticated graph
before serialized-source fallback. Describe/ClassProperty/OwnsProperty now admit
these exact native fields, with Alias members owned by their actual Struct.
Generic mutation of schema objects/defaults explicitly refuses rather than
writing misleading script overlays. Explicit Engine-kind routing prevents the
new default kinds from being treated as Engine payloads by enum ordering.

Cpp Property construction also creates its category FName (Core10164FCC..FE4).
RawKeys is absent from the owned package name tables, so preparation performs
that real arena insertion, not just string storage. Detached reads only find
the existing identity; they do not intern names. None remains genuine name0.

Expose detached indices/lists/typed-default snapshots for tests. No need to
invent public VM struct layouts or grant generic native construction authority.

## Config and instance boundary

Snapshots report defaultsPrepared=true/configApplied=false. This is not all of
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
The complete next config/instance/ResetConfig evidence and explicit service
boundaries are in ORIGINAL-INPUT-CONFIG-INSTANCE.md. The genuine post-Possess
continuation is separately documented in ORIGINAL-TRAINING-POST-POSSESS.md.

## Verification

### Joint schema/HUD memory regression

Adding the real schema initially pushed the existing32MiB combined retained
budget over its limit during the two-Fire diagnostic's HUD construction. Lazy
mover/texture storage alone moved that failure later, into compass creation.
The final representation also makes Actor animation clocks and state objects
lazy. Metadata/default/HUD UObjects no longer embed unused actor-only payloads.
Actual payloads remain accounted; state before-images are deep copies and
rollback/restore swap owned pointers without allocating during publication.
Animation/state save values retain their existing wire representation. Authored
texture caches remain in their pre-existing immutable loader/asset ledger.
Neither the production budget nor the diagnostic native-work limit was raised.

The complete cold/warm two-Fire test again reaches the prior authentic FExec
boundary at228837 instructions/26047 writes, preserving48 rows/two cycles/two
notifies/four traces/two crate changes/nine births and complete undo. This is
the joint regression control, not a substituted earlier memory-failure result.
The earlier budget failures remain in separate pre-lazy-storage/pre-lazy-clock
logs. Real first-Training material checks preserve227 selected materials,
128 nonnull MultiSkin slots and114 selected overrides, with zero null materials.
Training recording-route coverage passes1232 checks, including paired-door
mid-motion/key-door saves and explicit unsupported generated-map restore
refusal. The generated two-Mover-only fixture has no native Level/restore
authority; its prior success assumption was corrected without loosening the
production save guard or removing real Training's positive restore tests.

Owned integration passes26262 checks/1342 refusals: real Actor255-key enum;
Alias plus258 native properties; exactly259 weak lookup records and265 GC
objects including five default blocks/domain; literal layouts and independent
source inherited-list order; header/default/property reads/casts; detached typed
values; missing-phase refusal; unchanged prerequisite domains; dormant first
Training checkpoint/warm restore/unload/reload/GC/shutdown and source integrity.

Three fixture assumptions were corrected, not production behavior: key239's
actual UK_ spelling; package baselines before/after map load/retirement; and
real map Package constructor's attempted-but-absent legacy-module binding.
No module is executed or loaded. Late allocation/budget failure is structurally
staged but not fault-injected by these precondition refusal tests.

Desktop build, strict GNU test/ARM64 runtime checks and Android-native link pass.
Final shared CTest:219 executed/260 registered/41 owned-data skips/zero failures
112.60s. The new owned test above runs explicitly rather than treating its CTest
skip as a pass. Separate owned class registration10413/38, crate415/5,
deferred429/21, animation588/24 and FastTrace252/0 regressions pass. Actual
Training startup/doors/two-Fire probes remain rollback-only diagnostics, not
committed startup, live VR weapon damage, usable loot or automatic gravity.
Final startup repeat2 retains239200/19364 with144 dynamic slots/calls; paired
door repeat2 retains239536/19432 and both56-unit moves/two ends. Both reach
the precise missing configured Input service, not a new budget refusal. All
three controls verify cold/warm immutable Model proof reuse, complete root
undo and cold same-filename generation replacement. Final immutable audit
confirms45 original files and9 compiler inputs unchanged by length/mtime/SHA256,
with every compiler/test/probe handle terminal before commit.

Ignored local evidence: artifacts/input-schema-final-*-20261027.log. Earlier
discovery/owned-package failures are retained separately; the final owned-
optimized log is the passing integration control after lazy storage. No new APK.
