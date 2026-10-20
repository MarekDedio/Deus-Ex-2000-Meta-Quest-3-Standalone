# First Training: original raw Mesh/DrawScale stores

2026-10-19/20. First `Maps/00_Training.dx` only; other campaign-map integration
remains deferred. This increment is a startup prerequisite, not live crate,
loot, gravity or complete Training acceptance. No replacement APK is justified
by these CPU-owner controls alone.

## Original contract and implementation

The actual Engine.u `Engine.Inventory.BecomePickup` is87 logical bytes,
native0/flags2. It writes Actor.Mesh from Inventory.PickupViewMesh atPC22,
then DrawScale from PickupViewScale atPC33. It subsequently writes visibility,
carried-item and priority fields before explicitly calling native262
SetCollision(true,true,false) atPC79. The raw stores cannot be replaced with
extra calls to that setter or reordered after it.

Read-only static Core.dll inspection distinguishes the raw bytecode store from
a collision service: execLet1012FE40..1012FECC obtains a reflected destination
and evaluates the RHS into it; execInstanceVariable1012EC60..1012EC95 resolves
the field address. Scalar ObjectProperty copy10166B40..10166B69 and FloatProperty
copy10166750..10166779 are DWORD copies, without a collision or animation hook.
Original commercial EXE/DLL code is not loaded or executed by these tests.

For the live-hash exception, `ReferenceFor` authenticates the scalar original Actor declarations:
Mesh is an ObjectProperty constrained to Engine.Mesh; DrawScale is a
FloatProperty without a subtype; both flags0x21, dimension1, index0. Concrete
receiver ownership, declaring Actor, property class and qualified storage are
checked. Runtimes without that collision domain keep their existing ordinary
reflected-store admission; a synthetic FloatProperty is not newly required to
match native hash metadata. These fields use the existing TypedLiveValue/Touch/
VM undo path and ordinary write accounting. Other active-hash guards remain.

Neither raw store removes/adds hash links, writes ColLocation/CollisionTag,
changes native pose, invokes a setter, chooses a primitive eagerly, or rebuilds
the animation clock. No equality shortcut or new host finite-scale gate is
added. The existing common VM admission still rejects nonfinite Float values
before host stores; this remains a fidelity boundary distinct from the
original DLL's unrestricted raw DWORD copy. It is not bypassed by these
diagnostics. Original GetPrimitive observes live Brush-before-Mesh-before-Cylinder
precedence on demand. The audited primitive cylinder bounds/line providers do
not consume DrawScale; this increment must not claim a stale-bounds witness
from these two fields. Native262 still removes using current fields, updates
its original flags, then re-adds/prepends even for unchanged arguments.

## Diagnostic and test scope

Three closed rollback-only diagnostic operations exercise raw Mesh, raw
DrawScale and the original fixed Engine.Inventory.BecomePickup function.
They use the same host/root and normal VM references, not private direct
overlay writes. Detached rows record live Mesh/scale, actual selected primitive
and canonical full native-clock bytes. Strings/vectors retain conservative
output accounting. A null Primitive in a cylinder hit is not treated as proof
of which Mesh was selected.

The focused first-Training controls cover unchanged/changed crate stores,
Mesh=None selecting the actual Engine-owned Cylinder, a second genuine loaded
Mesh, exact finite/negative-zero/subnormal Float words, and independent hash/tag/hit/
encounter-order expectations. Only the explicit unchanged native262 changes
the tested encounter order. No-hash raw controls must not initialize tables or
the RNG. Typed Mesh/class/count/receiver failures and late failures restore the
full save/revision/lookup/GC/hash state. NaN/Inf controls assert the common VM
Invalid refusal before a host store and restore their successful finite prefix.
Actual Doctor1.PlayWaiting supplies the
animation clock; no idle, role, pose or begun-play flag is invented.

The real NanoKey original body is executed after changed raw inputs in the
same root, including its final SetCollision call, then checked against its
authored PickupViewMesh/Scale and the independent hash oracle. These remain
dormant diagnostics, not a forced pickup, live player interaction or successful
world startup. Focused entry: `portable_retail_collision_test --raw-shape-only
--owned-source-root OWNED_GAME_ROOT`.

## Observed startup advance

Two actual first-Training retries with explicit native-work262144 complete all
1334 PostBeginPlay and1334 SetInitialState visits. Both next refuse WorldBases
slot631, `00_Training.LightSwitch0`, after629 successful WorldBases visits:

```text
Unsupported original world FindBase: extent SingleLineCheck flags0x17 / SupportActor contract is not implemented
```

Both execute529945 instructions/39425 writes, final native work151787, not
exhausted. Their30.9605s/30.63s durations occurred during native compilation;
these are not headset frame times or controlled performance measurements.
Complete save/state/revision/hash/RNG/client/lookup/Actor/GC rollback and all40
owned source files' immutability pass. CLI exit2 is intentional unfinished
startup, not success. No memory/native-work ceiling or original callback was
skipped. The diagnostic-only output-allocation allowance was then corrected;
that branch is not used by the startup coordinator.

The next dependency is the original extent SingleLineCheck/SupportActor/
FindBase behavior, followed by remaining startup and live crate damage,
destruction/usable contents and support/falling/landing acceptance. Other maps
remain out of scope.

### Follow-up support audit (not implemented here)

Static Engine.dll review confirms FindBase103C9470..103C9566 sweeps eight
units downward with actual cylinder extent and flags0x17. It observes the hit
Actor, then invokes dynamic target.SupportActor(self), not a direct replacement
SetBase. SingleLineCheck1039AED0..1039B13B uses actual MultiLine order, excludes
self and self.Owner ancestors, and applies original class/flag precedence.
Flags0x17 do not impose IsBlockedBy or a floor-normal threshold. A miss writes
only Actor=None and Time1; a selected hit copies the full packet. The native
hit arena must be popped before the support script callback. The world pass
subsequently clears an actual-LevelInfo Base with SetBase(None,false).

The current legacy SetBase also needs correction before support acceptance:
retail10369C80..10369D8E scans the prospective Base chain before equality,
uses wrapping StandingCount byte decrement/increment (not saturated based-list
size), dispatches old.Detach, writes captured newBase, dispatches new.Attach,
then optional self.BaseChange. It rereads source.Level after Detach. It does
not maintain a native basedActors list. Eligible spawned FindBase shares the
same service and has original ordering before PreBeginPlay. None of these
contracts, complete Falling physics, or scheduler behavior are implemented
by this raw-store increment.

## Current verification

The focused actual first-Training suite passes294162 checks/34 explicit
refusals, including the real original NanoKey body through native262 and
genuine animated Doctor1 full-clock controls. Its independent source-cylinder
oracle permits the real key's fractional dimensions; the older crate-only
integer oracle is not reused outside its valid cohort. Exact binary64 sums
are restricted to bounded binary32 operands and retain the original Y-spill
distinction, then checked against the actual primitive bounds.

The shared generated Spawn/save/GC regression passes9211 checks/144 rejection
controls, including the ordinary no-domain DrawScale store. The coherent full
host build, strict NDK27 ARM64 syntax and final Android native compile/link
pass. CTest excluding the separately exercised owned retail target passes
154 executed/177 listed with23 explicit owned-data skips, zero failures,
99.87s. Skips are not real-map or device acceptance. The full actual first-map
regression now passes1422705 checks/119 explicit refusals, with1308 actual
native-slot actors,120 collidable/nondeleted actors,478 links and158 actual
primitive-query predictions; complete save/revision/clock/hash/lookup/GC/RNG
rollback and all40 sources unchanged. A final-source repeat2 startup observation independently confirms the
same WorldBases/LightSwitch0 refusal,529945 instructions/39425 writes and151787
native work, with complete rollback and all40 sources unchanged. Startup remains
unfinished (expected CLI exit2), not accepted gameplay.

Verified ignored logs beneath `artifacts/`:
`training-raw-shape-host-final-20261020.log`,
`training-raw-shape-host-all-verified-20261020.log`,
`training-raw-shape-arm-runtime-final-20261020.log`,
`training-raw-shape-android-final-20261020.log`,
`training-raw-shape-spawn-regression-20261020.log`,
`training-raw-shape-ctest-verified-20261020.log` and
`training-raw-shape-owned-focused-key-oracle-20261020.log`.
Final-source startup log: `training-raw-shape-startup-final-20261020.log`.
Complete actual first-map log:
`training-raw-shape-owned-all-key-oracle-20261020.log`.
Earlier diagnostic/test-assumption failures remain in their original logs;
they are not listed as acceptance evidence.
