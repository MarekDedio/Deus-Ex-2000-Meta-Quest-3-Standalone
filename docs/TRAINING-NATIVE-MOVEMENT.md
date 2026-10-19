# First Training: actual native movement bridge (2026-10-19)

This work targets only `Maps/00_Training.dx`. It connects the complete audited
movement coordinator to the actual runtime, rather than adding another mock
Move provider or replacing collision with direct transform writes. The short
term acceptance target remains live crate damage/destruction, surviving usable
contents, support/falling/landing and the rest of the first Training route.
Movement diagnostics alone do not finish that target.

## Connected authorities

`portable_actor_move.inl` binds native266 `Actor.Move` and native299
`Actor.SetRotation` to the actual current native Level. The original UFunction
and canonical Vector/Rotator parameter/result schemas are authenticated. Move
samples Rotation after argument evaluation; SetRotation supplies zero Delta.
Neither entry invents begun-play, possession, role or an active render actor.

The complete MoveActor coordinator uses the real actual-Level world/actor
MultiLine service, native hash, qualified hit store, blocking/basing/overlap
predicates, higher encroachment, recursive child carrying, moveSmooth, selected
BumpWall/Bump/HitWall/EncroachingOn/EncroachedBy events, BeginTouch/EndTouch and
movement-time SetActorZone. All share the same caller Execution, work/write
allowances, hash lease and outer host transaction. No nested Machine or local
gameplay commit is introduced. Ordinary false/veto is not a rollback command.

`RetailMultiLineLevel` is factored from the older diagnostic wrapper so native
movement has no diagnostic source-anchor active/deletion gate and does not
exclude its source from returned hits. The public dormant diagnostic retains
its separate anchor checks. World-only movement does not create a native hash
or consume the CRT initialization stream merely to own temporary hit lists.

The retained-hit adapter authenticates the native Level, map generation and
live lookup root on access. Every producer, retained vector capacity and
recursive frame participates in the existing shared state/scratch accounting.
Adoption/late admission stays in the same native hash root. Pop/Clear perform
no allocating lookup, budget callback or script dispatch; cleanup precedes
lookup/birth retirement. Commit refuses surviving marks, arenas or pose scopes.

## Pose and publication

Native Location/Rotation stores are individual typed scalar writes through the
same VM journal. Durable writes Touch the host snapshot before mutation and do
not reset an actor's animation clock. Generic script pose mutation cannot
bypass an active native field authority or strand live native hash links.

Higher encroachment's reverse push owns only a mutable temporary Location
layer. Recursive Location writes target that layer while Rotation remains
durable. Releasing it exposes the preceding raw Location; it does not repair
native hash/ColLocation, restore Rotation or erase candidate/event side effects.
Delayed VM undo uses generation-qualified tokens and cannot turn an expired
temporary write into a durable one. The outer host snapshot supplies complete
durable rollback on a real refusal.

Hash Remove precedes pose stores and Add follows them, with actual live field
refresh at each service boundary. ColLocation remains Add-owned. The older
derived presentation/placement collision registry is synchronized only from
final durable poses at successful outer publication, never from a reverse
temporary pose. Its legacy spawned FindBase consumer refuses while that cache
is unsynchronized instead of reading stale geometry. This does not implement
the missing original spawned FindBase contract.

Child carrying uses the CPU-owned constructor UnitCoords value and the pinned
sine-table profile. This is not an original mutable process GMath snapshot.
Core's constructor `10144130..10144386` uses literal integer stores: origin
positive zero at `10144182/10144198/101441AF`, X `(1,0,0)` at
`101441D1/101441DF/101441F6`, Y `(0,1,0)` at
`1014420E/10144224/1014423B`, Z `(0,0,1)` at
`10144246/10144254/1014426F`. No x87 calculation or later constructor rewrite
changes those48 bytes at GMath+18; original mutable external-module writes are
outside this sealed CPU-owned value's domain. The sine profile's distinct
arithmetic authority remains in ORIGINAL-SINE-PROFILE.md.
No original commercial executable or DLL is loaded or executed.

## Verification boundary

The new public movement diagnostics always roll back their complete root.
Detached output contains paths and copied pose/hit/clock fields, not live native
hit handles. Direct LevelMoveActor supplies an explicit caller-hit seed so
untouched fast exits can be distinguished from full movement. bTest is a
mutating native flag inside that root, not a read-only preview operation.

The strict ARM64 runtime/expanded-test syntax checks, coherent desktop build
and Android native compilation/link pass. The focused actual38-package first
Training test passes122541 checks/seven explicit refusals. Ten movement roots
cover static rejection, rotation-only fast exits/native299, real nonzero
native266/Smooth, clear and blocked sweeps, bTest mutation, Add-only ColLocation,
three late/partial refusals and complete mark/arena cleanup. Seven nonzero
cohorts use independent hardware-x87 cap/backoff expectations. The authored
upper crate stops at Z=-53.8338623 above the lower crate, corrected
Time=0.57709372, not at an invented directly-set floor height.

Actual Doctor1.PlayWaiting clock bytes remain identical inside full movement
with the native hash and after a separate committed world-only native Move.
The latter publishes the exact durable/render-consumer pose and one revision,
preserves RNG/lookup/GC and restores the exact checkpoint afterwards. It
exercises Commit's derived-cache synchronization path but does not independently
inspect the private legacy registry's contents. Partial/late rollback controls
remain separate from successful native false results.

The first full observation advances actual startup from native266 at
DeusExMover16 BeginPlay PC166 to native316 at PC197. Static original bytecode
identifies that as `Core.Object.Add_RotatorRotator`, evaluated before native299's
entry at PC195. This is not evidence that startup's SetRotation completed.
The final complete owned first-map regression passes1164319 checks/83 explicit
refusals:38 exact Quest source packages,1308 native-slot actors,120 collidable
nondeleted actors,478 registration links and94 independent-order actual
primitive calls. Full save/revision/lookup/hash/clock/GC/RNG rollback and source
immutability pass. The new startup boundary repeats after rollback and the
source actor/hash mirrors remain correct. A test-order correction keeps the
cold PointRegion oracle ahead of movement's immutable-cache warmup.

The coherent shared CTest passes151 executed/175 registered,24 explicit
owned-data skips and zero failures in66.83s. Skipped tests are not device or
actual-map evidence. Real moving-brush reverse-push/veto and based-child
scenarios are not established by this actual-provider cohort; detached kernel
tests and binding review are separate evidence. Begun-world callback execution,
playable gravity, destruction/loot, full startup and headset acceptance remain.
No other campaign map and no replacement APK are included in this backend work.

Ignored build/test logs are `artifacts/training-move-bridge-arm-current-20261019.log`,
`training-move-bridge-arm-test-final-20261019.log`,
`training-move-bridge-host-all-20261019.log`,
`training-move-bridge-host-final-cases-20261019.log`,
`training-move-bridge-android-20261019.log`,
`training-move-bridge-ctest-20261019.log`,
`training-move-bridge-owned-focused-20261019.log` and
`training-move-bridge-owned-final-20261019.log`, all beneath `artifacts/`.
