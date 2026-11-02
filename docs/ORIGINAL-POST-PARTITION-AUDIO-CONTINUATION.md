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

Artifacts input-read-audio-*-20261102.log/json retain the exact export/vtable,
bounded bodies and before/after source fingerprints. No renderer/audio
capability, original Init call site or later Reset/detail service was added.
