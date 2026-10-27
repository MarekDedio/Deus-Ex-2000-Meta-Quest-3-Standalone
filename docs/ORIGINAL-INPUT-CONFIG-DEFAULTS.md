# Original configured Input defaults (2026-10-27)

First Training remains the integration target. `PreparePortableRuntimeInputConfig`
now imports an explicitly supplied, read-only User.ini into the existing real
native Input and InputExt defaults. This supplies configuration for subsequent
player/input construction; it does not implement an account login, character
creation, Input instance, ReadInput, startup or the live Quest weapon bridge.

## Implemented behavior

- Requires the explicitly prepared native schema, its actual owning Classes,
  default objects, Alias UStruct, properties, Enum and live lookup generation.
- Retains one immutable explicit ordered config document. The same normalized
  source path is idempotent without rereading the file; another source is refused.
  No inferred User/DefUser fallback, original-file writes or cache writeback.
- Loads Input first, deep-copies its configured payload into InputExt, then
  loads InputExt parent-first into that same descendant destination. The actual
  non-Config Subsystem gate stops recursion. Three Config-class passes result.
- Iterates actual Config heads/next links. Fixed dimensions select indexed keys;
  ordinary Config uses the supplied class section, GlobalConfig the declaring
  class. Missing keys preserve prior storage; explicit empty strings overwrite.
- Uses the existing native1024-unit GetString profile, including1023-unit value
  truncation. Raw StrProperty import flags0 preserve the whole CString, spaces
  and separators; a returned cursor0 is not treated as failure.
- Imports Alias through its real Children/Next/Super and typed Name/Command
  members. Actual member Port checks element-size product, Category/PropertyFlags
  and the genuine portable Class FName identity, never a retail numeric index.
- Malformed Alias imports retain completed member writes and continue subsequent
  properties. Unknown members use FNAME_Find without interning their spelling.
  Ordinary failures/warnings are reported separately from provider/resource faults.
- Stages both typed payloads, retained document, detached result and budget
  admission before a nonthrowing pointer-swap publication. Existing CDO UObject
  identities, lookup, world revision, map actors and save schema are unchanged.
  Config document/payload capacities and live-plus-staged peak are accounted
  within the existing budget. No budget increase was made.

The original behavior is established by static inspection documented in
ORIGINAL-INPUT-CONFIG-INSTANCE.md and the typed config/struct import documents.
Core101655C0..101655EB independently confirms Port reads the member's own
PropertyFlags at+3C and Category at+40; it does not test an ImportText argument.
Commercial DLLs/EXEs are never executed or loaded.

## Integration and remaining work

The actual first-Training startup diagnostic now explicitly configures these
defaults from its already fingerprinted User.ini before constructing native
player/session objects. This is a real runtime service, not a fixture-only parser.

Configured Input instances, viewport ownership/lifetime, native FExec key queries,
platform input, ResetConfig/ResetInput and the live committing weapon/frame bridge
remain separate required work. Do not remove an explicit refusal or publish a
partially initialized player to make a diagnostic appear to pass.

## Verification

Owned schema/config integration passes34355 checks/1357 refusals. Existing
pre-config controls remain; additional controls cover original Fire/MoveForward
aliases and mouse/keyboard bindings, typed default reads, exact unchanged native
graph identities, malformed Alias prefix writes, continuation after unknown
members, parent-first subclass overrides, latest duplicate values, explicit empty
values, raw whitespace,1023-unit truncation, actual key0 allocation name, key239,
FNAME_Find non-interning, missing/invalid/incompatible-path refusals, changed or
deleted source idempotence, detached snapshots, and first-Training save/load,
unload/reload, GC and shutdown lifetime. Original sources remain read-only.

Strict GNU test syntax, strict ARM64 runtime syntax, targeted desktop build and
Android native link pass. Actual first-Training cold/warm repeat2 diagnostics
load404 original config entries in3 passes without import failures/warnings:
normal239200 instructions/19364 writes, doors239536/19432 with both56-unit
moves/two completion notifications, and two-Fire228837/26047 with48 observations,
two returned attack notifications, four traces, two crate changes and nine births.
All retain their exact unsupported Input/FExec boundary and whole-root rollback,
original-source fingerprints and cold/warm immutable Model lifetime. No budget
was raised and no partial world/player was published. These are expected-boundary
diagnostics, not successful startup or a live Quest crate test.

Full desktop build and CTest pass:260 registered,219 executed,41 explicit
owned-data skips,zero failures82.60s. Separately run owned Input class10413/38,
crate415/5, deferred429/21, animation588/24 and FastTrace252/0 controls pass.
First-Training materials preserve227 selected/zero null materials,128 nonnull
MultiSkin slots and114 selected actor overrides; the known single procedural
texture fallback remains. The owned Training door/save route passes1232 checks.
No replacement APK has been packaged by this phase.

Ignored evidence: artifacts/input-config-*-20261027.log and
artifacts/input-config-frozen-inputs-20261027.json. The latter records all10
compiler inputs frozen throughout the desktop/Android/strict compiler and test
runs. The45-original baseline remains
artifacts/pawn-continuation-original-baseline-20261026.json.

Final immutable audit passes all45 originals and10 compiler inputs by exact
length/mtime/SHA256; all compiler/test/probe processes are terminal. Evidence:
artifacts/input-config-immutable-audit-20261027.log.
