# Original ScriptedPawn Tick in the first Training world update

Implementation checkpoint, 2026-10-26. This advances the existing world-update
path; it is not a completed NPC simulation, a playable crate/loot/gravity
milestone, a new account screen, or a headset acceptance result.

## What changed

`native/portable_level_actor_tick.inl` selects the actual nearest native Tick
before entering base stasis, owner deferral, parity or animation. A genuine
`DeusEx.ScriptedPawn` now runs `TickOriginalScriptedPawn` through the typed
providers in `native/portable_scripted_pawn_tick.inl`. Unknown nearer native
classes remain unsupported. Selection validates the entire bounded actual
Class/Super chain, not just the first native name.

The wrapper preserves the original remote-role/role/tick-type/simulated gate,
render-age Float spill, disappearance, nonuniform PrePivot interpolation,
agitation/fear decay, countdown ordering, selected cloak/fire/blood callbacks,
tactical teardown, visibility-gated bleeding, and the actual qualified
`AActor::Tick` return. The qualified base entry is private and shares the same
Level Execution; it does not recursively dispatch the override, open a second
transaction, force return1, or grant Pawn physics authority.

The original base Pawn continuation still raises
`Original Actor Tick Pawn continuation is not implemented`. That is now after
the custom work, base prefix and animation, rather than a refusal before the
custom wrapper. Full-frame failure still restores earlier actors, timers,
arrays, callback changes, native bookkeeping, saved state and allocations.

## Arithmetic and actual metadata

`native/quest_native_scalar.h` represents retained ordinary masked,
nearest-even x87 PC64 values using integer dyadics. A raw memory operand and
an FLD are distinct: only the latter quiets a signaling NaN before the binary
operation. Explicit Float and Double stores reproduce the original spills;
infinities, signed zero, unordered predicates and NaN payload selection are
not replaced by finite-only geometry rules. The retained appSqrt result follows
the actual Core `FLD m64; FSQRT; RET`, including its dead caller Double spill.

The native helper and adapter preserve the independently established field
owners, flags, dimensions, enum/object constraints and exact four-member
`AlliancesEx[16]` schema. Alliance eligibility reads a Name identity, not an
Actor pointer; updating agitation preserves AllianceName, AllianceLevel and
bPermanent. Descending slots, fresh reads and original store/clamp order are
retained. Every reached service charges the same caller work budget.

The compatibility class walk's second literal target at `10019642` is
**DeusEx.DeusExPlayer**, PrivateStaticClass `1003AAD8`, not ScriptedPawn
(`1003C020`). Independent export-table and owned metadata checks establish
`DeusExPlayer -> Extension.PlayerPawnExt -> Engine.PlayerPawn -> Engine.Pawn`.
The helper now asks for `nativeDeusExPlayerClass`. A genuine ScriptedPawn
instead derives directly from Pawn, so it never enters the first PlayerPawn
arm. The adapter explicitly refuses the still-unrepresented PlayerPawn word
at +5D4 if a caller somehow reaches that arm; it does not invent its value.

See `ORIGINAL-SCRIPTEDPAWN-RUNTIME-BINDING.md` for the full property/array and
callback packet authority, `ORIGINAL-SCRIPTEDPAWN-DECAY.md` for native decay
load/store evidence, and `ORIGINAL-SCRIPTEDPAWN-TICK-PREREQUISITES.md` for the
static vtable/instruction/layout audit. No commercial DLL/EXE is loaded or
executed by these implementations or tests.

## Selected callbacks

The adapter performs checked, selected-node-aware function lookup and retains
the real authored UFunction. It applies Actor virtual/Level ownership,
bBegunPlay, scriptability, selected probe, pending kill, native-index and RPC
gates in the original order before executing that retained function in the
same caller root. EnableCloak accepts exactly one scalar Bool packet, including
false; ExtinguishFire and SpurtBlood accept no parameter/result packet. Locals
are allowed. A missing reached native dependency still refuses normally.

The generated production controls use an actual authored EnableCloak function
and verify both true and false input packets, rather than substituting an
empty provider. Full SpurtBlood/ExtinguishFire gameplay acceptance is not
claimed from their independent provider tests.

## Verification scope

`quest_native_scalar_test` independently compares retained 80-bit results,
Float/Double spills, reverse memory operations, chains, exceptional values,
appSqrt and extended-range behavior against its own host x87 instructions.
It never calls original machine code. The decay test independently checks
complete native helper/store schedules against its own x87 oracle. The Tick
test also covers source order, raw word copies, callback mutation, lazy
providers, exact base returns and caller work failures.

`portable_level_actor_tick_test` uses real generated package/Class/Property/
Function metadata and the production Level driver. A four-actor owner cycle
makes all four genuine base bodies return0, so the ScriptedPawn prefix can
commit before owner deferral without fabricating a successful Pawn suffix.
Controls check typed timers, vectors, Name clearing, real cloak bytecode,
alliance eligibility/clamping, untouched sibling values and flag behavior.
Separate late-Pawn, missing-field, work/write-cut and repeat controls require
complete root undo. These generated scenarios do not prove that the real
Training world frame or live crate gameplay completes.

The real first-Training diagnostic must independently run the unfiltered
actor phase with the owned assets and compare repeats/full undo. The crate,
deferred-ownership and animation regressions and the two-original-crowbar-
attack probe remain separate. Final build/test/probe counts are recorded in
`STATUS.md` and `TRAINING-FIRST-PRIORITY.md`; unexecuted tests are not passes.

No other map integration, packaged APK, on-device capture, or completed
destruction/collectible-loot/automatic-gravity claim follows from this change.

## Next actual Training boundary, not another-map integration

Independent static continuation review identifies the next actual Jaime route:
captured Role4 is not simulated movement; fresh RemoteRole2/Role4 takes
`103A2524`, and the real PlayerPawn cast fails for ScriptedPawn -> Pawn.
It reaches `103A2643`: tickType0 permits selected Tick/probe336, followed by
ProcessState, Actor timer, LifeSpan, then genuine APawn physics if fresh
Physics is nonzero and Role is not3. Capture the selected live StateNode/Tick
and probe before assuming a class implementation: original ScriptedPawn.Tick
has 560 script bytes and Standing.Tick has a 27-byte Global Tick override.

Possible reached script services include LastRendered723 and perception
CycleActors1002/AICanSee705/native2105; current live gates must establish the
actual first missing service. Their presence in a script is not proof of a
reached dependency. Player/input/network setup is not on the observed Jaime
route, and `nativeStandaloneLevelServices` is the actual absent-NetDriver
authority rather than a reflected NetMode guess.

For Physics2, APawn.performPhysics `103C9A70` selects AActor.physFalling
`10302FEF -> 103D0A50`, whose genuine Pawn preprocessor `103CC450` runs before
zone checks. The current falling helper deliberately refuses Pawns for this
reason: successful crate falling does not implement NPC falling. APawn also
has its own rotation call, MoveTimer/avgPhysicsTime updates and PendingTouch
epilogue. Preserve that distinction.

After physics the literal tickType0 uses the short retained-Pawn tail and
returns at `103A293C`; the sight/pain/speech/tactics tail belongs to type2 and
is not permission to add unrelated first-map work. Its live conditions still
need genuine bIsPlayer/Role/Weapon/eye-view handling. The independently linked
Bool layout places Pawn.bViewTarget at +31C bit1 (BoolProperty flags0), not an
invented bUpdateEyeHeight flag. These are next-step findings, not implemented
or verified gameplay in this checkpoint.
