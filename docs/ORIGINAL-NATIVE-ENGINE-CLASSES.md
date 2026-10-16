# Native-only engine class authority

2026-10-17. This is the native UClass registration prerequisite for actual
Engine/Cylinder collision ownership, not a replacement for that ownership.
Original owned binary evidence is in ORIGINAL-ENGINE-CYLINDER.md. The port
does not load or execute the commercial DLLs.

The original Core.u has seven serialized classes, including Object export3
and Subsystem export350. Core.Class and Core.Package are absent from that
serialized class set. Engine.Primitive, Engine.GameEngine,
Extension.GameEngineExt and DeusEx.DeusExGameEngine similarly have native
registrations instead of serialized class declarations. Inventing a
PortableClassDescriptor or a copied Model/Mesh CDO for them would give false
defaults and native identity.

Owned Core.u SHA256, independently rechecked during this work:
4FB22EE4E3A6868DFABB2B46B15FAD8E5094F5FDA5927A14FF11670A744EC36C.

## Implemented registration phase

`PreparePortableRuntimeNativeEngineClasses` binds a closed five-entry audited
native registration set, independent of maps, native clients and Quest leases:

| Native class | Actual superclass | Native size | Constructor class flags | Constructor UClass object flags |
| --- | --- | --- | --- | --- |
| Core.Package | Core.Object | 34 | 12 | 04084004 |
| Engine.Primitive | Core.Object | 54 | 12 | 04084004 |
| Engine.GameEngine | Engine.Engine | D0 | 1E | 04084004 |
| Extension.GameEngineExt | Engine.GameEngine | D0 | 1E | 04084004 |
| DeusEx.DeusExGameEngine | Extension.GameEngineExt | D0 | 1E | 04084004 |

All numbers are hexadecimal; every ClassWithin is actual Core.Object. These
are original native-registration constructor baselines, **not instance-allocation
flags**, serialized post-load flags or a claim that full UClass.Register/CDO/
config/default construction or UEngine.Init ran. Snapshot fields are explicitly
`registrationClassFlags`/`registrationObjectFlags`. The five known class flag
baselines remain unchanged by Register's superclass inheritance mask4EC;
that alone is not a full Register implementation. `quest_native_engine_classes.h`
contains only the fixed original registration values, not commercial code or
serialized class bytes.

The runtime reads the actual bounded GameEngine setting from System/DeusEx.ini
during initialization. Missing remains unavailable; malformed/conflicting
values refuse, and no fallback Engine class is supplied. Only the three
audited concrete GameEngine identities can authorize this phase. Root and
GameEngine are separate setting APIs with the existing grammar/resource bounds;
retail duplicate precedence is not claimed. Same-identity duplicate values are
accepted, conflicting values explicitly refuse.

Preparation requires actual loaded native Core.Object/Core.Subsystem/
Engine.Engine UClass metadata, exact direct Subsystem->Object and Engine->
Subsystem pointers. Original serialized class flags are112/112/11E respectively;
Engine.Engine's11E is not its native constructor baseline1F. The binding does
not add a nonexistent Abstract bit to loaded metadata. It checks actual inherited
Engine.Engine.Cylinder: original Core.ObjectProperty, dim1, flags0, no property
base, and original Engine.Primitive reference. A field merely named Cylinder,
foreign same-name metadata or absent typed ownership cannot authorize it.

Only then does it stage all native UClass objects, their immutable base/Within
GC references, a separate rooted registration domain and copied canonical/
case-folded lookup indexes. The complete result/owning root and retained-state
budget validation precede a swap-only publication interval. Late admission
failure publishes no partial indexes; temporary objects are collected after
their staging roots die. Successful repeated preparation returns the same
domain without new objects. No UObject naming counters are consumed.

The distinct native registration trait is not nativeClientClass. Original
object/class/meta casts and object-path string conversion recognize the
audited type identity without using a nonexistent exportIndex. Reflected
Core.Class header storage, selected native class defaults/CDOs, script actor
execution and state descriptors remain explicitly unsupported; a native
registration is not permission to fabricate them. Core.Class's own native
registration/storage hierarchy is separate future work if needed.

The rooted domain traces every registered UClass and immutable base/Within
references. Classes are outside script/map exports and world slots, never
actor births or script-save patches. Their domain/objects/string/reference/
lookup-node memory is included in retained-state budgets. Map unload/reload,
warm save restoration, GC and clearing/revoking a headset client do not replace
these static registrations. Shutdown removes indexes and the domain before GC.

`ReadPortableRuntimeNativeEngineClasses` exposes detached registration values
and configured-class provenance only. It validates the actual domain/index/
base identities, not just display paths. The API reports registration prepared,
not engine initialized. Runtime-owner thread rules apply.

## Remaining real integration

No Engine, UPackage or Primitive **instance** is constructed by this phase.
There is no Engine.Cylinder or Level.Engine edge yet. Actual constructor state
(including fresh FURL and Transient binding), explicit embedding allocation
inputs, unique names/counters, Engine rooting/serialization and map binding
are the next required native lifetime work. Original primitive selection,
generation-keyed Brush Model data, collision hash/RNG lifecycle, shared query/
Move transactions, callbacks/support, startup/possession and falling remain.
No crate destruction, physics tick, new APK or headset acceptance is implied.

## Verification

Targeted host build and strict ARM64 syntax pass. The independent config suite
passes455 checks, preserving Root diagnostics and exercising both selected
settings, bounds, missing/conflicting settings and read-only filesystem cases.
Owned-original integration passes10041 checks/145 explicit refusals across
actual dormant Training, literal native registration records, actual serialized
base flags, class/meta casts, complete checkpoint/revision/clock/world/Model
invariance and exact six-object GC delta (five UClasses plus domain). Client
revocation/clear, unload/reload/GC and shutdown preserve/release those roots at
their intended boundaries. A late identity collision after the first allocation
leaves no staged indexes, objects or saved/gameplay changes. Unknown/default/
descriptor/Actor and missing/unsupported/malformed config/base cases refuse.

Explicit warm checkpoint restoration also passes: native registrations, GC
counts, same Model generation/identity, dormant state, clock, world slots and
complete saved bytes remain intact. The explicit successful load advances
world revision once; pure registration/read checks retain their separate
unchanged-revision assertion. Full coherent host rebuild passes. Fresh CTest
reports121 passed/138 registered/17 explicit owned-data skips/zero failures
in57.12s. The original-data registration test above ran separately with an
explicit owned game root. Independent read-only review found no blocking issues.
No instance construction, original Register/default execution, movement or
on-device evidence is claimed. Local ignored logs use
`artifacts/native-engine-classes-*-20261017.log`.

Deployment now requires owned System/DeusEx.ini in both source validation and
remote post-transfer checks; the whole System directory was already copied.
Local read-only validation passes220 packages/738318285 bytes and no missing
required files. Deployment script syntax passes. No headset is currently
connected, so no remote-file or installation success is inferred.
