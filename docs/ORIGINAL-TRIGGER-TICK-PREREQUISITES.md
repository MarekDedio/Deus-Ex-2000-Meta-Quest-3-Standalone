# Original Engine.Trigger Tick prerequisites

Read-only owned GOTY audit, 2026-10-25. This records the next authenticated
native boundary after the first Training dynamic phase reached
`Original Actor Tick reached an unimplemented native virtual override: Engine.Trigger`.
It does not implement that admission, finish the dynamic phase, or claim a
successful first Training frame/device test. No compiler input was edited
and no build was started for this audit.

Only external objdump/static bytes were used for native code; a read-only
BinaryReader inspected the owned package's tables/Class payloads. No commercial
DLL/EXE was loaded or executed. Owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Owned Engine.u SHA256:
`1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA`.
Addresses are preferred-image virtual addresses, not portable layout.

## Exact exported identity and constructors

The Engine.dll name/EAT table identifies:

| Name index | Original export | RVA / preferred address |
| --- | --- | --- |
|675|`??_7ATrigger@@6B@`|12864C /1042864C|
|78|`??0ATrigger@@IAE@XZ`|388C /1030388C|
|1198|`?InternalConstructor@ATrigger@@SAXPAX@Z`|10C3 /103010C3|
|1390|`?PrivateStaticClass@ATrigger@@0VUClass@@A`|276088 /10576088|
|636|`??_7AActor@@6B@`|1268DC /104268DC|
|1|`??0AActor@@QAE@XZ`|3431 /10303431|

These are zero-based name/EAT indexes, not export ordinals. ATrigger is
distinct from ATriggers (vtable name677 RVA1285A0) and ATriggerMarker
(name676); their English names must not be conflated.

The actual ATrigger default constructor thunk1030388C jumps to
10341230..103412E4. At103412CF it writes literal1042864C into the object's
vptr. InternalConstructor thunk103010C3 jumps to10341210; a nonnull object
argument is passed through the same default constructor. These independently
corroborate the exported vtable's association with the real native class.

AActor's exported default constructor thunk10303431 jumps to10323550;
103235B0 writes literal104268DC into its vptr. No vtable addresses were
inferred from an unauthenticated replacement-engine header.

## Actual authored native ancestry

The owned Engine.u v68 export table and first UField Class links give the
following original one-based object references (all ObjClass0, Outer0):

| Class | Reference | ObjFlags | ObjBase / payload UField base | Payload bytes / offset |
| --- | --- | --- | --- | --- |
|Engine.Trigger|162|040F0004|215 /215|436 /87842|
|Engine.Triggers|215|040F0004|2 /2|106 /91247|
|Engine.Actor|2|040F0004|-3 /-3|1236 /61225|

Actual package name indexes4,1382,456 independently resolve respectively to
Trigger, Triggers, Actor. Trigger's initial Class links are base215, next0,
source5297, children2738; Triggers' are base2, next0, source5292, children0.
This is native Trigger to native Triggers to Actor ancestry, not a guessed
direct Trigger-to-Actor link or merely inherited ClassFlags. RF_Native is the
actual export ObjFlags bit04000000.

## Requested slots are exactly AActor implementations

Static DWORD reads of both exported vtables give:

| Virtual service | ATrigger cell | AActor cell | Same raw pointer | Thunk target |
| --- | --- | --- | --- | --- |
|ProcessEvent+10|1042865C|104268EC|10302D74|10369100|
|ProcessState+14|10428660|104268F0|10301F00|103E5B60|
|Tick+64|104286B0|10426940|10303DAA|103A1AA0|
|performPhysics+8C|104286D8|10426968|10303242|103C9770|

The names/EAT independently identify10302D74 as exported
`AActor::ProcessEvent` (name1441),10301F00 as `AActor::ProcessState`
(name1443),10303DAA as `AActor::Tick(Float,ELevelTick)` (name1667), and
10303242 as `AActor::performPhysics(Float)` (name2056). Each direct JMP
target in the table was statically disassembled. They are identical raw
vtable pointers, not merely similar-looking bodies or guessed import aliases.

Therefore the named error does not establish that Engine.Trigger has a
distinct native Tick override. For this owned source, all four requested
virtual entries select the existing AActor bodies. Existing actual script
state/event selection remains required inside those bodies.

As a separate corroboration for its actual superclass, exported ATriggers
default constructor name82, RVA1429, thunk10301429 to10341070 installs its
distinct vtable104285A0 at103410EA. That vtable's+10/+14/+64/+8C cells
104285B0/104285B4/10428604/1042862C also contain exactly the same four AActor
pointers. This does not collapse the two native Class identities.

## Bounded integration consequence

The current production gate is
`portable_actor_tick_tail.inl::RequireOriginalActorTickPhysicsImplementation`.
It walks genuine authored class ancestry, stops at the nearest Class export
carrying RF_Native0x04000000, and accepts only individually audited exact
native paths. The next implementation increment can add **exact
Engine.Trigger** to that native-boundary authority without replacing its
Tick, ProcessState, ProcessEvent or performPhysics with new behavior.

This is not permission to accept any Actor/Trigger-derived native class.
A different nearer RF_Native boundary still requires its own vtable audit;
ordinary script subclasses continue through their genuine metadata and
callbacks. Do not bypass the native-boundary check, skip this actor, or mark
an unsupported phase as completed.

ATrigger's destructor slot+0C is103014EC, whereas AActor's is10301479.
This audit does NOT authorize scalar teardown, destruction, or unrelated
virtual services from their equality in the four requested slots. It also
does not broaden Physics-mode support, timer/event support, world startup,
collision queries or production transaction ownership.

The immediate scope remains the whole first `00_Training.dx` dynamic phase
and its crates/loot/gravity, not other maps or any account/login interface.
The current build freeze is preserved; native-boundary admission and exact
serialized-fixture/regression checks belong to a separately thawed increment.
