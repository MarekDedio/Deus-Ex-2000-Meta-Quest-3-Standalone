# First Training: original Actor support relationships

Scope: only `Maps/00_Training.dx`. This source increment replaces the port's
reverse-list-based SetBase behavior. It is not FindBase integration, full native
Destroy, falling simulation, a playable crate-physics claim or a new APK.

## Actual fields, not inferred topology

Native SetBase now uses `quest_actor_base.h` through the same VM execution,
native-work ledger and transaction as the caller. The exact original qualified
`Actor.Base` Object field (flags0x22, Actor constraint), `Actor.Level` Object
field (flags2, LevelInfo constraint), and `Actor.StandingCount` Byte (flags2)
are authenticated. The two stores use `Execution::WriteReference`, sharing the
outer VM's write limit and undo journal. Base/count copies have no animation
clock, collision-setter or reverse-list side effects. Unrelated same-leaf
subclass fields are not granted these exact-key clock exclusions.

Prospective Base traversal precedes even the unchanged-Base return. Independent
counts increment/decrement modulo256. The old/new count targets are captured
pointers, while Attach/Detach receivers reload live Base after the count store.
The new Base parameter survives Detach reentrancy; the Level comparison is
reread afterwards. Selected Attach/Detach/BaseChange events execute synchronously
through the existing actual event/probe policy in the same root. There is no
presentation-active predicate in SetBase and no replacement child transaction.

The runtime `basedActors`, `initialBasedActors` and `initialStandingCount`
sidecars are removed, including their GC, rollback, save/load, budget and
revision-classification roles. Base and count remain independent raw fields,
even when callbacks or direct script stores leave them inconsistent.

## Load and historical-save migration

`StandingCount` flags2 mean Const, not Transient. Original Core serialization
does not exclude that bit. Static inspection of the owned original Actor
Serialize/PostLoad, LevelBase/Level Serialize, Level PostLoad and generic LoadMap
bodies found no basis for the port's reconstruction from authored Base links.
Map load now preserves the actual declared, inherited, default or serialized
byte. It neither derives a list size nor forcibly resets the byte to zero.
No original executable or DLL is loaded or executed.

The historical `ScriptSavedActorLifecycle.basedActors` field remains in the
structural codec with identical layout, field order, limits and identity
validation. Valid entries are charged at ingress but deliberately not resolved
into a second runtime pointer graph. New runtime captures always emit an empty
deprecated list. Thus old-save load/recapture intentionally canonicalizes that
field away; structural codec encode/decode remains byte-compatible. Actual
saved Base/count fields and owner-child/touch/removal state are preserved.

## Coherent teardown boundary

The legacy Destroy path can no longer drain a removed reverse list. Its support
detach fragment gates once on the live count, then scans ascending receiving
Level slots and checks each candidate's current direct Base. It rereads live
slot count/storage after callbacks and does not stop when the count becomes
zero. Cleared (`worldRemoved`) slots are absent; no presentation-active or
candidate-delete filter is added. Metadata-only generated fixtures explicitly
use their export-order slots.

This is not complete retail Destroy: the older surrounding delete/event order
still differs, and the existing active-retail-hash Destroy refusal remains.
FindBase's original downward sweep and selected SupportActor callback still
need integration before original world startup or real crate gravity is ready.

## Verification

The corrected generated lifecycle integration passes1954 checks/25 rejection
controls. It proves authored count7 with exactly one Base edge, inherited
default count19, wrapping byte operations, synchronous reentrant callbacks,
fresh Level reads, exact VM write limits, complete rollback/revision/GC checks,
historical-list migration and the count-gated forward teardown fragment.
Generated timer coverage passes1469 checks/47 refusals. The fixtures now encode
the original Net property's replication-offset word as well as its flags.

The focused actual first-Training run passes409929 checks/31 explicit refusals.
Its independent declared-field model covers8 successful dormant roots,
27 genuine native298 entries,47 Base/BYTE stores,2 cycle and2 equal controls,
14 refused roots, and the actual LevelInfo exception. Hash registration,
encounter order, tags, ColLocation and RNG are unchanged. Exact N-1/N shared
write/work cuts and a second call sharing the first call's work allowance
exercise complete outer-root rollback.

Actual Doctor1.PlayWaiting supplies a genuine full animation clock, compared
with canonical codec bytes INSIDE ordinary/equal/LevelInfo/None Base updates
and after count stores from a separate crate. Success, late refusal and partial
write failure restore the exact checkpoint, class/Outer/lookup identities,
revision, Actor census, RNG and GC state. No pose, role, begun flag or idle clock
is fabricated. These dormant controls do not prove executed begun-world
support callbacks or automatic gravity.

Strict NDK27 ARM64 runtime checking and Android native compilation/link pass.
The coherent host build passes. Corrected shared CTest passes156 executed/
180 listed,24 explicit owned-data skips, zero failures,53.56s. Skips are not
actual-map/headset evidence. The complete actual first-Training regression
also passes1896941 checks/143 explicit refusals:1308 actual native-slot actors,
120 collidable nondeleted actors,478 registration links and214 independent
primitive-provider predictions. Full snapshot/save/revision/GC/RNG rollback
passes. The final owned
executable is preserved as `artifacts/training-set-base-final-20261020.exe`;
the earlier preview executable predates the support-scan Level-identity fix
and is not final verification.

Repeated actual startup still stops at the deliberately unimplemented original
WorldBases/FindBase boundary on slot631 LightSwitch0. Both attempts complete
all1334 PostBeginPlay/SetInitialState visits, execute529945 instructions and
39425 writes, and use151787 native work out of explicit262144. Both verify
full root rollback and all40 owned sources unchanged. CLI exit2 means startup
did NOT finish. Durations43.5899s/88.9579s were observed alongside compilation;
they are not frame times or controlled performance measurements. No other map
is loaded and no new APK is packaged. The subsequent FindBase integration is
separate work and does not inherit this checkpoint's acceptance automatically.
