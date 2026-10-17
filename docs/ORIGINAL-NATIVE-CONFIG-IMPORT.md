# Native config typed imports (2026-10-17)

This closes a data-import prerequisite for configured Engine defaults. It does
not construct native defaults, resolve live UClasses, initialize an Engine or
enable crate destruction/physics. Only static inspection of the owned original
binaries was used; none were loaded or executed. The source hashes and explicit
Editor-config/file-binding boundary remain in ORIGINAL-NATIVE-DEFAULT-CONFIG.md.

## Original Core evidence

The private ReadToken body is10164CB0..10164F70, distinct from ParseToken.
UIntProperty.ImportText is10165F00..10165F7E, UBoolProperty.ImportText
10166380..1016650D, UObjectProperty.ImportText10166E20..10166FAE and
UClassProperty.ImportText10167110..10167184. These are body addresses from the
audited Core.dll, not portable ABI offsets. Core.LoadConfig ignores scalar
ImportText's return at10151321..10151327, so writes on a null return matter.

- ReadToken does not skip leading spaces. Only an initial ASCII letter/digit
  enters its identifier branch; continuation permits letters, digits, `_`, `-`
  and optionally `.`. Other first units produce a one-unit token without
  advancing the input pointer. Quoted tokens require a closing quote; `\\`
  produces a backslash and other escapes consume two hex nibbles, mapping each
  invalid nibble to zero. This is not C-string escape syntax. Exactly1023
  token units are rejected even if the closing quote follows immediately.
- UInt only writes for an initial digit or minus. It calls appAtoi once, then
  consumes any run of digits/minuses: `12-34` writes12 and consumes5; `--12`
  writes0 and consumes4. Leading whitespace/plus leave storage unchanged but
  return the original nonnull pointer. Signed overflow of the imported CRT
  `_wtoi` remains outside the established profile and explicitly refuses.
- UBool accepts case-insensitive `1`/`True` and `0`/`False` tokens and updates
  only the property's supplied bit mask. Invalid input preserves storage and
  returns null. No general boolean coercion is substituted.
- UObject first recognizes None, then supports a plain dotted token or typed
  `Class'Path'` syntax. Plain successful lookup advances one delimiter, even
  past the CString terminator. Typed syntax first finds the explicit UClass,
  then its object. Parse/explicit-class failure preserves storage; final object
  lookup failure writes None and returns null.
- UClass delegates to UObject, then requires the found object's exact
  Core.Class metaclass and actual Super ancestry against MetaClass. Constraint
  failure writes None but keeps a successful token-cursor return.

The lookup is StaticFindObject10151DE0..10151FD0, not StaticLoadClass. Its
ANY_PACKAGE fallback scans actual indexed GObjects in ascending index order.
No loading, constructor execution or string-to-class capability is implied.

## Implemented bounded helpers and owned-data check

`native/quest_native_config_import.h` implements these token, Int, Bool and
Class-import rules with UTF16-unit cursors and explicit return/write semantics.
Class lookup, exact-metaclass and Super checks are separate callbacks; callers
must supply genuine registered object authority. The helper creates none.
Input is bounded to16384 units; incomplete quoted hex escapes that would read
or advance beyond the existing CString terminator explicitly refuse. Unsupported
resolver exceptions preserve the destination. These are portable safety
boundaries, not claims about out-of-bounds retail behavior.

The read-only owned-data audit now decodes actual Engine.Engine CacheSizeMegs2
and UseSoundTrue tags, then applies typed scalar imports in inheritance order:

| Class section | CacheSizeMegs | UseSound |
| --- | --- | --- |
| Engine.Engine | 2 | True |
| Engine.GameEngine | 2 | True |
| Extension.GameEngineExt | 2 | True |
| DeusEx.DeusExGameEngine | 4 | True |

The five configured ClassProperty values are token-inventoried only and
explicitly require an actual registered resolver. The audit fingerprints source
bytes, size and modification times before/after; all remain unchanged. It
publishes no runtime objects/defaults or gameplay state.

## Verification and next boundary

The commercial-free helper test passes20348 checks/10 explicit safety refusals,
including independent integer expectations from-10000 through10000, token
capacity/cursor/escape controls, bit-preserving Boolean masks and an opaque
catalog fixture for class lookup order/failures/ancestry. That fixture is test
data, not native UObject registration. Targeted host and strict ARM64 syntax
checks pass; the owned scalar inventory passes separately with an explicit game
root. A coherent full rebuild passes. Fresh CTest:127 passed/145 registered/
18 explicit owned-data skips/zero failures in38.75s. Local ignored evidence:
`artifacts/native-config-import-*-20261017.log` and
`artifacts/native-defaults-owned-import-inventory-20261017.log`.

Actual native static-property/default storage, exact indexed Core.Class lookup,
inherited configured default publication and Engine/Transient/Cylinder lifetime
remain required before movement/startup/falling and the live crate damage graph.
No replacement APK or headset acceptance is claimed by this checkpoint.
