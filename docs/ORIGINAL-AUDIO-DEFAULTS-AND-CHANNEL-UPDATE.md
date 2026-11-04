# Audio defaults and channel consumption: source continuation

Static source audit2026-11-04 for the pending concrete Quest audio provider.
This adds no provider, native sound-channel playback, committed startup, crate
gameplay or APK. External GNU objdump read owned commercial DLLs strictly as
data. No commercial DLL/EXE was loaded or executed; no compiler inputs changed.

## Source identities

Before/after SHA256 values agree:

| Source | SHA256 |
| --- | --- |
| Core.dll | 8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4 |
| Engine.dll | 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4 |
| Galaxy.dll | 606E71D13BBD3C4DB2460AABCF60D87B1C79BB49D77725CF9E087513C7D06353 |

Addresses/offsets below identify the original preferred-image PE32 source,
never portable C++ field layouts or authority to load a Windows audio driver.

## Fresh SLOT_None counter: default initialization, not a guessed zero

The previous bounded Galaxy constructor/Init reads did not find an explicit
write to subsystem+8FC. The missing origin is now traced through class defaults:

1. Galaxy's native class initializer10604910..10604977 supplies actual native
   size904, base Engine.AudioSubsystem and Within Core.Object to Core's native
   UClass constructor at10604962. It records InternalConstructor106011BD and
   StaticConstructor106011A4, not an invented abstract Audio instance.
   The actual base registration10371000..10371080 supplies size2C. This agrees
   with the earlier native config-class registration proof.
2. Core's native UClass constructor10102D9C ->10128630 initializes its default
   array pointer/count/capacity to zero at101286F2..101286FE. Its UStruct native
   constructor1010126C ->10125EF0 stores the supplied property size at+3C
   (10125F47). The actual UClass virtual+64 cell10175958 ->101026E4 ->1010F7C0
   returns that size; it is not a guessed struct extent.
3. UClass.Register10101E74 ->10127800 sizes the actual default byte array to
   that property size and calls UObject.InitClassDefaultObject10101226 at
   101278A2. The latter's body10152300 supplies the actual parent Class as the
   defaults source to InitProperties, with the complete derived size.
4. InitProperties1010102D ->101520D0 uses source defaults when present, copies
   their post-header bytes, then zeros the remaining tail. The copied prefix
   begins at28; the zero tail is explicit REP STOS at10152185..101521A7.
   Thus the fresh Galaxy default's native tail after the2C-byte parent includes
   the complete counter word at8FC and initializes it to0. The static constructor
   registers reflected audio settings, not a counter field at8FC; config import
   of those fields does not manufacture a counter initializer.
5. For null-template fresh instances, StaticAllocateObject10102EC3 ->101579C0
   calls that same InitProperties at10157EF1 with the actual Class/default
   source. StaticConstructObject10101771 ->10157FF0 calls it at1015804D BEFORE
   the class's genuine InternalConstructor callback at1015805C. Galaxy's
   InternalConstructor106011BD ->106043B0 delegates its real constructor
   10601226 ->10604A20; the audited constructor does not overwrite8FC.

This establishes initial counter0 for the fresh registered-default/null-template
profile. It does NOT reset a copied/template instance's inherited counter,
prove arbitrary modified defaults, or authorize a counter reset on every map
load. SetViewport and the checked Init body do not reset this counter. A fresh
SLOT_None request decrements it before shifting, so its first generated DWORD
Id isFFFFFFF0 (Int-16), not an Actor's ordinary fixed channel. The entire incoming
Id is replaced, including the no-override bit. With valid viewport/sound, the
decrement precedes channel selection, even if a genuine configured zero channel
count leaves no eligible slot. Counter mutation still belongs in the VM journal.

## Source-created record fields

PlaySoundW1060823F..106082AC constructs the selected replacement after StopSound:
handle0, Actor/Id/Sound, supplied Location/Volume/Radius/Pitch, initial calculated
priority, Float elapsed0 at+30, two additional explicit zero words at+0C/+34,
Float-1 at+38 and zero at+3C. The sixteen-DWORD copy includes the whole record.
Only authenticated semantic fields should become portable state; this is not
permission to copy PE32 records or treat every StopSound temporary padding byte
as a semantic default. The earlier StopSound/NoteDestroy contract still applies.

## Update is a real consumer, not just a Play command queue

Galaxy Update export10601136 ->10609510 has separate ambient admission,
ambient maintenance and ordinary live-channel loops. This audit retains its
full bounded owner body10609510..1060AC20 but establishes only the operations
below; it does not claim every spatial/music/backend branch is specified.

- The Update prefix uses an actual cycle-time/seconds source (including the
  imported appSecondsSlow), not reflected Level.TimeSeconds or the actor's
  dilated delta. At10609595..106095E8 it spills the difference from its prior
  Double time, updates that prior time, and bounds the consumed delta to[0,1]
  using authenticated Double0 at1063D538 and Double1 at1063C0E8. A future adapter
  must not silently replace this with paused/dilated actor time. Exact clock
  provider/precision and its transaction ownership remain integration work.
- The actual live count at+64 bounds ascending loops, independently of the
  constructor's32-record capacity. Ambient slots are recognized by `(Id&0xE)==8`.
  Maintenance106097F0..1060997E requires an Actor, checks its current location,
  radius/current AmbientSound and the earlier captured eligibility gate,
  updates volume/radius/pitch from current actor fields, or stops that slot.
  Ambient admission can synchronously call actual audio virtual+74 PlaySound.
- The ordinary loop106099B9..1060A497 checks nonnull owner validity before
  selecting active Id!=0. It does not select active channels by Sound or handle
  alone. For an existing backend handle,10609A1E..10609A34 queries backend status;
  a false result calls StopSound(index), increments the index and immediately
  continues, without this channel's elapsed/location/spatial work.
- Otherwise elapsed advances at10609A3C..10609A47. A retained nonnull Actor
  supplies its current Location at10609A4A..10609A61; Actor=None preserves the
  cached Location. Therefore NoteDestroy's nonambient detach must not blank the
  position or stop the voice: the destruction one-shot can finish detached.
- Listener position selects stored Viewport.Actor or its nonnull ViewTarget.
  Priority is recomputed from distance, radius and volume at10609AA7..10609B0E;
  the later priority is not frozen at PlaySound selection. This matters for
  subsequent channel replacement when a source/listener moves.
- The actual Sound resource is asserted nonnull and registered on demand at
  10609FDE..1060A00C when its backend resource field is absent. This is not
  package decode inside a real-time callback. Existing voices take update
  branches; absent handles reach real backend creation at1060A3AC..1060A43D,
  including primary creation10612800 and fallback10612310. Their returned handle
  is stored in the channel. A queue with no owner voice consumer is not playback.

Backend status-query flags, sample/resource structures, spatial volume/pan,
frequency/doppler, smoothing, renderer interaction and all fallback/error
contracts still need their own exact source/adaptation work. Imported helper
names and offsets are not guessed from those call sites. No Windows backend
function is a Quest implementation.

## EffectsChannels and initialization scope

The earlier audit already proves Config Int EffectsChannels at+64 and
PostEditChange's signed[0,32] clamp. UClass.Register now proves ordered default
initialization, StaticConstructor, class linking, LoadConfig then LoadLocalized.
Core.LoadConfig10150D60..101514E2 imports reflected config fields; neither this
bounded path nor ConditionalPostLoad1014FF70 proves a call to the concrete
Galaxy PostEditChange before Init. Do not relabel LoadLocalized1010306C as that
clamp. The current owned INI's16 fits the capacity, but arbitrary out-of-range
initial configuration must not be silently normalized under an unproved call
sequence. A Quest platform profile must define its admission separately and
preserve genuine configured0 rather than inventing it to disable output.

## Evidence and next implementation

Ignored bounded logs use the prefixes native-audio-core-*,
native-audio-galaxy-class-*, native-audio-baseclass-registration-*, and
native-audio-update-full-owner-* under artifacts. DLL hashes were reread after
inspection. Compiler inputs and protected game files remain unchanged; no new
build/tests/APK were needed or claimed for this source-only continuation.

The provider still needs a distinct concrete Quest Class/UObject, real ready
endpoint binding, rooted bounded channel COW (including the counter), prepared
sound assets, commit-reserved Stop/Detach/Play publication and an actual owner/
mixer consumer. Decode/admission must precede the last fallible config boundary;
rollback must leave audible output untouched. Connect real LoadMap Get/Set,
DestroyActor NoteDestroy and ClientHearSound together. These source findings do
not justify publishing an empty or inaudible provider as completed audio.
