# First Training explicit load-context save plan

Initial design audit and implementation checkpoint, 2026-10-22. Only
`Maps/00_Training.dx` is the actual-map acceptance target. The initial audit
preceded runtime integration; it must not be read as the current implementation
status. No original commercial executable/library was loaded or executed.

## Current checkpoint: filtering/save regressions verified; startup incomplete

The implementation now carries an explicit immutable load profile
through script/map construction, excludes fresh source exports before allocating
UObjects, preserves source-aligned null export cells and projects excluded Level
references to None without modifying source bytes. Source-reference, sparse
runtime-loop, save-schema and warm/cold restoration seams have been updated.
The Quest entry point and owned Training probe explicitly select noneditor
client/server loading; omitted profiles retain unfiltered inspection mode.

Authored nested Object members now have an explicit loader-aware optional
identity resolver. Successful None is distinct from missing/wrong-type refs
(which throw) and an engaged empty path (which remains malformed). Historical
string-only callbacks keep their prior empty-path refusal; binding both
callbacks is rejected. Focused decoder verification passes511 checks/160
rejections without raising the input/node/identity/retained limits.

Runtime persistence now adds and validates the profile-bound `DXLC` envelope
described below, including early mismatch rejection. Gameplay-prefix validation
rejects source-proved exclusions in inventory/inactive/activated/damaged lists;
cold preflight can use the intended map's immutable source membership instead
of depending solely on the current world's lookup registry. The generated
runtime integration below has now exercised these paths successfully.

The two focused helper tests pass (2/2): linker-context selection and structural
save-envelope controls. They do not construct the full runtime or prove player
startup. The expanded generated runtime fixture now passes13586 checks/223
rejection controls, including its complete `LoadContextSynthetic` section:
sparse script/map storage, direct/nested None, source indices/slots, GC absence,
profile mismatch, byte-exact warm/cold replacement, 28 same-profile excluded
gameplay-prefix cases, four unloaded-map validate-only exclusion cases and an
unrelated prior-map inventory positive control. Valid typed imports of excluded
objects resolve to None; wrong imported classes refuse rather than bypassing
validation. Evidence: `artifacts/training-load-context-generated-20261022.log`.

The owned first Training probe now verifies1356 admitted/1991 excluded exports,
312 source Actors and1337 preserved serialized slots, including excluded
editor Camera/Brush references resolving to None while compiled BSP remains.
Two same-root startup attempts each execute201628 instructions/14642 writes,
observe117 Spawn operations including actual JC Denton and starter keyrings,
complete335 PostPostBeginPlay visits, and partition1360 slots to335 with the
first dynamic slot192. Both stop at `Unsupported runtime native 3015` in
`DeusEx.DeusExGameInfo.Login`, PC133: Login has not returned an accepted player
or published possession. Native work is459538 cold/458996 warm within the
unchanged1048576 limit. Full save/state/revision/hash/RNG/client/lookup/Actor/GC
rollback and all41 unchanged owned sources pass; no other map is loaded.
Evidence: `artifacts/training-load-context-repeat-20261022.log`.

Strict ARM64 runtime/test compilation, Android native linking and the full
desktop rebuild pass. Shared CTest passes167 executed/197 registered,30
explicit owned-data skips, zero failures in66.18s. Separate owned first-Training
movement122706/21 and isolated crate347/two contents-refusal regressions pass.
The latter still deletes Lockpick contents through dormant Game=None relevance;
it is not surviving usable loot. Logs are under
`artifacts/training-load-context-{ctest,movement,crates}-20261022.log`.

No completed save migration, accepted original player Login, live crate loot or
gravity, new APK, or headset acceptance is claimed by this checkpoint.

## Identity and ownership invariants

- An explicit non-editor client/server load profile is engine-owned immutable
  loading authority. The old generated/reflection fixtures retain an explicit
  raw-inspection compatibility path. Client existence is not itself a profile.
- Each original package retains its full source-aligned export segment. An
  excluded ordinal is `nullptr`, not an allocated placeholder, lookup entry,
  cached object or compacted-away ordinal. Source export and native linker
  indices do not change. Actual births append after all authored segments.
- A serialized native Level actor reference resolves to None when its export is
  authoritatively excluded. The raw source reference is not edited; the runtime
  projection preserves that original slot until the genuine native partition.
- Profile exclusion is not Actor destruction: no invented `worldRemoved`,
  deletion queue, empty/default UObject or lifecycle record supplies it.
- An admitted export unexpectedly absent is an error, not an excluded export.
  Schema eligibility must use the same source flags/profile policy as loading,
  including its established dependency rules, never a missing lookup alone.

## Implemented actor-order seams and focused runtime verification

`portable_saved_actor_order.inl` keeps raw reference bounds/duplicate validation,
then projects excluded authored references to zero through the schema's
`SourceExportAdmitted(index)` authority. Synthetic fixture fallback likewise
selects only admitted exports. Explicit saved authored slots cannot name an
excluded export. Omission of a context-excluded original does not require a
fabricated removed lifecycle; omission of an admitted actor still does.

`portable_stage_actor_order.inl` retains every authored null export cell,
preserves script/map segment lengths, and appends only real staged births. Name
registries must contain actual UObjects, not null placeholders. Source export
resolution distinguishes excluded None from an admitted-but-missing error.
Source/reverse-index censuses skip None; every actual map Actor still receives
its staged member index or `-1`. Publication remains allocation-free and does
not reattach/reorder source linkers.

`portable_world_actor_order.inl` full-order undo census and
`portable_deferred_actors.inl` detached queue observation now skip export holes.
None slots remain present in the native actor-order snapshot/rollback array.

## Runtime-owned null-sensitive seams

The following null-hole handling is now implemented. Approximate numbers retain
the initial audit's locations; function names are the stable anchors. Generated
runtime replacement and the owned Training rollback exercise the combined
sparse graph; they do not establish live gameplay or complete campaign startup.

| Source location | Implemented handling |
| --- | --- |
| `CollectScriptSavedState`, approximately10285/10322 | Skip null exports before native-clock, AI, class-default or actor-state reads |
| `PrepareScriptSavedState`, approximately11361 | Skip holes in authored child-link aggregate census |
| `SavePortableRuntimeState`, approximately15761 | Skip holes in inactive/activated/damaged/mover capture without changing source segment counts |
| `ReadPortableRuntimeState`, approximately16187 | Skip holes while resetting previous native AI ownership |
| Same, approximately16203/16218 | Skip holes during global script/lifecycle and map gameplay reset |
| `ClearNativePlayerEdges`, approximately1046 | Skip holes; no excluded UObject has a session edge |

These are not the only loader loops: initialization, metadata refresh,
dispatch indexing, state/default materialization, object lookup staging and
map unload must also accept sparse source-aligned storage. A `continue` must
never accidentally skip retaining the corresponding ordinal in the export
vector itself.

## Schema and cold-validation authority

The `ScriptSaveSchema` constructor now publishes map class/export identities
only for admitted source exports and separately caches excluded source paths as
metadata, not UObjects. `SourceExportAdmitted(index)` checks source bounds and
applies the selected immutable profile. Its bounds failure must not silently
become a legitimate excluded None.

The following raw-table seams have been updated to share this authority. The
focused passing regressions above do not substitute for broader lifecycle and
gameplay verification of every possible saved record:

- `ReadAuthoredMover` and `NativeLevelPath`: a raw export existing on disk is
  insufficient to create an admitted runtime owner.
- `ValidateAI` and `ValidateNativeDecals`: their sorted native membership caches
  now start from projected `OriginalActorOrderReferences()`, preserving source
  indices instead of admitting excluded raw Level references.
- `RegisterBirths`: the legacy authored slot baseline remains the original
  native slot count, including projected holes. It is not the number of live
  admitted actors. Real birth class/default identity must remain available.
- `SavedClass`, lifecycle links, typed saved Object/Class properties and state
  definitions cannot resurrect context-excluded source actors or script nodes.
- `ObjectClass`'s disk-only asset fallback must apply export eligibility too.
  Otherwise an excluded script export can reappear as a valid symbolic object
  solely because its raw table entry exists. Genuine native asset ancestry
  fallback is a separate established capability, not permission to allocate an
  excluded serialized UObject.

Validation with `apply=false` and validation of a cold map must use the intended
load profile, not current live slots, current overlays, viewport count or the
presence/absence of one cached UObject. The profile must survive initialization,
ordinary map replacement, checkpoint validation/application and shutdown/reset
according to its engine-owner lifetime.

## Persistence boundary: do not silently reinterpret old saves

Historical context-free envelopes encode no load-profile evidence. A pristine
filtered world may still choose the old v3 payload, so actor-order presence or
the saved object count cannot identify the profile. Existing saves must retain
their raw-inspection meaning and exact bytes unless an explicit versioned
migration is designed and verified.

A bounded structural outer codec is implemented in
`quest_load_context_save.h`: `DXLC`, version1, explicit three-bit original load
mask, exact inner length and CRC32 binding profile plus opaque inner bytes.
Its total overhead is20 bytes within the unchanged whole-file cap. Decode
borrows the original inner buffer; no UObject or map is materialized. All-false
profile mask0 is distinct from absent/raw mode. Its structural helper test has
passed; the separate generated runtime integration supplies the tested
warm/cold capture/restoration and rejection evidence described above.

Runtime integration retains existing inner script/mover/quickbar versions and
checks envelope profile equality before schema construction or publication.
The policy is exact: raw saves belong to omitted-profile inspection mode;
explicit-profile saves belong to the identical explicit profile, even when two
different profiles happen to admit the same current exports. Raw-to-explicit,
explicit-to-raw and different-explicit-profile restores are rejected. No implicit
migration, profile guessing or state deletion is authorized by this format.

The cold loader obtains its desired profile at engine initialization, before
allocating the map; it cannot correct an already wrongly allocated graph by
dropping actors afterward. The structural codec is not a recovered original
archive layout. Combined runtime capture/restoration, cold validation and
atomic failure behavior pass the generated cases described above; a helper
pass alone would not supply that evidence. This does not authorize migration,
prove every campaign save combination, or complete original player startup.

## Focused regression requirements

Use generated fixtures for exact profile flag/dependency truth tables, then only
the owned first Training for integration. Cover holes at first/middle/last
package ordinals, script plus map segments, exact linker/export indices, no
lookup/GC placeholder, source-reference projection, native partition rollback,
birth append and cold rebirth, canonical profile mismatch refusal, and unchanged
legacy raw save bytes. Malformed saved records naming excluded actors/functions,
AI/decal/mover owners or typed assets must fail before publication. Failed
admission/restore must preserve live slots, native index caches, RNG, session
edges and source fingerprints.
