# After Audio: original Input reset and detail continuation

Read-only continuation audit, 2026-11-13. Integration scope is ONLY the first
`Maps/00_Training.dx`. This document is source evidence and a next-patch plan,
not an implementation, completed startup, live physics or headset acceptance.
It was written while compiler inputs and runtime tests were frozen/running;
no compiler input was edited and no build or game test was run for this audit.
Original DLLs were read as data by external GNU objdump, never loaded/executed.

## Source identity and evidence

Owned installation: `D:/Steam/steamapps/common/Deus Ex`. SHA256 values were
reread while preparing this document and match the preceding source audits:

| File | SHA256 |
| --- | --- |
| System/Engine.dll | 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4 |
| System/Engine.u | 1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA |
| System/Extension.dll | 93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C |

The retained full caller is
`artifacts/world-startup-contract-20261011/original-loadmap.asm`, especially
the instructions at1038DFC6..1038E059. This audit also freshly read the
Engine.dll DetailChange body1039CF30..1039D0CF and the aligned detail-selection
instructions1038D6A9..1038D6E3. Existing export/vtable authentication is retained
in `ORIGINAL-POST-PARTITION-CLIENT-SETUP.md` and
`ORIGINAL-QUEST-INPUT-PROVIDER.md`; those documents distinguish historic gaps
from later implemented services. Addresses below identify PE32 source behavior,
not portable field offsets or permission to instantiate Windows drivers.

## Exact selected LoadMap order

Following genuine player creation and non-Entry brush-tracker construction:

| Source range | Required operation |
| --- | --- |
|1038DFC6..1038DFDB| Read actual Engine.Audio; if nonnull call its GetViewport virtual+84, then SetViewport virtual+5C with that returned pointer. |
|1038DFDE..1038DFEB| Start slot0; reread actual Engine.Client and its live viewport count/data on every loop test; capture that slot's actual viewport. |
|1038DFEE..1038DFF3| Read the captured viewport's Input and call its actual virtual+68, ResetInput. There is no nullable-Input skip here. |
|1038DFF6..1038E001| After ResetInput returns, reread the SAME captured viewport's actual render-device pointer; if nonnull call that device's virtual+68 with Int1. |
|1038E004..1038E005| Increment the slot and return to the live-array test; do not retain a copied count/array across callbacks. |
|1038E037..1038E053| Outside the Client-only block, reread the actual current Level/slot-zero LevelInfo detail bit and invoke Level.DetailChange virtual+D4. |

The no-Client branch still reaches DetailChange. No head-pose, focus, viewport
metadata count or fabricated Audio=None predicate replaces these native gates.
A real initialized Audio provider whose actual current viewport is None remains
distinct from an absent Audio subsystem. Its Get/Set(None) path is not evidence
that channels played or that the original initial engine driver ran.

The configured Extension.InputExt ResetInput slot inherits original UInput:
base vtable10428C18+68 ->10302A45 ->10394910; Extension slot1005C454
->10054642 ->IAT100592D8. It is not native544 ResetKeyboard/ResetConfig.

## Existing Input service versus missing caller authority

`PortableActorVmHost::ResetNativeViewportInput(viewport, execution)` in
`native/portable_input_initialize_host.inl` is the reusable later-reset entry.
It already shares the exact255 key clears, independently selected inherited
Byte/Float input-property walks, Action/Delta clears and real UpdateInput(1)
coordinator with initial Init. It requires the actual initialized receipt,
current Pawn/Viewport/Input backedges, matching input source and root ownership.
Its COW payload, reference undo and work/storage accounting stay in the SAME
startup execution; a public nested transaction is not an alternative.

Do not call InitializeNativeViewportInput at this later site to satisfy those
requirements. The ordinary constructor-only diagnostic does not have completed
Init authority. The explicit initial-input primitive diagnostic does establish
Init and a later-reset proof inside its rollback root, but that diagnostic is
not the production initial GameEngine driver. It must not silently become an
unconditional initialization phase within LoadMap.

After any Input/platform/script callback, validate the live Client/root lease
again and reacquire the captured viewport's native payload before reading its
render-device pointer. On the next iteration reread the actual Client array.
Preserve the existing native ownership/receipt refusal rather than continuing
through a stale or cleared session.

## Renderer boundary: genuine null is not a fake provider

The existing `RuntimeQuestViewportPayload` has an authenticated pre-OpenWindow
baseline: `renderSlotBound`, null `renderDevice`, and `windowOpened=false`.
`RequireQuestViewportPayload` and the native Client root graph can prove that
baseline. It legitimately skips the conditional render-device call above in a
pre-open diagnostic. Missing payload, an opened viewport without a provider,
or a nonnull unsupported provider must not be converted to that null branch.

The semantic identity and complete body of the selected render-device
virtual+68(1) are NOT authenticated by the retained audit. Do not label a no-op
as an implemented Flush, infer it from the portable renderer's method names,
or treat existing OpenXR targets as an original native RenderDevice instance.
The native Client root capture currently explicitly refuses a nonnull device
without its retained-byte provider; viewport Exec also explicitly refuses its
missing concrete render-device service. A future real Quest render provider
needs exact Class/default/instance ownership, output lifetime, retained-byte
accounting and transactional command preparation/publication before these
guards can be replaced. No Windows/Glide handle should be invented.

## Detail selection and real ULevel.DetailChange

Earlier LoadMap instructions1038D6B3..1038D6BB unconditionally set bit3 in the
actual slot-zero LevelInfo packed word. The following1038D6C3..1038D6E3 clear
it ONLY when actual Client exists, its actual viewport array is nonempty, the
first viewport has a nonnull render device, and that device's HighDetailActors
value is zero. A genuine pre-open null device retains the high-detail bit.
Desktop INI settings alone do not prove an initialized renderer or its value.

At1038E044..1038E053 the caller rereads that bit, shifts/masks it to0/1 and
calls actual Level.DetailChange. The authenticated Level vtable10428AFC+D4
contains exported thunk1030315C ->body1039CF30. The fresh bounded body read
confirms:

1. Assert the actual slot-zero Actor exists and has original LevelInfo ancestry.
2.1039CFC2..1039CFD8 replace ONLY bit3 using `(inputInt & 1) << 3`, preserving
   all other packed bits. The native helper is low-bit conversion, not general
   nonzero truthiness; LoadMap itself supplies only0/1.
3. Reread actual slot-zero LevelInfo and its current Game pointer. A genuine
   Game=None skips the event; no dummy Game or forced absent Game is allowed.
4. For nonnull Game,1039D09A..1039D0AD supplies ENGINE_DetailChange FName
   10583618 and Global0 to native FindFunctionChecked, then1039D0BC ProcessEvents the
   selected function with no argument/result packet.

The portable write must authenticate the actual declared
`Engine.LevelInfo.bHighDetailMode` Bool schema and native packing evidence;
this document does not guess declaration flags or derive false from a missing
serialized tag. Use the existing reference/execution journal. Invoke the real
selected GameInfo callback in that same execution, not a forced Engine base
function and not only a flag write.

The retained original-script audit records GameInfo.DetailChange125 logical
bytes: low-detail mode destroys bHighDetail and not-bGameRelevant Actors, but
both detail modes then iterate ZoneInfo and call original LinkToSkybox. Those
callbacks and their selected skybox references are required world mutations.
Reuse normal script/iterator/destruction providers, retaining any concrete
unsupported refusal; do not replace this work with a skybox presentation flag.

## Minimal next root patch and acceptance boundaries

- Add the actual per-viewport later ResetInput loop immediately AFTER the
  Audio Get/Set pair, using the existing initialized service and shared root.
- Reacquire and validate the genuine render slot after each reset. Permit its
  established pre-open null branch; keep a named refusal for nonnull unsupported
  device behavior. Do not implement a fake render callback.
- Add the earlier source-backed detail-selection write at its proper LoadMap
  phase, then the real Level detail coordinator and selected Game callback at
  the later site. Keep their writes/iterations/destruction under existing undo,
  RNG, birth/death, native work, Client and output leases.
- Record which genuine services completed, compare repeated observations and
  verify full checkpoint/Input/audio/lookup/Actor/GC rollback on the actual
  first map. Do not turn merely reaching DetailChange into startup acceptance.
- Replace the aggregate guard only as far as implemented source warrants.
  The later LoadMap FURL/current-URL/config/return tail beginning1038E059 still
  requires its own bounded audit/integration; keep an exact subsequent refusal.

Original initial GameEngine.Init remains a distinct driver: actual empty
Client construction/Init, Browse/bootstrap work, NewViewport (Canvas.Init and
Input construction), Console.Init, real SpawnPlayActor, initial Input.Init,
actual OpenWindow, then detail selection/callback. Initial Input.Init must not
be moved into later LoadMap.ResetInput. Genuine bootstrap Entry/FURL authority
is an engine prerequisite, not permission to integrate other campaign maps.
None of this audit completes that driver, live VR control/weapon/frame wiring,
crate destruction/loot/gravity, or a new APK.

## Source handoff: the LoadMap URL tail

The next tail starts at `1038E059`. Through `1038E318` it deep-copies the
incoming FURL to GameEngine `+74`: Protocol, Host, Port, Map, ordered options,
Portal, and Valid. The per-member alias checks do not authorize writes to
the adjacent reserved Level fields. `1038E31B` tests the imported `GIsClient`
global, not whether the Engine has a Client object or any open viewport.

On the client branch, FURL.SaveURLConfig is invoked for `Name`, `Team`,
`Class`, `Skin`, `Face`, `Voice`, and `OverrideClass`, in that order, in
`[DefaultPlayer]` of the `User` config namespace. The selected original body
(`103032DD -> 103F9CB0`) splits each ordered option at its first `=`, matches
the full key case-insensitively, and writes the matched option's key and value
through SetString. Each match is applied (duplicates are last-wins), absent
values leave the cache unchanged, and an explicit empty value is valid. The
copy to the native 1024-wide local buffer needs bounded admission, not silent
truncation. There is no Flush at this site. Reuse the owned cache's COW and
ordered-document replacement, but do not request its persistence operation.

The existing Login source contains only Portal and ordered options. It cannot
be treated as a complete FURL, nor can one be reconstructed from the flattened
InitGame option string. A single complete source must be shared by LoadMap,
InitGame, and SpawnPlayActor, with an explicit, authenticated process role. The
tail retains the *actual* Engine current Level at `1038E3C6` and returns it at
`1038E44E`. This describes a future journaled implementation, not proof that
the map can now be published.

## Source handoff: initial audio order

Base `UGameEngine::Init` loads Entry at `103892F5`, browses at `103895E2`
(fallback `1038965B`), and creates the viewport only at `103896C5`. After
OpenWindow (`10389776`) and Level.DetailChange (`10389785`), it calls
`UEngine::InitAudio` (`1038978D`; thunk `10303314` to `1037FE50`). The audio
provider is then actually stored at `1037FEDE` and initialized at `1037FEE8`;
the caller reads Engine.Audio and, if present, sets the new viewport at
`10389792..1038979C`. The earlier base `UEngine::Init` does not perform this
audio initialization.

Consequently the initial LoadMap/Browse path can have a genuinely empty
viewport array and a fresh Engine.Audio=None. This is distinct from clearing
a live provider or skipping an initialized viewport on later map loads.
Keep early platform output preparation separate from publishing Engine.Audio;
bind the existing Quest transport to the actual new viewport when the native
initial driver reaches audio initialization. The explicit rollback probe with
an initialized viewport and Audio instead exercises the later LoadMap path.

## Next initial-driver requirement: retained Entry

In the initial driver, `103893CB..103893D4` swaps the actual current Level
(`+68`) into Entry (`+6C`) before Browse. A fresh Entry slot was None, so
the current slot becomes empty, not a second alias for the Entry world.
The existing public replacement loader retires the old linker and truncates
its backing export range before decoding the candidate; calling it unchanged
would destroy the Entry ownership required by this branch.

The owned `Entry.dx` is a v68 package with 36 exports and 17 imports. It has a
real Model and imports `DeusExItems.Skins.BlackMaskTex` and
`Title_Music.Title_Music`. The current Training-only source set admits the
Training track, not Title Music. Pre-admit the actual Entry dependencies,
retain its Package, Model, actors, exact linker slots and lookup identities,
and attach the selected playable map privately. The active-world generation
must not double as the monotonic map-allocation counter if Entry can become
current again. On candidate failure, preserve the private Entry for the real
fallback or unwind the entire fresh embedding. Do not use the already-begun
Training world as a surrogate Entry or expose it before viewport startup.
