# First Training: collision work after Weapon updates

Update2026-10-26: the measured validation blocker below is now passed by
[lazy immutable-source validation reuse](IMMUTABLE-MODEL-VALIDATION.md).
Actual cold/warm Training repeat2 advances to Engine.GameInfo after121 slots /
120 calls at237,922 instructions /19,289 writes, with full undo and fresh
proof state after same-filename map replacement. No work cap was raised or
reset; no collision/NPC was skipped. The remaining game-rules native gate is
recorded in ORIGINAL-TRAINING-GAMEINFO-TICK-GAP.md. The measured sections below
are historical motivation, not a claim that the old cap is still reached.

Reached on 2026-10-26 after exact native `Engine.Weapon` Tick/physics admission.
Only the actual first `Maps/00_Training.dx` is loaded. This is a rollback-only
startup/world-update diagnostic, not published gameplay, an APK or Quest acceptance.

## Measured cause, 2026-10-26

Bounded detached telemetry now identifies the reached failure without changing
the existing exception text, numerical work, limits, hit publication or undo:

`model="00_Training.Model36" owner="" phase=validation`

- Aggregate validation before the query: 7,937,992 of 8,000,000 steps.
- Aggregate traversal before the query: 8,344 of 16,000,000 steps.
- Admitted remaining validation: 62,008; this kernel uses exactly 62,008 and
  refuses the next validation step.
- Admitted remaining traversal: 15,991,656; traversal is not the exhausted phase.

This proves the immediate blocker is validation work, not traversal, shared
VM/native work or an unimplemented Weapon method. The budget error alone is
not evidence of malformed geometry.
It does not claim that all preceding validation work belonged to this one
Model, nor that completing this query would complete the whole frame.

`ModelExtentTraceWorkExhausted` retains the compatible runtime_error message
and pre-increment used/limit. The shared `Validate` entry tags validation,
including its reuse by box occupancy; query steps remain separately tagged.
Native world/owned extent LineCheck providers retain one bounded Model/owner
and prequery allowance snapshot before existing catches consume their ledgers.
The CLI requires that snapshot for this refusal and checks repeated snapshots.
Occupancy providers are not yet instrumented: the CLI would reject, rather
than silently accept, an identical-text occupancy failure without a snapshot.

Kernel coverage passes 60,055 checks / 64 deliberate refusals, including
owner/null-owner early/middle/final phase cuts, direct shared validation,
exact sufficient-budget result bits/all work counters, malformed pruned data,
the independent scalar oracle, and 96 successful original first-Training
collision checks. Training's actual root Model36 has 9,524 nodes and 3,183
distinct collision hulls. This is not a native Move/physics completion claim.

The grounded next optimization is secure reuse of successful validation of
loader-owned immutable geometry. Keep mutable helper inputs fully validating,
authenticate map/Model/cache lifetime, distinguish graph-only/owned-hull/
null-owner-hull proofs, and preserve all real traversal and collision outputs.
No validation cache or physics fix was implemented by those diagnostics;
the later immutable-source implementation is linked in the update above.

Both actual Training repeats retain that exact snapshot and the previous
233,606 instructions / 18,817 writes / 113 slots / 113 calls. Paired-door
repeat2 retains both 56-unit panel moves and two completion callbacks, then
the same Model/validation failure with prior validation7,938,066/query8,526,
remaining validation61,934/query15,991,474 and used=limit61,934. Door script
counts stay233,942/18,885. Two original crowbar Fire cycles repeat2 retain
228,837/26,047 and48 rows/two notifies/four traces/two crate changes/nine births
before the later FExec boundary. Every diagnostic verifies whole-root undo.

Final desktop/Android-native/strict ARM64 checks pass. CTest216 executed /
255 registered /39 explicit owned-data skips /zero failures76.64s; separate
production driver8,825/163 before final malformed-schema control, owned crate
415/5, deferred429/21 and animation588/24 exit0. All45 original baseline files
and7 frozen compiler inputs retain exact size/timestamp/SHA256; all jobs are
terminal. This is not an APK or headset performance validation.

### Immutable-source implementation route

Existing world geometry is a fresh-loader `shared_ptr<const PortableModelGeometry>`;
brush geometry is a fresh-loader `unique_ptr<const PortableModelGeometry>`.
Both caches authenticate map generation, actual Level/Model, source/export/path
on use and clear before map GC. These are immutable asset caches, not script
state and not a reason to publish a partially failed gameplay transaction.

The two immediate world branches (nonzero-extent LineCheck and box PointCheck)
call exactly the same `ModelExtentTraceDetail::Validate` with owned=false.
A successful null-owner hull validation proof on the same immutable source
can therefore cover both; it need not be duplicated per query kernel. Keep
cheap current-call Limits/cardinality admission and both actual query bodies.
Do not prevalidate hulls at decode: FastLine/zero-point branches intentionally
do not activate all those hull/bound dependencies.

Use an opaque immutable collision source with private successful proofs,
created directly by fresh world/collision loaders; a fixture factory must
deep-copy mutable input. Do not accept an arbitrary caller-provided const
shared_ptr or moved mutable vectors as immutable authority: outside aliases
could still mutate them. Typed query overloads should accept this source,
not raw geometry plus a public skip-validation flag. Raw mutable helpers
must retain their full validation behavior.

Validate lazily at the old call position. Charge complete actual cold
validation and mark a proof only after success; warm queries charge no
validation work but keep all traversal work. Distinguish graph-only, owned
hull and null-owner hull proofs: owned hull bounds are structurally required
but their NaN/Inf/reversal is deliberately inactive. A warm proof may permit
a real query when the aggregate validation allowance is zero; an uncached
validation must still refuse there. Never renew/reset either root ledger.
Exercise mutable aliases, owner/null-owner differences, malformed/pruned
graphs, cold failure, warm sharing, budget cuts, map replacement/unload,
checkpoint/rollback and the unfiltered real Training update before claiming
the optimization works.

## Observed boundary

Two unfiltered real-map attempts advance beyond WeaponCombatKnife0 to
`00_Training.UNATCOTroop0`, concrete `DeusEx.UNATCOTroop`, nearest native
`DeusEx.ScriptedPawn`, Role4/RemoteRole2/Physics2. Both stop at:

`Original model extent trace: work budget exhausted`

Both retain 233,606 script instructions / 18,817 writes, 113 dynamic slots /
113 Actor calls, 11 stasis returns, 9 deferred entries and no completed pending
pass. The former exact Weapon gate stopped at 66 slots / 65 calls and
219,208 instructions / 17,062 writes. All effects are undone on failure.

The observed final **shared native-work** usage is 509,733 then 509,191 of
16,777,216, with exhausted=false. That budget is not the exhausted resource.
The variation is not a claim of different script writes or a completed frame.

## Independent collision safety ledgers

`native/quest_native_work.h` defines the portable shared native-work maximum
16,777,216. The CLI already uses that maximum; increasing `--native-work`
cannot address this separate failure.

`native/portable_unreal_runtime.cpp` independently defines:

- `MaximumNativeModelValidationWork`: 8,000,000 per outer root.
- `MaximumNativeModelQueryWork`: 16,000,000 per outer root.

`BeginRoot` resets those collision counters once. Model primitive LineCheck,
PointCheck, world MultiLineCheck, FastLine and far-move paths share the
counters and clamp their kernel allowances to the remaining work. They do
not receive a new allowance for each Actor or falling substep.

`native/quest_model_extent_trace.h` has its own fixed positive per-call limits
and checks both validation and traversal with the same `Step` error text.
Validation scans all BSP nodes/hulls and all graph components before a query.
Both validation and query statistics are returned only after success; catching
world/occupancy providers conservatively consume the ledgers after refusal.

Consequently the historical error text alone **did not identify** which ledger
was exhausted, which Model/owner was being checked, or how much work that
particular failed kernel had performed. The measured section above now proves
validation is exhausted; it does not replace the counters with a guess.

## Next implementation/verification work

Bounded direct extent failure telemetry is now implemented: actual Model/owner,
prequery aggregate counters, admitted remaining validation/query allowances,
and validation versus traversal failure. Preserve original hit output,
script callback order, shared-root work/state accounting and whole undo.

Then address the measured cause. If repeated immutable-geometry validation
is responsible, any reusable validated representation must authenticate the
actual retained geometry/lifetime and the distinct owner/null-owner contracts;
it must not suppress validation of changed, malformed, cyclic or pruned data.
If traversal is responsible, measure it before choosing an optimization.

Do not reset a budget mid-root, raise caps silently, skip this NPC, force
Physics0, pretend a trace is clear, publish a partial startup, or describe an
expected-refusal test as working crate gravity. Recheck the unfiltered first
Training update, paired doors and two actual crowbar attacks after a fix.
Other-map integration remains deferred.
