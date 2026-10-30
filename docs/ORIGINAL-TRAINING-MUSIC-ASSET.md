# Original Training Music asset identity: next dependency (2026-10-30)

Source contract and implementation checkpoint, not completed startup or a
native audio-subsystem claim. Only the port's package tools and static native
disassembly were used. No commercial DLL/EXE is loaded or executed.

## Actual failure and source binding

After the original Window navigation implementation, first Training returns
from TravelPostAccept, then stops at Engine.PlayerPawn.ClientSetMusic PC0/
opcode15,266846 instructions/37929 writes. The error is unavailable actual object
Training_Music.Training_Music, not failure to decode or hear the existing music.
The discovery verifies complete rollback and42 unchanged sources.

00_Training.dx import-176 is Core.Package Training_Music with Outer0; import-180
is Engine.Music Training_Music with Outer-176. Original
Music/Training_Music.umx is1450137 bytes, v68, package flags1,8 names/2 imports/
1 export. Its import-1 is Core.Class Music, Outer-2; import-2 is Core.Package
Engine, Outer0. Export+1 is Training_Music, Class-1 (Engine.Music), Base0,
Outer0, ObjectFlags000F0004, offset159,size1449947. Actual standalone load mask
00030000 admits this export. None or an Engine-local object is not a binding.

The real source relation is map-180 -> UMX+1, owned by the authentic
Training_Music Package/LinkerRoot. File SHA256:
0215E4B6C157EB7E011FA02F6A781EFE61FF658BEC7E0E399D2C54E1A440A5AA.
Its payload has an empty tagged-property stream, format Name it, lazyEnd1450106,
dataCount1449937 and bytes169..1450106 within the exact export boundary. Existing
LoadPortableMusic authenticates the Music type and bounded lazy payload.

## Script and native class distinction

ClientSetMusic is original SCRIPT, native0, flagsC2/logical46: ordinary Lets to
Song atPC0, SongSection11, CdTrack22 and Transition33, then Return44. Song is a
scalar Object constrained to Engine.Music; the other three fields are Bytes,
Transition using Actor.EMusicTransition. Params are required80; fields flags0.
This function does not invoke a decoder or an audio-subsystem native.

TypedLiveValue -> ValidateObjectValue can authenticate an actual asset with no
serialized Music UClass wrapper: it derives Engine.Music from the export's
actual ObjClass import, and ClassDerives succeeds on the exact expected class.
Engine.u has no Music Class export. Do not invent one or claim native Music
CDO/construction/destruction to solve the object-admission problem. Broader
Core.Object constraints need audited native Music ancestry, not path guessing.

For future genuine native services, Engine registration10370EB0..10370F2F
supplies size4C, class flags40, registration object flags04084004 and actual
Super/Within Core.Object. Constructor1034D170 initializes its lazy array at+28
(Data+34/count+38/capacity+3C); Format is serialized at+40, OriginalSize+44 is
copied from loaded Data.Num; +48 tail remains unauthenticated. Serializer is
10370BD0..10370CD1. Destroy10370D20 may call AudioSubsystem virtual+70 before
UObject.Destroy. The simpler presentation decoder is not that native service.

## Required admission/lifetime seam

Production and the startup probe currently initialize only the exact shared38
System script packages. The presentation asset resolver searches assets and
decodes audio/graphics, but does not publish VM RuntimeObject identities. Keep
the script list a script list. Use a separate shared dependency discovery from
authenticated actual map/script import records and their native asset types.
Resolve the top-level Package through the configured source search policy,
authenticate import class/outer provenance and the matching actual export,
then admit the real package tables in the normal PRE-MAP source phase.

That phase already owns sourceLinkers, load-context exclusion, export indices,
actual Package roots, source ObjOuter0 binding, lookup generation and native
runtime Package GC lifetime. Presentation decoding must not secretly create
the identity, and an absent/unreadable source must not become a successful None.

Appending exports after the established map boundary is NOT currently a valid
shortcut: persistentScriptExportCount partitions every later export as map
owned, and UnloadMap retires that suffix; native source linkers retain explicit
baseline first/count. Later-map asset admission therefore needs an explicit
source-domain lifetime policy or a supported pre-map preparation phase, not
append-and-retag. Full-campaign loads/travel remain requirements; first Training
is the immediate integration test, not a reason to hardcode its package name.

Required proof: actual map import/export/Class/Outer binding, source load flags,
correct read-only package identity and source offsets, typed Song store, whole
startup-root rollback and untouched originals, deterministic repeated startup,
no map-owned asset retirement/leak and invalid source/provenance refusals. Keep
audible presentation music acceptance separate from these gameplay proofs.

## Pre-map admission implementation (verification in progress)

LoadPortableRuntimeMusicSources now supplies a detached additional-source bundle
without mutating any runtime state or changing the shared38-script prefix. It
discovers actual Music imports from those scripts and the selected map, follows
checked Package/outer provenance, uses ordered Core.System.Paths with native
CString String import, and resolves actual disk casing with exact extension/
first-configured-path priority. Ambiguous case-folded files in one directory,
missing sources/configuration, conflicting baseline Package identities, invalid
import chains and name indices, duplicate qualified exports, wrong actual
Engine.Music class imports and source-context exclusion explicitly refuse.

The additional source files undergo header/count/range preflight before the
existing table loader reserves storage. Conservative cumulative retained-byte
admission covers table vectors, file-sized decoded/interned name storage,
bounded final-name/buffer scratch, ordered INI/Paths lifetime and observations.
Package/binding/directory-entry/source-byte/work ceilings are finite policy;
they are not represented as original engine maxima. LoadPortableMusic validates
each selected original lazy payload within admitted scratch, then releases the
extra validation bytes. File length and UTC write time are checked around
loading; originals are never opened for writing.

Quest startup and the actual Training diagnostic call this shared service before
InitializePortableRuntime. Additional tables join its normal baseline source
linkers, native Package roots and current lookup generation. They do not cross
the map-owned export boundary. The diagnostic fingerprints the newly admitted
original source alongside the scripts/map/INI files. This initial supported
Music closure is not generic lazy asset loading or full later-map admission;
other native asset classes and campaign transitions remain required work.

An owned-data test now covers actual import/export/source/Paths binding,
invalid/truncated source controls, explicit resource limits, normal baseline
Package/index/linker lifetime, the original ClientSetMusic four typed stores on
an isolated Camera (not a substitute Login pawn), partial-store rollback,
checkpoint restoration and same-map unload/reload ownership. Builds/tests are
in progress against1864 frozen compiler inputs; no runtime pass is claimed yet.
Android-native compilation already passes; no new APK is packaged.

The first focused desktop build and strict ARM64 checks also pass. The owned
test discovers/adopts the real Music package, then fails while inspecting its
linker snapshot: the shared snapshot reader calls DestroyObjectFlags, which
requires a loaded Class wrapper even for a read-only native asset export. This
failure is NOT a passing integration gate. A native Music Class/CDO must not be
fabricated to satisfy that diagnostic. Authenticate the actual source export/
Class import/Core.Object header for the read-only snapshot, independently of
the intentionally stricter ConditionalDestroy/native-destructor service.
The actual Training discovery and full regressions remain live on this frozen
snapshot; no compiler input may change until they finish.

The first Training discovery now returns from actual ClientSetMusic, PostLogin
and the original SpawnPlayActor tail (postTravel1/postLogin1/tailReturned1).
It stops at the existing explicit original post-partition client/player/detail
integration guard,266870 instructions/37932 writes,299.915s, exit2. Complete
save/state/revision/hash/RNG/client/lookup/Actor/GC rollback and43 unchanged
original sources are verified, with an independent cold Model generation.
This clears the asset-object failure but does NOT complete startup/publication.
The first full desktop/CTest run passes244 executed/297 registered/53 explicit
owned-data skips/zero failures121.65s; the separately invoked owned source test
above is still NOT a passing gate.

After all jobs terminated, the read-only linker snapshot was separated from the
destruction reader for actual native Music exports without serialized UClass
wrappers. It now authenticates source export/identity, live linker, pinned
Music->Core.Object ancestry, exact Core.Object.ObjectFlags schema, unchanged
immutable header and absence of script overlays. It grants no mutation,
constructor/CDO or destruction service; ConditionalDestroy remains unchanged.
This snapshot is frozen in music-source-header-frozen-20261030.json (1864 inputs)
and is undergoing fresh desktop/strict ARM64/Android verification.

The corrected source-header snapshot passes desktop/strict ARM64/Android builds.
The next two owned-fixture discoveries are retained as NOT passing: placing an
isolated Camera at LevelInfo's position returned None (the correct actual
PlayerStart permits the birth), and unloading that real spawned state correctly
hit the existing per-map archive guard. The final fixture now restores its
genuine pre-birth checkpoint before map retirement; it does not weaken placement
or preservation rules. Original ClientSetMusic's four stores, checkpoint restore
and partial-write undo then pass, along with source/header/index lifetime.

Owned Music source admission now passes268 checks/19 refusals and verifies41
original script/map/INI/Music files unchanged. It includes same-map unload/reload
without retiring the baseline Music export and complete final GC cleanup.
The final snapshot is music-source-final-frozen-20261030.json (1864 inputs).
Its full desktop/CTest and actual expected-boundary Training repeat2 are running;
no final regression or repeated-startup pass is claimed yet.

## Final verification

Final full desktop build and CTest pass:244 executed/297 registered/53 explicit
owned-data skips/zero failures137.94s. Strict ARM64 and Android-native builds
pass on unchanged native inputs; only the owned test file changed afterwards
to use actual PlayerStart and restore its pre-birth checkpoint. The final owned
test268/19 passes on the final snapshot.

Both expected-boundary Training runs return from TravelPostAccept, PostLogin
and the original SpawnPlayActor tail, then reach the exact existing original
post-partition client/player/detail setup refusal at266870 instructions/
37932 writes (284.696s and270.417296s). Full rollback/43 unchanged original
sources and independent cold Model ownership pass. These are BOUNDARY
DIAGNOSTICS ONLY, not successful or playable startup or publication.

Final audit:1864 compiler inputs match length/UTC timestamp/SHA256;45 baseline
originals are unchanged; the original Music UMX hash matches the value above;
no compiler/test process remains live. Discovery and corrected-fixture failures
are retained as NOT passing gates. No new APK is packaged, no live Quest
crate/loot/gravity acceptance is claimed, and no other map was integrated.

The next actual LoadMap continuation is documented in
ORIGINAL-POST-PARTITION-CLIENT-SETUP.md: non-Entry brush tracker construction,
subsystem rebind, live viewport input reset/render handling and original
DetailChange callbacks. Never erase that guard merely because SpawnPlayActor
now returns. Native Flag/FlagInt/archive/travel, live input/world/frame/render
publication and full-campaign acceptance remain requirements of the full goal.
