# Original dry Actor falling, first Training integration

This increment implements an explicit original `AActor::physFalling` timestep
for finite, dry, non-Pawn/non-bobbing actors. It is not the full world scheduler,
`performPhysics` rotation/PendingTouch epilogue, or on-headset acceptance.
Only `Maps/00_Training.dx` is used for actual-map integration.

## Original authority

Static owned Engine.dll body103D0A50..103D248E is the authority, not the pinned
engine's different fixed-step/acceleration implementation. Its SHA256 remains
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
No commercial DLL/EXE is loaded or executed. See
`ORIGINAL-TRAINING-CRATE-FALLING.md` for reproduction, class identities and
field-layout corroboration, and `ORIGINAL-DECORATION-LANDING.md` for landing.

Additional original static evidence recovered for this implementation:

- `Actor.bIsPawn` is +28 bit00800000, not an invented drift flag. Ordinary
  non-Pawns include actual stored ZoneVelocity. A flagged non-PlayerPawn uses
  it only when its retained squared magnitude is strictly greater than40000.
- Native Actor copy103244BF..10324569 copies consecutive post-light Bool
  masks1/2/4/8/10/20/40/80/100 at+1EC, followed by DodgeDir at+1F0.
  Together with the actual property chain and Core Bool.Link this authenticates
  bBounce=10 and bJustTeleported=100. Their metadata flags are21 and2.
- The adaptive remaining-time test uses original **Double**0.1. Thus supplied
  **Float**0.1 is above that threshold and splits into two Float0.05 steps.
  Remaining>0.1 selects min(Float0.1, extended remaining*Float0.5); the
  signed iteration bound is8. There is no invented fixed0.02 step.
- Apex crossing requires opposite `>0` classifications and both absolute
  vertical speeds strictly>5. A retained ratio splits both durations; both
  must be strictly greater than Double0.015. The first duration is spilled
  as Float but retained for its comparison. The second is added to remaining
  before its one Float spill. Failed duration tests still retain apexFlag1.
- After real MoveActor, deletion returns immediately. Collision with actual
  PlayerPawn decrements a Decoration's signed counter with wrapping DWORD
  subtraction, then clamps nonpositive results to0.
- Ordinary landing uses Normal.Z>Double0.7. Unspent time is retained through
  `(1-hit.Time)*dt+remaining` until its Float store. Impact Velocity is rebuilt
  from real displacement only if not freshly teleported, hit.Time>Double0.1,
  and unspilled hit.Time*dt>Float0.003. No drift is subtracted on this branch.
- On ordinary no-hit completion, rebuild Velocity from stored Float
  displacement times retained `1/dt`, then subtract the captured zone drift.
  Apply stored `2*newVelocity-oldVelocity` only if newVz<oldVz or oldVz>=0.
  SizeSquared is spilled as Float before comparison with unspilled terminal
  squared. A clamp uses original Core.SafeNormal, then freshly rereads the
  actual **ZoneTerminalVelocity** and applies scalar XYZ stores.
- Imported Core identities:10599B28 FVector constructor;10599B80 SafeNormal;
  10599CAC SizeSquared;10599CA8 operator*=;1059A070 operator-.
  SafeNormal10144BE0..10144C5E retains the PC64 root; it is not guarded
  Normalize, whose tiny-vector behavior differs.
- The zero ZoneNumber callback name is independently authenticated by original
  FellOutOfWorld export/name address10583608. Actual Engine.Actor event flags
  are802, not800; its original script calls SetPhysics0 and Destroy.

## Implementation and native storage

`native/quest_actor_falling.h` borrows live providers and the caller's work
ledger. It preserves original binary32 constructor spills with existing exact
x87-PC64 dyadic helpers, adaptive subdivisions, apex resplitting, actual sweep,
landing handoff and terminal clamp. There is no local undo or fixed map gravity.
Active unimplemented Pawn, water, bobbing and non-bounce wall/slide
branches explicitly refuse and propagate to the outer rollback.

The subsequent original selected-HitWall bounce integration and genuine
first-Training debris diagnostic are recorded in TRAINING-FRAGMENT-BOUNCE.md.
They retain the same dry timestep and caller-owned rollback contracts; they
do not add the performPhysics rotation/PendingTouch epilogue or world scheduler.

`native/portable_actor_falling.inl` binds the coordinator to actual same-Level
objects, stored Region.Zone, real zone properties, MoveLevelActor, selected
Landed and original SetPhysics. It adds a private exact-schema OldLocation raw
read capability without weakening general VM/save Vector validation. Native
OldLocation, Velocity, Acceleration and bJustTeleported stores use the shared
journal without restarting animation clocks. All three Vector-word references
are admitted before each original XYZ store group. Original early landing or
deletion does not restore Acceleration; normal loop completion does.

The landing coordinator now also implements the original basic non-Pawn,
non-Decoration path used by dropped inventory: skip Decoration counters/fit/
Carcass providers, selected Landed, fresh Physics, SetPhysics0(original floor).
This does not claim all original Inventory falling/pickup gameplay is complete.

## Actual-map observation workflow

`portable_training_startup_probe --owned-source-root OWNED_GAME_ROOT
--native-work 1048576 --fall-training-crates` requests bounded original timesteps on the genuine
lower and upper Training crates after real Login returns JC and before normal
pairing/Possess. It never forces Physics, flags, support, velocity or position.
It stops requesting steps only after an observed native step stops/removes its
actor. `--sweep-training-crates` remains a separate explicit-delta diagnostic
and is mutually exclusive. Normal startup requests neither.

Detached rows retain real pose/V/A/OldLocation, stored zone, Physics/Base,
teleport flag, full animation clock, hit/floor, selected event, native operation
counts and shared write prefixes. `--repeat 2` compares every observation.
`--damage-write-limit` can interrupt inside falling and must preserve whole-root
undo, including completed earlier timesteps. The tool also checks all41 owned
source fingerprints. Exit2 still means the later original startup gap, not
accepted startup or published gameplay.

## Verification

Strict ARM64 syntax checks of runtime, CLI and both helper tests pass; Android
native compilation/linking passes. The independent dry coordinator fixture
passes26892 checks/457 refusal controls. The full coherent desktop build passes;
shared CTest executes176 of208 registered tests, with32 explicit owned-data
skips and zero failures in44.83s. The owned integration probes above run
separately, not through those skips. Evidence: ignored
training-native-falling-full-build-20261023.log and
training-native-falling-full-tests-20261023.log.
The separate owned first-map crate regression passes347 checks/two genuine
contents refusals, with no dormant-fixture claims of surviving loot. See
training-native-falling-crate-regression-20261023.log; surviving actual-world
loot is instead proved by the combined original startup-root probe below.
The separate owned SetPhysics regression passes172882 checks/53 explicit
refusals, including19 exact partial-write cuts and real Doctor animation-clock
preservation: training-native-falling-physics-regression-20261023.log.

The first actual-map discovery reaches all four requested observations:

| Actor / timestep | Actual Z before -> after | Actual vertical speed after | Outcome |
| --- | --- | --- | --- |
| Lower /0 | -103.899864 -> -103.899864 | 0 | real LevelInfo floor, Physics0/Baseworld |
| Upper /0 | -16.8998642 -> -21.6498642 | -95 | two native substeps, Physics2 |
| Upper /1 | -21.6498642 -> -35.8998642 | -190 | two native substeps, Physics2 |
| Upper /2 | -35.8998642 -> -55.3848038 | 0 | real lower-crate collision, selected Landed, Physics0/Baselower |

There are seven actual MoveActor calls and two ordinary landings. The upper's
last actual hit has Time0.673488081 and Normal.Z1. Full animation clocks are
unchanged across every captured step. Writes14762->14797->14844->14891->14956
are retained in one root. Normal continuation still reaches the original
DamageHUDDisplay.InitWindow PC39 nested Name-array refusal, with218631
instructions/24386 writes and493826 of1048576 native work on this cold run.
The whole startup root rolls back and all41 originals remain unchanged.
This is explicit timestep integration evidence, not a completed startup.

Combined gravity/destruction/original-player-pickup repeat2 retains identical
observations and224441 instructions/24807 writes, with514186 cold/513559 warm
native work. Both crates execute10->4->-2, each leaves six WoodFragments/one
genuine surviving Lockpick. Before damage the lower is stopped/Baseworld;
actual lower destruction releases the based upper into Physics2/BaseNone,
with its original10HP intact. This does not yet step that released upper to
another landing. Damage writes are14956->14958->15123->15125->15267; original
first-fit pickup uses15267->15337, and original second-item merge uses
15337->15375. The player grid/full Pickup clock remain unchanged on merge.
Whole-root undo and all41 sources pass.

Repeated write14946 cutoff occurs inside genuine DeusExDecoration.BaseChange
PC0/opcode20 while the upper establishes its support. The lower landing and
two complete upper timesteps remain captured; the final upper step is entered
but NOT returned/captured. Default after-values in that partial row are not
observations. Both repeats stop with203097 instructions/14946 writes,
464914 cold/464372 warm native work, and fully undo completed plus partial
effects. Normal no-command startup still has no landing/falling/damage/loot
rows and retains218253 instructions/24192 writes/native491158 at the same HUD
boundary. All diagnostic exit2 results are refused startup, not accepted play.

Logs in ignored artifacts/: training-native-falling-discovery-full-budget-20261023.log,
training-native-falling-combined-repeat-20261023.log,
training-native-falling-late-repeat-20261023.log and
training-native-falling-normal-20261023.log. The initial default65536-work run
stopped before Login/physics; it is not gravity evidence.

## Still required

Follow-up actual released-crate/Inventory stepping and pickup evidence is in
TRAINING-RELEASED-FALLING.md. Both dropped Lockpicks now settle through actual
timesteps and remain collectible. Released upper contact with an unadvanced
airborne Lockpick demonstrates the remaining interleaving/support-carry gap;
it is not final stack/world settling or an automatic scheduler.

Automatic world dispatch, rotation/PendingTouch, remaining reached physics
branches, falling after actual support destruction, dropped loot's full
fall/settle path, original equipped-weapon/controller interaction and committed
player/startup integration are not established merely by these diagnostics.
The original later Possess/HUD failure must remain visible and roll back the
entire root. No partial startup publication or unrelated HUD redesign is used
to bypass it. No APK/headset feature is claimed by a backend-only native build.
