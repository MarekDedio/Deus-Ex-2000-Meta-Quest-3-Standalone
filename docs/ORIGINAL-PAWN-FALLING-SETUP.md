# Original Pawn falling setup and retained horizontal cap

First-Training implementation foundation, not completed Pawn landing/Walking,
playable startup, surviving crate loot, automatic stacked-crate gravity or
headset acceptance. This connects actual falling setup rather than skipping
NPCs or treating a Pawn as a Decoration.

## Original source authority

GNU objdump statically reads owned `Engine.dll`; the existing port package
inspector and bounded Children/Next reader read `Engine.u`. No commercial
EXE/DLL is executed, loaded as a module, linked, patched or rewritten.

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |

Machine addresses/offsets below describe the original preferred-image PE32,
never portable C++ object offsets. The complete Core linking/packed-Bool
method and independent Actor318/Pawn5D4 registration anchors are in
ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md. Fresh owned Pawn Children/Next
traversal, keeping the previous property across non-property nodes, confirms
GroundSpeed378/ref302/next1304, AccelRate384/ref4030/next176, AirControl390/
ref3979/next1978. All are actual Pawn scalar FloatProperty/dim1/flags21/base0/
no subtype. Runtime authenticates those qualified names, types and owners;
no native-layout reinterpret cast or matching renderer field is used.

## Captured Pawn and legitimate negative preprocessing

Original physFalling103D0A7D..103D0AA9 captures its actual APawn ancestry once.
A nonnull Pawn calls103CC450 before even ZoneNumber. Inside that preprocessor,
actual PlayerPawn Class1058C160 walks receiver Class/Super at103CC4A6..103CC4C9.
A genuine nonplayer NPC jumps to103CC9E6, returns AL0 and performs no world
stores, traces or callbacks. The runtime represents that exact negative arm
using its bounded authenticated PlayerPawn cast. A real PlayerPawn positive
arm remains an explicit unsupported service, not a fabricated false result.
A handled preprocessor returns without reading Zone/capturing Acceleration.

After legitimate continuation, the existing ZoneNumber path remains. Nonzero
ZoneNumber captures raw original Acceleration.X/Y/Z before Pawn-specific
setup103D0BD0. That captured vector restores only at normal iteration/time
exhaustion, not after landing, deletion, swimming or a handled preprocessor.

## Setup and predictive collision query

`quest_pawn_falling_setup.h` models the original103D0BD0..103D0E0C plus its
103D0EA0 high-speed detour. Source Float/Double spills and retained PC64
arithmetic use NativeScalar, with no host FMA or float-only substitution.

1. Raw-capture AirControl. Only ordered `control > Float0.15` predicts a hit.
   Form Float predictiveLimit=control*fresh AccelRate; call actual imported
   Core.SafeNormal on current Acceleration. Prediction's Y product remains
   PC64 through adding fresh Velocity.Y and multiplying whole delta. X product
   spills Float before adding fresh Velocity.X; that sum spills Float again
   before delta multiplication. Both displacement products spill Float.
2. Read radius raw, height raw, then radius again through FLD/FST. Query
   extent X uses that loaded/quieted Float, Y the first raw radius, Z raw height.
   Form end from Location.Z/Y/X, adding displacement to XY with Float spills.
   Start is the actual Location vector (fresh XYZ copied at provider boundary).
   Actual XLevel virtual+C0 receives source, hit packet, end, start, flags6,
   cylinder extent and extra0. Ignore its signed INT; only nonnull hit.Actor
   changes retained control to Float0.05. Subsequent rate/velocity reads stay
   fresh. Do not use an always-clear result, line-of-sight Boolean or fake floor.
3. Multiply retained control by fresh AccelRate. Raw-store Acceleration.Z=+0,
   read Velocity.X, spill limit Float, read Velocity.Y and raw Velocity.Z.
   Compute retained Y²+X², spill Double and call actual appSqrt. Core thunk
   101032BA to10123290 is FLD Double/FSQRT/RET, retaining its PC64 result.
4. Speed<10 OR unordered takes Float limit=(Float10-retainedSpeed)/wholeDelta
   + oldFloatLimit. Otherwise compare retained speed with fresh GroundSpeed.
   Below/unordered ground speed keeps the current limit. At/above GroundSpeed,
   ordered control>Float0.05 captures retained speed as Float horizontal cap;
   control<=0.05/unordered instead sets acceleration limit Float1.
5. Compare retained limit² with current Acceleration X²+Y²+Z². C0 selects less
   OR unordered. If selected, call SafeNormal again on fresh Acceleration;
   commit its normalized XYZ first. Then reread those stored fields Z/Y/X,
   hold scaled products, preserve Float spill/store order and commit scaled
   XYZ. This is not one final vector assignment; all real stores are journaled.

Core.SafeNormal import10599B80 reaches10144BE0. Existing
ActorLineTraceDetail::Normal(value,true) matches the independently audited
finite path: retained squared sum, Double argument spill, threshold comparison,
retained sqrt/reciprocal and Float XYZ stores. Active nonfinite normalization
remains explicitly unsupported through that provider. This implementation does
not claim that finite normalization reuse implements Core's exceptional arms.

## Retained speed cap across substeps

The local horizontal cap starts+0 at103D0BA5. Keep the setup result through
all substeps. Block103D1A7B..103D1B39 runs after provisional/apex handling but
before drift/movement. C3 against Float0 skips either signed zero and NaN
without demanding Velocity/normal/store providers. Otherwise read raw current
Velocity XYZ; overwrite local Z=+0. C0 of cap² versus retained X²+Y² selects
clamp for less/unordered. Normalize that horizontal local, scale/spill Z/Y/X,
then read fresh raw source Velocity.Z before any final actor store. Write
scaled X/Y and that raw Z unconditionally. Preserve signaling-NaN Z bits;
do not normalize vertical velocity or cache Z before the normalization call.

## Shared falling integration and swimming handoff

The shared Actor falling coordinator now carries captured Pawn context,
the preprocessing/setup hooks and retained cap. Its original non-Pawn branch
does not demand any of these inactive providers. All helper/provider effects
belong to the same Level Execution, write journal and work budget; no local
commit/undo or alternate world scheduler is added.

Entry/class-only reads use authentic LifecycleActor/ZoneLevel/ZoneActor and a
bounded, metadata-validated native Class/Super walk, not RetailCollisionActor's
eager Engine/Cylinder preparation. The original entry has no such collision
call. Reached RetailSingleLineLevel and MoveLevelActor retain their unchanged
real collision admission; this is not permission to fabricate a collision
world for the zero-time fixture. Location providers retain existing guards.

After actual MoveActor/deletion, a captured Pawn must read fresh Physics.
Mode3 at103D1CAC..103D1D08 computes/stores remaining time as
`(Double1-hit.time)*substepDelta + oldRemaining`, passes raw old-substep
Velocity, current substep delta, new remaining Float and current iterations
to APawn.startSwimming10302B76->103D2B70, then returns without restoring
Acceleration. That service is currently an explicit reached refusal, not a
fall-through into collision/landing. Pawn processLanded/Walking and nonbounce
wall/slide remain separate original services; no Decoration alias is admitted.

## Verification scope

Focused tests cover provider ordering/packets, hit-Actor versus native-INT
selection, fresh rate reads, normalized and scaled stores, speed/ground/control
boundaries, raw fresh Z, missing providers, every reached write/work cutoff
and explicit mock-caller undo. Their x87 oracle executes only its own
instructions, not production scalar code or a commercial module. Shared-step
tests cover preprocessor early return, genuine setup/cap integration and the
swimming packet/early-return/refusal. Generated production controls use an
authentic zero-time Physics2 branch to verify typed setup/stores/checkpoint
without pretending to supply missing sweep authority. A nonzero-time actual
first-Training run remains mandatory and distinct from those fixture controls.

Current focused evidence: setup/cap101,371 checks/79 refusal controls and
9,344 independent x87 cases pass; shared Actor/Pawn coordinator60,161 checks/
980 refusal controls pass. The first cap assertion was corrected using exact
independent Float arithmetic: stored Float0.6 times Float25 stores41700001,
one ULP above15; negative storesC1700001. Production math and the fresh raw-Z
check were not changed to force a passing assertion.

Final runtime verification: generated ScriptedPawn section2,765 cumulative/
46 refusal controls and full driver7,637/142 plus its final malformed-schema
case pass. Typed zero-time Physics2 setup/restore/checkpoint and late budget
undo controls pass. Full desktop/Android-native/strict ARM64 builds/checks and
CTest254 registered/215 executed/39 explicit owned-data skips/zero failures
68.04s pass; separate owned crate415/5, deferred429/21 and animation588/24 pass.

The nonzero-time actual first Training repeat2 passes this falling work and
reaches `Original Pawn physicsRotation is not implemented`, identically at
206,922 instructions/15,829 writes with whole-root undo. Paired doors repeat
207,258/15,897 and preserve two56-unit moves/two completion callbacks before
that same gate. Separate two-Fire repeat2 retains228,837/26,047 and its genuine
crate/loot/debris observations before whole undo at later FExec. All45 checked
originals and7 final compiler inputs are unchanged; all jobs are terminal.
None of these expected-error tests proves a committed Training startup, live
controller melee, surviving collectible loot, automatic gravity or headset
acceptance. Next actual Pawn rotation is recorded in ORIGINAL-PAWN-ROTATION-GAP.md.
