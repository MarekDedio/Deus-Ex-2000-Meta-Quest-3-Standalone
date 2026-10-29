# Original Training Music asset identity: next dependency (2026-10-30)

Source contract, not an implemented asset-admission service or completed
startup/audio-subsystem claim. Only the port's package tools and static native
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
