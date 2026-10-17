# Original native default configuration (2026-10-17)

Prerequisite for actual configured Engine default preparation/instance lifetime,
not a claim that Engine/Transient/Cylinder exist, native Register/LoadConfig is
complete, or crates now fall/break. The previous Mesh/selected-Model query
providers remain intact. No original executable or DLL is loaded or executed.

## Original evidence and explicit embedding boundary

Core's exported GConfig is accessed through a factory supplied to appInit. The
packed DeusEx.exe factory binding remains unproven. The shipped Editor.dll has
an identifiable FConfigCacheIni/FConfigFile implementation. The new portable
data reader explicitly binds **that audited profile**, default CRT C locale,
and a supplied owned file; it does not claim to recover the packed launcher's
factory, System/User aliases, command-line selection or cross-file layering.

Audited file SHA256:

| File | SHA256 |
| --- | --- |
| Core.dll | 8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4 |
| Engine.dll | 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4 |
| Editor.dll | 93E31B3E401880AB17AB4561C3FD4D2BAA530FBE25CCB5154911EBAD60FFF035 |

Editor FConfigFile.Read1021B040..1021B37C clears the current file map, then:

- Independently skips CR/LF and splits on either, without a fixed retail line
  limit. The first resulting NUL ends the scanner.
- Recognizes a header only if first unit is `[` and final unit is `]`. Empty
  section names and repeated sections are supported; repeated sections merge
  within this read, not through an invented file-inheritance chain.
- In a current section, splits the first `=`. Empty keys/values are retained.
  No whitespace trim, comment stripping, include/continuation, indexed-array
  syntax or `+`/`-`/`!` command expansion occurs.
- Removes one pair of quotes only if first/last value units are double quotes
  (1021B312..1021B335). A lone quote becomes empty: advancing to its existing
  terminator and zeroing the preceding quote does not require a memory-error
  substitute. There is no escape processing.
- Keeps duplicate key/value pairs. Hash chains are newest-first. Scalar
  GetString1021B620 selects the newest matching pair.

Comparison uses imported Core.appStricmp10123560 to MSVCRT._wcsicmp; hashing
at Editor1021AFB0 folds ASCII a..z only. The port selects the same default-C
locale string profile already used by quest_unreal_string.h, with no Unicode
case expansion or dynamic locale change.

Core appLoadFileToString1010323D ->101488C0..10148B4E reads complete bytes.
Even-sized input beginning FF FE or FE FF decodes BOM-stripped UTF16 LE/BE.
All other input widens each unsigned byte to U+00xx. Odd BOM files therefore
fall back to byte widening; UTF8/BOM detection is not silently added. The
portable reader returns the existing UTF8/WTF8 FString transport, preserving
unpaired/split surrogate units. Embedded NUL terminates parsing after decoding,
not file-size/resource admission.

## Core LoadConfig array/scalar rules

Core.LoadConfig thunk101023DD ->10150D60..10151384 receives
Propagate0/ClassNone/FilenameNone from Register10127983. Null class selects the
actual object's Class+24 and requires ClassFlags&4. This call does not recursively
load parent sections. Ordinary CPF_Config4000 properties use the current class
GetPathName section; GlobalConfig40000 uses actual property.GetOwnerClass.
Parent registration/configuration still occurs before descendant defaults are
copied. Absence of a parent-section recursion is not permission to omit that
earlier parent phase.

Null filename normally selects actual Class.ConfigName+4A0 (`System` for the
native engine chain); Editor.Find later adds .ini and maps System/User names.
The audit command's supplied `System/DeusEx.ini` is an explicit owned-file
binding, not proof that filename0 means that physical path everywhere.

Dynamic arrays call GConfig.GetSectionPrivate slot1C with Force0/Const1 at
10150F40..10150F51. Editor slot target1021BC70 returns null for a missing
section: Core leaves the existing array unchanged. A present section gathers
all matching values, destroys the old array at10151084 and rebuilds it, even
when there are zero matching values. Missing key in a present section means an
empty array. Gathered newest-first values are imported in reverse at
101510C6..10151115, preserving source order and duplicate/empty entries.
UStrProperty.ImportText10167980 with flags0 copies the complete string,
including spaces in ServerActor command lines; it does not tokenize them.

Scalar missing values leave existing storage untouched. Core passes a1024-unit
GetString destination; appStrncpy101233C0 force-terminates unit1023. The new
PropertyScalar helper therefore returns at most1023 UTF16 units, preserving a
split surrogate. Raw Scalar and array reads remain untruncated.

LoadLocalized10151390 exits unless ClassFlags&20 and not GIsEditor. The audited
engine chain's effective inherited11E flags do not gain20. This is a gated
no-op for that chain, not permission to omit localization for other classes.

## Implemented bounded data service

`native/quest_native_config.h` supplies immutable OrderedIni records, newest
scalar views, owned/truncated property-scalar values and ordered string arrays.
No UObject, default buffer, class import or engine capability is fabricated.
Scalar views require a retained, unmoved document; direct rvalue calls are
deleted. Existing quest_engine_config.h Root/GameEngine selector semantics are
unchanged and are not reinterpreted as this general backend.

Portable limits (callers may only tighten positive values):1 MiB source,
16384 UTF16 units/line,1024 sections,16384 entries,8 MiB conservative retained
record/string capacities and8 MiB byte-comparison work per parse/query. These
are not retail maxima. Canonical lookup transport and decoded unit bounds are
checked. Retained capacity includes SSO string capacity conservatively; it is
not process RSS. Source and UTF16 parse scratch are separately file-bounded.
Reads are read-only, check size before allocation and detect truncation/growth;
they do not promise an atomic snapshot against concurrent same-size writes.

## Actual owned metadata inventory

`portable_native_engine_defaults_audit` now traverses bounded actual Class
Children/Next chains in Core/Engine packages and reports source native fields,
serialized tags and per-class config actions. It fingerprints bytes/size/mtime
before/after, initializes no runtime and constructs no classes/defaults/instances.

Core.Object/SubSystem have zero tagged defaults. Engine.Engine has15 declared
properties and exactly CacheSizeMegs Int2 / UseSound BoolTrue tags. Its seven
ordinary config properties are GameRenderDevice, AudioDevice, Console,
NetworkDevice, Language, CacheSizeMegs and UseSound. Language is genuinely a
ClassProperty constrained to Core.Language, not a String to be guessed from
the INI `int` spelling. Inherited config/typed class imports need real handling.

UEngine.StaticConstructor1037D610..1037D6EC creates CacheSizeMegs at5C and
UseSound at60 with native Settings/config metadata, and zeros CurrentTickRate
at64. Serialized property flags include Edit bits and must not be replaced by
remembered constructor arguments. UGameEngine.StaticConstructor creates
ServerActors atB8 and ServerPackages atC4, with StrProperty inners. Native
property Outer/GetClass, Children insertion, inherited default copying and
full typed config/default preparation remain distinct from this inventory.

The supplied owned INI concrete section overrides CacheSizeMegs to4, retains
UseSoundTrue, imports five ordered ServerActors command strings and replaces
ServerPackages with an empty array. Missing intermediate native-class sections
preserve inherited values. These are config-action data, not prepared native
class/default-buffer or instance authority.

## Verification and remaining work

Strict host/ARM64 builds and41717 checks/25 refusals pass, including original
owned INI read-only comparison. Generated controls exercise source/scalar/array
semantics, every byte, both UTF16 BOMs/odd fallback/NUL/surrogates,1023-unit
scalar truncation, raw/quoted/space-bearing duplicate values, limits and256
random duplicate-section inventories. The read-only original metadata/config
audit passes with unchanged source bytes/mtime. The coherent full rebuild passes.
Fresh CTest:126 passed/144 registered/18 explicit owned-data skips/zero failures
in40.03s. Independent read-only review confirmed the profile; decoded lookup-unit
bounds and temporary-document borrowed-view guards were added from its findings.
Local ignored logs use `artifacts/native-config-*-20261017.log` and
`artifacts/native-defaults-*-20261017.log`.

Next: actual typed native static-property/default-buffer preparation and
inherited Engine defaults/config class imports; then explicit Engine/Transient/
Primitive construction/session roots and Level.Engine binding; then original
hash load lifecycle, Move/support/startup/falling and real crate damage graph.
This data service does not claim those later phases or a new APK/headset test.
