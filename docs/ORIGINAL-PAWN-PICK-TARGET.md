# Reached Pawn.PickTarget531

Scope: the first Training map's real knife/crowbar attack. The prior verified
Trace277 checkpoint reaches531 at original PlayerPawn.AdjustAim180. This is
an internal weapon/aiming dependency, not account or character-creation UI.
This document does not claim live melee, crate damage or completed startup.

## Original static evidence,2026-10-25

Only immutable package inspection and static disassembly were used. No original
commercial DLL/EXE was loaded or executed. Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Original export[1962], RVA3689, jumps to103BACA0..103BB031.

Engine.Pawn.PickTarget is native531, flags401, logical20. Ordered fields:
bestAim Float180, bestDist Float180, FireDir Vector80, projStart Vector80,
ReturnValue Pawn Object580. Four NativeParm tokens occupy0/5/10/15.

- 103BACA3..103BAD3A initializes two separate positive-zero Float temporaries.
 Each Step uses Frame.Object, clears GProperty and retains the resulting live
 lvalue address, or its own evaluated temporary when no address was produced.
 A numeric non-lvalue can therefore supply the fallback threshold. This is
 NOT the address-only/discarded-output contract used by GetAxes/Trace.
- 103BAD3A..103BAD7E copies FireDir then projStart into uninitialized12-byte
 destinations and consumes EndFunctionParms. Missing/partial Vector writes
 cannot borrow a declaration zero. The Float Step copies and live addresses
 are separate: later argument effects may change an eventual live threshold.
- 103BAD81..103BADF2 authenticates source.XLevel.Actors[0] as LevelInfo and seeds
 PawnList. Follow live Pawn.nextPawn at103BB002, not the broad Actor array.
 Skip self, Health<=0 and !bProjTarget in that order. Actual native slot0.Game
 None skips that candidate. If both actual PlayerReplicationInfo pointers are
 nonnull, bTeamGame and equal Team skip the candidate. No begun/Role/nearest-
 distance/visibility success is fabricated before these reached gates.
- 103BAF14..103BAF54 subtracts Location-projStart without Float delta stores.
 Dot sums (Zproduct+Yproduct)+Xproduct in PC64, stores Float angle, and requires
 angle>0.103BAF69..103BAFA8 independently recomputes the unspilled differences,
 sums (Zsquare+Ysquare)+Xsquare and requires retained square<4,000,000.
 This is not the vendored2500-unit or inclusive-distance policy.
- 103BAFAA..103BAFD4 spills square to Double for imported Core.appSqrt, stores
 its root to Float distance, computes PC64 angle/distance, separately FSTs
 candidateAim to Float, then compares the STILL RETAINED quotient strictly>
 the LIVE bestAim address. A rounded Float tie can still qualify. bestDist
 old contents are never read by this candidate loop.
- 103BAFD6..103BAFE4 calls internal C++ Pawn.LineOfSightTo(candidate,0,false).
 Only a qualifying candidate reaches this service. It is not a selected
 script/native514 call, Actor.Trace or a three-ray vendored approximation.
- 103BAFE6..103BAFFC immediately stores aim THEN distance through the retained
 addresses per accepted candidate. OUT aliasing changes later comparisons.
 Return Pawn is stored after traversal at103BB010..103BB031, not before OUTs.

Actual schema inspection confirms Health flags10021; PawnList/nextPawn flags2,
Game0, PlayerReplicationInfo20, Team20, bTeamGame1, bProjTarget21. Native offsets
are only evidence, never portable object offsets: Health+428, PawnList+54C,
nextPawn+498, Game+544, PRI+558, Team+340, bProjTarget Actor+1DC bit4 and
bTeamGame GameInfo+328 bit9. Reused property readers authenticate the original
declaring schemas, class constraints and current native Level ownership.

## Current implementation boundary

The adapter binds the complete declaration, copied Vector inputs, initialized
Float temporaries plus guarded live aliases, actual PawnList/team gates and
the scalar candidate evaluator. Existing QueryPreferredSize1465 capture keeps
its former behavior; no public VM header/API was changed. Internal Capture
reads each supplied531 Float once at its Step and keeps the alias. Candidate
threshold reads and each ordered store belong to the handler/shared journal,
not a final detached copy-out operation.

A candidate that reaches internal LOS explicitly refuses. Returning false,
assuming PlayerPawn.bIsPlayer, using a pointer-sphere or dispatching514 would
silently replace original behavior. Empty/filtered lists may legitimately
return Pawn None without any LOS query. This is a faithful selection PREFIX,
not a complete531 implementation or successful visible-target proof.

Portable safety policy remains explicit: traversal refuses cycles or more
than4096 distinct Pawns and admits4096 pointer slots of scratch under the
existing combined state budget. Native work meters reached Pawn iterations;
the bounded linear visited-pointer comparisons are not individually metered.
VM ingress requires finite active Float/Vector values, and mandatory Float
overflow or undefined zero stored distance refuses instead of manufacturing
an aim. These guards are not inferred retail handling of malformed/nonfinite
data, an expanded world budget or a claim of unrestricted compatibility.

Internal LOS export[1273], RVA24E1, jumps to103BEB50..103BF522. Even with the
fixed arguments0,false it has source bIsPlayer/target Pawn/Visibility/range,
BaseEyeHeight, Enemy and collision-height/corner-ray branches. Non-player
sources can consume FRand. Enemy success can write LastSeeingPos then
LastSeenPos at103BF4C7..103BF50A. Exact Point/Model ownership, live reads,
native work/RNG and these stores must share the existing execution/undo.
NativeFastLine is the existing directed actual Model.FastLineCheck provider;
it is not by itself the full LOS policy. The full internal LOS service remains
the next dependency if reached, rather than a blanket refusal before the loop.

## Reproduction and limits

Static evidence logs are in artifacts/training-pick-target-{script,native,
property}-audit-20261025.log. The inspector now also accepts actual property
paths, avoiding interpretation of a UProperty as a UFunction.

```powershell
& artifacts/prerequisites-20261009/build/script_bytecode_inspect.exe 'D:/Steam/steamapps/common/Deus Ex' 'Engine.Pawn.PickTarget' 'Engine.PlayerPawn.AdjustAim' 'Engine.Pawn.Health'
& 'C:/ProgramData/mingw64/mingw64/bin/objdump.exe' -d -Mintel --start-address=0x103BACA0 --stop-address=0x103BB032 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
```

## Verification checkpoint

Final scalar controls pass157625 checks/21 portable cases and55014 independent
test-owned hardware x87 cases, with8725 matched lazy bestAim reads and7537
explicit refusals. The oracle executes only our test instructions, not the
commercial DLL. Scoped mixed Float/OUT VM controls pass1338 checks/21 refusals;
existing mixed Trace/GetAxes1205/32 and full VM696084/897 also pass.

Review caught an extra alias read when a captured Float forwarded through
Context/ClassContext/Skip. The fixed internal forwarding marker preserves both
the first Step snapshot and live alias without rereading at every nesting
layer.56 added raw/declared/nested/same-execution controls exercise both empty
and accepted mock candidate cohorts. They prove the VM packet/journal contract,
not genuine LOS, NPC target selection or crate damage.

Both actual weapon discovery runs now enter531 and reach its internal LOS
dependency at AdjustAim180/opcode98: crowbar206070 instructions/14982 writes,
knife205995/14977. Corrected crowbar repeat2 and knife retain that exact boundary
and full-root undo/all41 unchanged owned sources. Internal LOS is NOT bypassed.
Final desktop/strict ARM64/Android native builds pass, with a separately fixed
GCC misleading-indentation error in the new test only. Final coherent shared
CTest passes186 executed/222 registered,36 explicit owned-data skips, zero
failures in43.95s. Fresh actual Trace277 controls retain43 calls/37063 checks/
23 refusals and all9 warm native-work cutoffs. No new APK or Quest acceptance
is implied.

Owned first-Training531 controls pass181 production calls,277720 checks/90
refusals and all4 reached warm native-work cutoffs (including the cycle-prefix
cut), with exactN successes. Authentic AddPawn produces empty/self/nonself
Game=None lists and then a two-Pawn cycle; three explicit fixture publications
and one validated original checkpoint Load leave the original bytes/links
restored. Every query/refusal preserves its complete fixture save/revision/
lookup/GC/no-hash/RNG/dormant-world state. All40 used owned sources retain
bytes/size/mtime. These controls do NOT create GameInfo, begin play, accept LOS,
select a visible target, damage anything or claim headset acceptance.

Fresh no-command startup repeat2 retains exact HUD1463 at ConfigurationChanged
606/opcode101,218693 instructions/24284 writes, full-root undo/all41 sources.
Logs: training-pick-target-{owned-controls,crowbar-repeat,knife-final,
default-repeat}-20261025.log. The completed diagnostic remains unsupported
startup; matching an expected refusal never turns it into a playable world.

Fresh first-map native279 crate controls347/two genuine contents refusals and
Doctor main-animation70/six dormant refusal prefixes pass after the final
shared link. Their limits remain unchanged: Game=None deletes spawned contents,
dormant begun gates suppress active script callbacks, and neither is a live
crate/loot/gravity acceptance witness. The final inspector handles the real531
function and Health10021 property in one read-only invocation.
