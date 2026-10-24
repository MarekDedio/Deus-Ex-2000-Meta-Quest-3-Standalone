# Reached Actor.Trace277 wrapper

## Implemented query checkpoint,2026-10-24

`portable_actor_trace.inl` now binds the actual seven-property declaration,
six NativeParm30-byte body, source/Level identity and original SingleLine
service. VM mixed packets preserve address-only OUTs as disposable prefix
markers while copying ordinary inputs at their own Step. Sequential defaults
and supplied omissions retain native order; absent trailing placeholders are
materialized in order with no intervening expression. The public VM header is
unchanged. Returned hit identities are Actor instances, not UClass values;
Location/Normal writes reuse the existing guarded journal in that order.

Mixed VM controls pass1205 checks/32 refusals; the existing VM suite remains
696084/897. Actual owned first-Training native277 controls pass43 production
calls,37063 checks/23 refusals, including all9 warm query native-work cutoffs,
exactN success, eight genuine BSP point/extent/actorflag hits, a clear miss,
defaults, aliases/discards, output-write cutoffs and late external faults.
Full save/revision/lookup/GC/no-hash/RNG/dormant-Level checks and all40 used
source fingerprints/mtimes remain unchanged. These dormant controls do NOT
prove an actor-hash hit, melee damage, startup or headset gameplay.

Actual begun Training attacks separately pass277 through their original
HandToHandAttack/TraceFire/PlayerPawn.AdjustAim path, next refusing native531
PickTarget at AdjustAim180/opcode98. Crowbar206070 instructions/14982 writes;
knife205995/14977. Full root undo/all41 original source checks pass. No hit,
TakeDamage or returned/completed animation phase is inferred from that prefix.
Logs: `training-actor-trace-corrected-*20261024.log`.

The first attempts exposed an overly conservative diagnostic memory precharge:
Fire reserved8MiB, while every inventory snapshot reserved128 link records
despite actual chains of three. The snapshot now first authenticates the
bounded chain into its fixed128-pointer array, reserves only the actual count,
then copies with separately charged work and chain-stability checks. Fire's
fully admitted observation cap is tightened to4MiB. No query/state budget is
raised; the32MiB combined limit and all native-work/write checks remain intact.
This changes detached diagnostics, not the original player's inventory or UI.
Final coherent desktop build/shared CTest passes184 executed/219 registered,
35 explicit owned-data skips, zero failures in87.99s. Strict ARM64 syntax for
the final runtime/VM/mixed-packet/owned-Trace sources and Android native link
pass. Crowbar repeat2 reaches the exact531 boundary above. Two write-limit14979
runs stop after the first Trace output store at AdjustAim18/opcode97:
206046 instructions/14979 writes, VM write limit; both restore partial outputs
and all earlier pickup/equip/attack state, with full-root undo/all41 sources
unchanged. Equip-only repeat2 retains native1463 at ConfigurationChanged606,
221615 instructions/24458 writes. No replacement APK is made for this checkpoint.

Logs: `training-actor-trace-{full-build,ctest,final-syntax,crowbar-repeat,
crowbar-output-writecut,equip-regression}-20261024.log`.

Fresh2026-10-25 no-command startup repeat2 retains original HUD1463 at
ConfigurationChanged606/opcode101,218693 instructions/24284 writes, full-root
undo/all41 unchanged sources. Fresh crate controls347/two genuine contents
refusals and Doctor animation70/six dormant prefix refusals pass. Those
controls retain their documented limits: they do not prove surviving loot,
active combat callbacks, full Tick or playable startup.

## Original static audit

Read-only first-Training follow-on audit,2026-10-24. Actual knife/crowbar
attacks now reach native277 at `Engine.PlayerPawn.AdjustAim` logicalPC18,
after original GetAxes229 and ComputeProjectileStart. This is the next missing
wrapper, not a completed trace/hit/crate damage or live input implementation.
No original commercial DLL/EXE was loaded or executed.

Owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Static export name index2008 is execTrace; its RVA1005 thunk at10301005 jumps
to103E3330..103E354A. This is AActor's script wrapper, not UPrimitive.LineCheck
or ULevel.SingleLineCheck itself.

Actual `Engine.Actor.Trace`: native277, flags401, logical30; canonical
ordered fields: HitLocation Vector180, HitNormal Vector180, TraceEnd Vector80,
TraceStart Vector90, bTraceActors Bool90, Extent Vector90, ReturnValue
Actor Object580. Only the six input/OUT fields have NativeParm tokens at
logical0/5/10/15/20/25. Authenticate complete source declaration, canonical
types and property order; the two OUTs are not initialized ordinary inputs.

## Exact Step and publication order

- HitLocation: clear GProperty at103E3344; Step with separate uninitialized
  Float3 at SP+2C. Retain its resulting address, or that disposable temporary
  if no address. HitNormal similarly clears GProperty at103E3382, Steps to
  SP+38, and retains address/disposable storage. Do not read old output values.
- TraceEnd: Step to uninitialized Float3 at SP+44 (103E33B0..33C9), completely
  copied before later expressions. Undefined/partial input must refuse, not
  default to zero. All arguments use Frame.Object, while query source/receiver
  remains the wrapper's actual Actor.
- Immediately AFTER TraceEnd Step, seed TraceStart from live receiver Location
  atActor+114 (103E33CC..33EC), then Step it at103E33F0..3404. Later effects
  cannot retroactively alter that captured default. Supplied Nothing consumes
  this initialized default; trailing optional placeholders must reproduce the
  same original seed boundary rather than a call-entry snapshot.
- AFTER TraceStart Step, seed bTraceActors from live receiver bit0 atActor+1DC
  (103E3407..3415), then Step (103E3419..342D). This is the audited
  bCollideActors default, not unconditional true/false. Confirm its native
  field mapping against existing registration before implementing a provider.
- AFTER that Bool Step, initialize Extent to positive-zero Float3
  (103E3435..344C), then Step it (103E344D..3461). Advance past EndFunctionParms.
- Construct a complete local FCheckResult at original SP+50: Next/Actor,
  Location, Normal and Primitive zero; Time1; Item-1 (103E3469..34D4).
  Pass actual XLevel atActor+AC and source Actor, captured End/Start/Extent,
  flags `(bTraceActors ? 0x31 : 0)+6`, and extra byte0 to native virtual+C0
  at103E3501. This is the existing original SingleLine selection service;
  retain its caller-seeded miss semantics and actual Actor/Level/hash identity.
- Ignore the SingleLine Boolean. Copy Hit.Actor to the native return pointer
  FIRST (103E3507..351A), then Hit.Location to captured HitLocation
  (103E351C..352D), then Hit.Normal to captured HitNormal (103E3530..3542).
  OUT aliases legitimately retain the later Normal copy. The VM's returned
  Object value is detached; do not fabricate a hit when the query refuses.
  A genuine miss returns Object None and the seeded zero Location/Normal.

## Existing service and remaining adapter work

`portable_retail_single_line.inl::RetailSingleLineLevel` already binds actual
current native Level/Actor ownership, root-owned hit marks, live filtering,
geometry and same-root budgets. It is not yet native277's script wrapper.
Do not replace it with the presentation pointer ray/target sphere or existing
Pawn-only health shortcut. TRAINING-SINGLE-LINE.md records its separate tests.

Native277 needs mixed address-only OUT plus sequential ordinary-input seeds.
The new Core229-only combination presently rejects whole-call sequential
prefix capture when address-only slots exist. Extend that combination with
explicit disposable markers for OUT prefix slots; never Load those old OUTs
just to build the prefix. Preserve addresses through later effects, and keep
full TraceEnd copying/optional Step omissions before authentic collision work.
This must cover both raw/native-declaration and direct calls with the same
budgets, lifetime guards, first-failure poisoning and ordered-output undo.

Further reached services (e.g. autoaim/PickTarget531, TraceShot and hit effects)
must be discovered from actual runs, not assumed bypassed or already done.
VR controller aim/player publication remain separate live integration work.
