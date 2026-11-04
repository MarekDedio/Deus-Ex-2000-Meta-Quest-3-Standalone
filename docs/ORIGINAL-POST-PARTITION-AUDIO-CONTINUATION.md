# Post-partition audio: bounded source continuation

Static prerequisite audit2026-11-02 while the Input/ReadInput batch is frozen.
Only original first Training integration is in scope now. This is NOT a new
audio implementation, initialized subsystem, completed startup or APK gate.
Original DLLs are inspected as data by external objdump, never loaded/executed.

The earlier LoadMap audit identifies1038DFC6..1038DFDB after non-Entry brush
construction: if actual Engine.Audio(+4C) exists, call its virtual+84, then
pass that return to the same subsystem's virtual+5C. The subsequent live
viewport Input.ResetInput/render-device loop and DetailChange remain distinct.

## Exact selected desktop source identity

Owned DeusEx.ini selects Galaxy.GalaxyAudioSubsystem. Galaxy.dll SHA256 is
606E71D13BBD3C4DB2460AABCF60D87B1C79BB49D77725CF9E087513C7D06353;
before/after inspection hashes agree. Original Engine.dll is the separately
protected0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4.

Engine's abstract UAudioSubsystem UObject vtable is104294C4 (export EAT695,
RVA1294C4). Its+5C/+84 entries are purecall104135CA. A constructed abstract
Engine.AudioSubsystem is not a concrete provider for the selected operation.

Galaxy image base10600000; its UObject vtable is1063D0E4 (EAT7/RVA3D0E4).
The actual+5C cell1063D140 contains106010DC, exported SetViewport thunk
(EAT29/RVA10DC), jumping to10605C40. The+84 cell1063D168 contains1060119A,
exported GetViewport thunk (EAT14/RVA119A), jumping to10606190. GetViewport
reads the actual subsystem's viewport at+7C and returns it.

## Same-viewport SetViewport is not empty

SetViewport10605C40 logs the supplied viewport's actual Name, then walks its
live sound-channel count at+64. For each index it calls1060125D, authenticated
by EAT35 as StopSound(int), BEFORE comparing current viewport+7C with the new
argument. Only after all channel stops does10605CB6..5CBB branch to the return
when they are equal. Thus LoadMap's GetViewport/SetViewport pair cannot be
implemented as a no-op merely because it passes the same pointer.

Changed-viewport branches include resource/driver teardown, storing+7C,
configuration/format selection, real output initialization and Music/Sound
registration. Those branches are not fully audited or implemented by this
bounded continuation. This is not permission to copy PE32 offsets/layouts
into portable objects or instantiate Galaxy/Glide drivers on Android.

## Concrete next integration requirement

quest_main.cpp has a real AAudio stream/callback and existing music/dialogue
playback. Those alone are not an authenticated Engine.Audio object/provider
or implementation of the original subsystem interface. A distinct explicit
Quest audio adapter must bind to the current real client/viewport and actual
audio-owner lifetime. Its native object/Class, initialization, teardown and
supported virtual profile need their own ownership evidence; do not infer a
successfully initialized disabled subsystem from constructor Audio=None.

Original script/world mutations and audio publications must remain one logical
transaction: stage bounded channel-stop/rebind commands, validate actual
provider lifetime/capacity before publication, and do not stop audible output
for an ultimately rolled-back startup/frame. The CPU runtime must not access
AAudio or render-owner handles just because viewport metadata exists. Existing
accepted Training music/dialogue must survive unrelated input work.

## Concrete existing bootstrap and commit seams

Historical pre-endpoint review confirmed StartAmbientAudio was called
only from CompleteInitialPreparation AFTER the exclusive runtime worker's
PrepareInitialRuntime had returned. A real audio capability needed during
original startup therefore could not be supplied by that late call. The required
split separates actual device initialization from content attachment: open/start a real initially
silent48kHz stereo/I16 stream on its owning thread, validate negotiated format,
then hand a revocable ready endpoint to the CPU worker before original startup.
Attach prepared ambience/music/dialogue later without closing the endpoint.
An initialized silent output is not a fabricated disabled/absent subsystem.
The real endpoint-first split is now implemented by b6fdc6f; see
QUEST-AUDIO-ENDPOINT-STARTUP.md for the current code and verification. This
does not implement the native Audio UObject/channel interface below.
Readiness must come from successful real AAudio open/start, not just a nonzero
audioSampleRate_: the historical StartAmbientAudio assigned that field before
builder creation, which could still fail. Audio disconnect/error must revoke its own generation;
an otherwise live client/render session does not prove the stream remains live.

The proposed endpoint needs its own audio generation/lifetime in addition to
the exact current NativeClientLifetime and client/viewport identities, bounded
live voice/channel inventory and a reserved commit queue. No AAudio/userData/
GL/XR handles cross to the CPU runtime. Same-viewport SetViewport must stage
stops for actual source channel slots before equality, without using the broad
StopAmbientAudio teardown or restarting unrelated accepted music content.

PortableActorVmHost::Commit currently performs all fallible world/mesh/lease
preparation before PublishNativeUserConfig, its last fallible boundary.
Audio queue capacity/generation validation and reservation must finish BEFORE
that boundary. Publish an already-prepared batch afterward without throwing;
rollback cancels the reservation and native COW changes without stopping any
currently audible output. The audio owner drains committed bounded batches;
decode/join/destruction work stays off the real-time callback.

Binding an actual nonnull Engine.Audio also makes the existing DestroyActor
audioActorDestroyed provider reachable; it currently explicitly refuses.
That notification must be source-backed and implemented along with the audio
adapter. ClientHearSound similarly still refuses original audio publication.
Neither missing operation may be hidden by forcing Audio=None. These are
concrete implementation requirements, not claims that the proposed endpoint
or original subsystem has already been implemented or accepted on hardware.

## DestroyActor notification and audible destruction sounds

The exact Engine caller1039609F..103960AC loads Level.Engine, then actual
Engine.Audio and calls nonnull virtual+7C after slot removal/pending-delete but
before ConditionalDestroy. Galaxy vtable1063D0E4 cell1063D160 selects exported
NoteDestroy thunk10601203 (EAT18), targeting106083A0..10608485. The abstract
Engine.AudioSubsystem slot is purecall, not a usable default implementation.

NoteDestroy requires a nonnull UObject-valid Actor. At this point pending
delete/world-slot removal is legitimate; a future audio adapter must not reject
the identity merely because bDeleteMe or worldRemoved is already set. It walks
the live channel count ascending without compaction or active-handle filtering.
For a matching channel Actor owner, source Id mask `(Id & 0xE)==8` calls actual
StopSound(index). Otherwise ONLY the Actor ownership pointer is cleared:
sound, Id, cached position and playback are preserved. This is how nonambient
destruction one-shots can finish after their source actor is removed; do not
blanket-stop every sound owned by a destroyed crate/NPC.

StopSound thunk1060125D (EAT35) targets10607E50..10607ECD. It conditionally stops
the backend handle, then replaces the channel record even when no handle was
present. Do not infer an all-zero64-byte portable default: the source has raw
temporary padding and explicitly writes field+38 to Float-1. Only separately
authenticated semantic defaults should be represented, not guessed PE layout.

A real Quest adapter therefore needs native channel-ownership COW plus staged
Stop/Detach operations, with exact order and rollback-reserved publication.
The audio thread consumes opaque lifetime-safe voice identities/data, never
RuntimeObject pointers. Main independently retained bounded caller/body source
in artifacts/input-reset-runtime-audio-*-20261102.log; a separate read-only
source review confirms this contract and unchanged owned source hashes.

## PlaySound channel selection and the actual native packet (2026-11-04)

Further external objdump reads authenticate Galaxy export index19/ordinal20,
PlaySoundW thunk10601221 ->10608100..10608308. Its actual UObject vtable slot+74
contains that thunk. Engine APawn.execClientHearSound export index1883/ordinal1884,
10302752 ->103DFC40..103DFDFB, calls actual Engine.Audio virtual+74 at103DFDE8.
Both source DLL hashes above remained unchanged. No commercial code was run.

ClientHearSound evaluates its five inputs, then decodes Parameters.X/Z with
Double0.01 and individual Float spills before receiver eligibility checks.
Parameters.Y supplies Radius. Actual non-PlayerPawn, Player=None, non-Viewport
Player, and Engine.Audio=None are legitimate source exits, not permission to
manufacture an absent Quest provider. An eligible source Actor with UObject
pending-kill flag0x80 becomes None; reflected bDeleteMe is not that flag.
Radius==Float0 becomes1600; other values are not normalized by this wrapper.
The provider receives Actor/Id/Sound/unchanged Location/Volume/Radius/Pitch;
its Int return is ignored by the callback.

The concrete Galaxy selection is not simply "take a free voice":

1. Assert Radius!=0 before the Viewport=None or Sound=None early return0.
2. For SLOT_None `(Id & 0xE)==0`, decrement a subsystem counter and replace
   the entire Id with DWORD(counter<<4). Its initial value remains unaudited.
3. Use actual Viewport.Actor, or its nonnull PlayerPawn.ViewTarget, for listener
   position. Initial priority is `(1-distance/radius)*volume`, without clamping.
   The original x87 accumulation/spill/comparison precision is significant;
   a host Float implementation is not yet an authenticated equivalent.
4. Scan live slots ascending. An Id match ignoring bit0 wins immediately,
   except incoming bit0 forbids that override and returns0. Otherwise a running
   priority >= stored Float priority selects that slot and lowers the running
   threshold to its priority. Later ties win; a later same-Id still overrides
   an earlier priority candidate. No backend-active filter is applied.
5. If no candidate, return0. Otherwise StopSound(selected) first. Internal
   Sound=-1 means stop-only and return1. Ordinary Sound fills semantic fields
   with the INITIAL calculated priority, not the minimum scan threshold, and
   starts with empty backend state and the authenticated Float-1 field.

This body does not start playback. The later Update/backend path must consume
the channel inventory. StopSoundId10601186 ->10607EF0 stops only the FIRST
matching Id slot (ignoring bit0), not every match. Constructor10601226 ->10604A20
explicitly initializes32 records, but the separately configurable live count
and SLOT_None counter initialization are not established by that constructor
read. The fixed constructor capacity is not a license to invent the live count.

A further bounded read identifies EffectsChannels as the actual native Config
IntProperty at+64 (StaticConstructor10604BB0, property construction10605177..51AF).
PostEditChange10605AD0 clamps signed count to[0,32] at10605B22..5B40. Init106056DB
reads that count for backend1060EBC0 without doing that clamp itself. The complete
config/default/post-load invocation chain remains unestablished; a clamp's
presence does not prove it ran before Init. The owned INI selects16 channels.
No explicit initial counter+8FC store was proved in constructor, Init,
StaticConstructor, PostEditChange, SetViewport or the bounded Update read.
Init's zero block ends before that counter. Its source allocation/default-tail
initialization must be authenticated rather than guessing counter0.

Follow-on source evidence now closes the fresh default/null-template counter
profile and the ordinary channel-consumer skeleton. See
ORIGINAL-AUDIO-DEFAULTS-AND-CHANNEL-UPDATE.md: the parent-default copy/zero-tail
chain establishes fresh counter0, while Update independently retires completed
handles, advances audio-clock elapsed, follows retained Actor locations and
preserves detached one-shot positions before real voice creation. Arbitrary
template/copy state, full spatial/backend semantics and concrete Quest provider
publication remain separate. The preceding unknown-counter statements describe
the earlier constructor-only audit, not a requirement to guess its value now.

Ignored bounded evidence: native-audio-playsound-contract-20261104.md and
adjacent native-audio-playsound-*-20261104 logs under artifacts. Remaining source
work includes actual Update voice start/completion/owner following/spatial gain,
pitch/resource handling, configured EffectsChannels/counter initialization,
changed-viewport resource/init/failure behavior, and exact priority math. This
audit is not a provider, audible crate effect, startup completion or new APK.

Artifacts input-read-audio-*-20261102.log/json retain the exact export/vtable,
bounded bodies and before/after source fingerprints. No renderer/audio
capability, original Init call site or later Reset/detail service was added.
