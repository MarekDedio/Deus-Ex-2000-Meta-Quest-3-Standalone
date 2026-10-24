# Reached Actor.Trace277 wrapper

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
