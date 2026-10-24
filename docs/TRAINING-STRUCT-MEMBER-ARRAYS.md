# Original fixed arrays inside structs

First-Training player startup formerly stopped while initializing the original
DamageHUDDisplay because its sIconInfo.DamageType is a fixed Name[3] member.
This is missing reflected storage/addressing, not a request to replace the
original HUD graphics or add account/character creation UI. It must be fixed
for the normal genuine player/weapon path to be committed for live use.
Crate/loot/cohort diagnostics remain separate same-root evidence, not permission
to publish an incomplete player startup.

## Owned source and serialization evidence

Only immutable local original packages/static bytes are inspected. No original
commercial EXE/DLL is executed or loaded. Owned DeusEx.u SHA256 is
257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480;
Core.dll is8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4.

Actual sIconInfo export12315 has Children897: DamageType NameProperty dim3,
Icon ObjectProperty dim1, bActive Bool, initTime/hitDir Float, bHitCenter Bool,
Color StructProperty dim1. DamageHUDDisplay.iconInfo export80 is StructProperty
dim7 referencing that struct. Original InitWindow's first member-array write
is iconInfo[0].DamageType[0]='Shocked'; its ArrayElement/StructMember expression
reaches the former schema refusal at normalized PC39. Later original writes
initialize all seven icon records and their three DamageType slots.

Static original Core.dll UStruct::SerializeBin10126683..101266A6 initializes
the element index to0, compares against Property.ArrayDim(+34), computes
ElementSize(+38)*index+Offset(+48), invokes SerializeItem at virtual+78 and
increments the index before the next field. Fixed member arrays therefore
contain every element consecutively in declaration order, without an array
length or per-element property tags. Loading only element0 is not compatible.

## General representation and addressing

For ArrayDim1 the existing scalar/nested value is unchanged. For an authored
member with ArrayDim>1, its ordinary field owns an internal Kind::Struct
envelope with canonical decimal keys0..N-1 and the existing typed element
values. Only real reflected arrayDimension authorizes this representation;
decimal-looking maps do not themselves become script arrays. No new VM value
kind or checkpoint version is introduced, and field names are not flattened.

Authored-struct validation must preaccount the envelope, every repeated
element/nested schema, keys/depth/nodes/bytes before construction/decoding.
The decoder reads the complete original field-major element stream and uses
the VALUE package's name/object resolver for each slot. Specialized canonical
Vector/Rotator layouts remain three scalar channels. Unsupported legacy
StringProperty/dynamic-array contracts remain explicit refusals.

StructMember opcode0x36 retains the element typed zero/result layout and
reflected dimension, with real per-slot references for existing ArrayElement
index evaluation/clamping. Reads and writes unwrap only the selected slot;
unindexed access denotes element0, never the internal envelope. A write
rereads its fresh parent before replacing one element, preserving sibling
changes made by RHS evaluation or OUT callbacks. Temporary values retain
owned lifetime without acquiring a writable persistent alias. Missing address/
NoWrite/native result-layout rules remain independent of stored value shape.

Runtime live assignments and checkpoint preflight reuse each element's real
declaration, including every nested Object/Class ancestry constraint. Existing
recursive Struct shape/save codecs enforce exact slot count/names/kinds and
retain all slots. Nested owning payload cleanup additionally carries a slot
index at each member-address depth, preserving the original ConstructorLink
field-major/outer-element order, with the same execution journal and rollback.

## Verification state

The first actual original
Training startup now passes the former DamageType schema refusal and reaches
DeusEx.DeusExHUD.ConfigurationChanged PC606/opcode101, unsupported native1463
(QueryPreferredWidth). It records218693 instructions/24284 writes and492818
native work of1048576; full save/state/revision/hash/RNG/client/lookup/Actor/GC
rollback and all41 unchanged source fingerprints pass. This discovery returns2,
not successful player startup. Evidence:
artifacts/training-struct-arrays-startup-discovery-20261024.log.

Codec tests pass712 checks/253 rejections. The full VM suite passes695885
checks/867 rejections; the new fixed-member cases account for122 checks/21
refusals including native316 copy-once, guarded lifetimes, inactive addresses,
read-only temporaries, fresh-parent OUT and every exercised write prefix.
Generated production assignments/checkpoint controls pass737 checks/21
refusals, including every constrained Object array slot and malformed save
shape/type/path rejection. Genuine registered-native-metadata-backed nested
StrProperty cleanup passes259 checks/11 refusal controls: Records[2] containing
Children[2] containing Text[2], all eight payload stores, every write prefix,
native-work cuts and full save/revision/GC undo. Evidence: generated-retest log.
Strict ARM64 runtime/VM/codec/generated and final player-fixture syntax pass.
The final Android native compilation/link passes; no APK is implied.

Fresh actual player controls pass6877 checks, including all seven original
iconInfo records, all21 typed DamageType names, six owning nested texture GC
edges, original colors/default flags, the remaining132-object graph and late
write8874 rollback. Both direct root initialization and isolated Possess now
retain native1463 as their genuine next boundary. The isolated fixture prepares
real native lookup/Engine collision services, matching production, rather
than ignoring missing authority during CreateKeyRing/BecomeItem. Its cold
original ring graph/checkpoint and native session controls also pass. Only
first Training is loaded: compass cache invalidation unloads/reloads Training
and checks the fresh native generation instead of loading another map.
Evidence: artifacts/training-struct-arrays-player-final-20261024.log.

Fresh actual first-Training ConstructorLink cleanup passes1164 checks/33
refusal prefixes with39 source fingerprints unchanged. Fresh original dormant
crate controls pass347/two contents refusals; the focused owned first-map
SetPhysics byte/Base/vector/save/rollback controls also pass. These remain
their separately described scopes, not live melee or full startup evidence.
Logs: training-struct-arrays-owned-teardown/crate/set-physics-20261024.log.

Both original no-command repeats reach the new native1463 boundary with
218693 instructions/24284 writes, native work492818 cold/492276 warm. Both
first-Training cohort repeats retain all64 equal observations,11 eligible/
53 skips,48 sweeps/13 hits/two ordinary landings/nine rotational moves/three
carries and their full state/clock/write continuity. Their later totals are
223490 instructions/25937 writes; native work521631 cold/521007 warm. All
original source/whole-root rollback controls pass. Evidence: startup-repeat
and cohort-repeat logs with training-struct-arrays prefix/date20261024 in
artifacts/; codec, vm-final and generated logs use the same prefix/date.
Generated authored map Object targets are already rooted; those cases alone
must not claim sole nested runtime-birth GC retention/replacement-birth proof.

Fresh late cohort interruption at write16414 repeats exactly207564 instructions,
native work488534 cold/487910 warm, undoing all three carries, earlier landings
and the partial final active fragment update. Full-root rollback/all41 unchanged
sources pass. Evidence: training-struct-arrays-cohort-latecut-final-20261024.log.
The first invocation omitted the explicit larger native-work allowance and
correctly exhausted the default65536 during PostBeginPlay; it is not accepted
as a late-cut witness. The final run uses the same1048576 allowance as the
other original startup/cohort observations and checks exact error VM write limit.

Removing this storage boundary is not completed startup, actual weapon notify
delivery, committed physics, first-Training completion or a new installable APK.
The original post-Possess/post-partition continuations and any newly reached
native gaps must remain real failures until implemented, with whole-root undo.

The final coherent desktop build and shared CTest pass179 executed/211
registered with32 explicit owned-data skips, zero failures in138.13s; the
fresh owned checks above run separately. Evidence: final-full-build and
final-ctest logs with this increment's prefix/date. No new APK was built or
installed. Next crate work is the original main-animation notify phase and
real pickup/equip/Fire bridge; the preferred-axis query audit records a later
startup dependency, not a reason to detour into new HUD features.
