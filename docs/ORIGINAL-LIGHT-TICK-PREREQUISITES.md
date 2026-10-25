# Original Engine.Light Tick prerequisites

Read-only owned GOTY audit, 2026-10-25. This authenticates the next exact
native boundary reached by the unfiltered first Training dynamic phase after
DeusExLevelInfo admission. It is not a blanket native-Light allowance,
lighting/render implementation, completed frame, gameplay or APK claim.
No compiler input was edited and no build started for this static audit.

Only external objdump/static bytes plus a read-only BinaryReader of the owned
Engine.u tables/Class links were used. No commercial DLL/EXE was loaded or
executed. Addresses are preferred-image virtual addresses, not portable layout.

| Owned source | SHA256 |
| --- | --- |
|Engine.dll|0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4|
|Engine.u|1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA|

## Actual Class identity and ancestry

Owned Engine.u v68 Light Class export630 / original reference631 has
ObjClass0, ObjOuter0, ObjFlags040F0004. RF_Native04000000 is actually set on
that export, not inferred from inherited ClassFlags or a leaf name. ObjBase2
names actual Engine.Actor. Its Class payload147 bytes at103546 independently
begins UField base2, next0, source5420, children0.

Actor Class reference2 has ObjClass0, ObjOuter0, ObjFlags040F0004, ObjBase-3,
payload1236 bytes at61225; its initial UField base is likewise-3. The relevant
actual chain is Engine.Light to native Engine.Actor to Core.Object. Another
nearer RF_Native subclass must not be bypassed merely because this ancestor
was authenticated.

## Exported native vtable and constructor proof

The owned Engine.dll name/EAT table gives these zero-based indexes (not
export ordinals):

| Index | Exact native export | RVA / preferred address |
| --- | --- | --- |
|656|`??_7ALight@@6B@`|1288FC /104288FC|
|40|`??0ALight@@IAE@XZ`|1230 /10301230|
|1179|`?InternalConstructor@ALight@@SAXPAX@Z`|2A54 /10302A54|
|1371|`?PrivateStaticClass@ALight@@0VUClass@@A`|27C8F0 /1057C8F0|

The actual default constructor thunk10301230 jumps to10344600..1034468F.
At1034467A it installs literal104288FC into the object's vptr, independently
corroborating the exported vtable's association with ALight. InternalConstructor
thunk10302A54 reaches103445E0; a nonnull object argument is sent through the
same constructor at103445E8. These are not vtable identities guessed from
the separate ToWorld audit, although that audit's address agrees.

## Four actual slots are exactly AActor implementations

Direct static DWORD reads of ALight and AActor exported vtables give:

| Service | ALight cell | AActor cell | Identical raw pointer | Actual body |
| --- | --- | --- | --- | --- |
|ProcessEvent+10|1042890C|104268EC|10302D74|10369100|
|ProcessState+14|10428910|104268F0|10301F00|103E5B60|
|Tick+64|10428960|10426940|10303DAA|103A1AA0|
|performPhysics+8C|10428988|10426968|10303242|103C9770|

The names/EAT independently identify Engine name1441 as AActor.ProcessEvent,
name1443 as AActor.ProcessState, name1667 as AActor.Tick(Float,ELevelTick),
and name2056 as AActor.performPhysics(Float). Their corresponding direct JMP
thunks were statically disassembled to the bodies above. The ALight slots
are exactly those raw pointers, not similar replacement bodies or an
inheritance-based analogy. No distinct ALight override exists for any of
these four requested virtual services in the owned Engine.dll.

This does not mean selected script Tick/state/events are empty or can be
discarded. The existing genuine AActor dispatch still selects them on the
actual Light receiver with the same original scheduling/physics rules.

## Bounded next implementation consequence

The reached error
`Original Actor Tick reached an unimplemented native virtual override: Engine.Light`
identifies an unauthenticated-path gate, not evidence of a distinct Light Tick
body. The static proof supports admitting **exact Engine.Light** at the
existing nearest-RF_Native Tick/physics boundary in a later implementation
step. Unknown nearer native children, arbitrary same-leaf classes and other
native ancestors remain explicit failures, not skipped actors or empty phases.

ALight destructor slot+0C is10303F3A, whereas AActor's is10301479. No scalar
teardown/destruction authority, deletion-filter relaxation, other virtual
service, renderer light behavior, active physics capability or world startup
completion follows from these four equal slots.

Any admission requires its own serialized native Light to Actor ancestry,
selected script Tick/Physics0 and shared late/write/work rollback controls,
plus another unfiltered first Training observation. This audit changes only
this new document. It does not authorize integrations for other maps or
establish finished crates, loot, gravity, full Tutorial, or headset testing.

## Subsequent exact-class implementation increment

After the audit's four raw-slot/constructor/Class identities were independently
confirmed and the root approved this same increment, the native boundary
gate was extended with exact Engine.Light only. No generic native Light-child
allowance, scalar teardown, deletion rule, renderer or brightness behavior was
changed.

The serialized production fixture now supplies genuine RF_Native Light to
Actor Class ancestry and checks its raw ObjClass/Outer/ObjFlags/SuperField
metadata, actual dispatch ancestry, selected script Tick with Physics0,
unknown nearer native Light-child refusal, late selected failure, every write
prefix and representative native-work cuts with complete same-root/save/parity/
revision/GC undo. The pre-existing Trigger and new DeusExLevelInfo controls
remain distinct. These added controls are not claimed as built/passed by this
note; actual build/test and unfiltered Training observations are recorded by
the root separately.

Final combined verification2026-10-26 passed full desktop, strict ARM64 syntax
and Android-native builds. Light production controls log2631 cumulative checks/
63 refusals; complete driver exits0 after extended Physics93740/89 and its
final malformed-schema case. CTest205 executed/244 registered/39 explicit
owned-data skips/zero failures65.03s passes. Actual Training now enters the
real selected TriggerLight.Tick rather than refusing native Light; both
ordinary repeats reach52 actor calls and pc6/native172 Non-finite VM float,
205893/15721 and full undo. Paired original door movement/events remain intact
before the same later script failure at206229/15789. No brightness behavior,
complete frame or headset claim follows; the remaining original scalar
behavior is documented in TRAINING-TRIGGER-LIGHT-TICK-GAP.md.
