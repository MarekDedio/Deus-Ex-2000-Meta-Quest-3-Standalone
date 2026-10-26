# First Training: collision work after Weapon updates

Reached on 2026-10-26 after exact native `Engine.Weapon` Tick/physics admission.
Only the actual first `Maps/00_Training.dx` is loaded. This is a rollback-only
startup/world-update diagnostic, not published gameplay, an APK or Quest acceptance.

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

Consequently the current error text alone **does not identify** which ledger
was exhausted, which Model/owner was being checked, or how much work that
particular failed kernel had performed. Repeated full-Model validation is a
candidate, not a demonstrated diagnosis.

## Next implementation/verification work

First retain bounded failure telemetry: actual Model/owner and branch, the
prequery aggregate counters, admitted remaining validation/query allowances,
and whether validation or traversal failed. Preserve original hit output,
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
