# Original non-Pawn post-animation Tick phase

First-Training crate/loot/gravity work, 2026-10-25. This is an implementation
of the original per-actor **post-animation phase**, not a completed world Tick
or a claim that the Quest trigger now destroys crates.

## Original authority

Read-only static inspection of the owned GOTY `System/Engine.dll` agrees with
[the broader Tick audit](ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md):

- `103A23CE..103A23E0` reads RemoteRole and then Role. RemoteRole3/Role4 runs
  ProcessState and Timer at `103A249F..103A251C`, then bypasses normal expiry
  and physics. Other RemoteRole3 roles skip this whole phase.
- For RemoteRole!=3/Role>=2, literal ELevelTick1 returns before the ordinary
  event/state/timer/expiry/physics work. Otherwise `103A2653..103A2709` does
  live Tick probing, checked selected Tick with one copied Float DeltaTime,
  original ProcessState, then the existing original timer block.
- `103A270B..103A2757` reads LifeSpan after callbacks. Initial signed zero
  **and unordered NaN** skip subtraction: the native branch tests C3. Active
  finite arithmetic uses the existing portable PC64 dyadic implementation.
  `FST` at `103A2721` stores Float **without popping** the extended subtraction.
  The following `FCOMP` compares that live PC64 result, not the rounded stored
  Float, against the binary64 `0.0001` bytes `2d431ceb e2361a3f` at `1042AC40`.
  A reached result <= that threshold runs checked selected Expired, then real
  Level.DestroyActor(source,force0), without rereading LifeSpan/deletion.
- `103A275C..103A2783` reads fresh Physics and, only if nonzero on the normal
  branch, fresh Role. Earlier callbacks can change either. The separate
  Role<2 branch dispatches only Physics2, including for literal ELevelTick1.

No original EXE/DLL is loaded or executed. Reproduction:

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A23CE --stop-address=0x103A23F8 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A2643 --stop-address=0x103A278D 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -s --start-address=0x1042AC40 --stop-address=0x1042AC48 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
```

## Implemented service

`native/quest_actor_tick_tail.h` implements that control flow with live native
providers. It introduces no active/delete filter between callbacks, no forced
Physics, no standalone timer/state transaction and no fabricated inventory.
Every reached provider shares caller work admission; missing active services
refuse. Initial NaN LifeSpan remains untouched, but reached nonfinite arithmetic
and Float overflow explicitly refuse rather than silently inventing results.

`native/portable_actor_tick_tail.inl` binds actual reflected Actor fields,
authenticates the real Tick/Expired declaration and selected Function packets,
uses the same Execution for state/timer callbacks, writes LifeSpan through the
root write journal without reconstructing unrelated animation fields, invokes
the original DestroyActor provider, and dispatches the existing real falling,
rotation and PendingTouch implementation. No direct Health subtraction is added.

`AdvancePortableActorNonPawnTickTail` is a **committing phase API**, comparable
to the existing explicit state/timer APIs. It rejects Pawn substitution and
unproven nearest-native virtual implementations. The audited native classes
for the first-Training crate, Lockpick and WoodFragment are respectively
DeusEx.DeusExDecoration, Engine.Inventory and Engine.Projectile. The existing
physics-cohort diagnostic reuses this native virtual-identity check.

## Verification scope

The focused helper test enumerates all unsigned RemoteRole/Role byte pairs,
Physics0/2/6 and tick types0/1, and separately checks exact provider ordering,
callback mutations, suppressed Tick, zero/negative/NaN LifeSpan, active missing
providers, work cutoffs, overflow and the PC64-versus-stored-Float expiry edge.

The owned crate integration test uses explicitly saved begun/Physics0/LifeSpan
preconditions and original GotoState(None) initialization of the existing raw
Actor frame, not actual completed startup. It establishes genuine native
Engine/metadata/config/Package/lookup/collision services before native expiry.
It checks committed countdown,
native Expired/destruction and original Containers.Destroyed, authoritative
save/reload, early write refusal and late callback refusal with whole-root undo.
Its existing Game=None fixture does not prove surviving collectible loot.

The helper passes 3,714,760 checks / 26 deliberate refusals. The owned crate
test passes 415 checks / 5 deliberate refusals, with all 18 owned source files
unchanged. The normal countdown executes 28 script instructions; actual native
expiry executes 247. A fresh re-created CPU fixture refuses at instruction246
and restores its complete checkpoint, world revision, deferred ownership and
GC counts after the genuine contents/deletion prefix. Successful native
deletion retains its real deferred list; checkpoint capture explicitly refuses
until the deferred cleanup/save protocol exists. This guard is not weakened.

Strict ARM64 syntax checks of both new/extended test translation units pass
with `-Wall -Wextra -Werror`. The actual Android `deusex_quest` native build
also links successfully; no APK is generated by this native-only command.
Normal Training startup and the aimed two-crowbar Fire diagnostic each repeat
twice with their previous exact instruction/write counts and whole-root undo;
all 41 owned sources are unchanged. Both still stop at the named bound-player
ConsoleCommand/FExec boundary, not successful or playable startup. The Fire
diagnostic retains two complete cycles, four actual traces, crate HP10->4->-2,
nine births and a surviving Lockpick before that later refusal.

The complete desktop rebuild is terminal and passes. The final, post-build
shared CTest gate executes 199 tests out of 238 registered, with 39 explicit
owned-data skips and zero failures (48.40s). The actual owned Engine collision/
config/default/lookup/lifetime regression separately passes 15,156 checks /
31 refusals; the owned crate phase test above is also run separately, not
counted as an executed owned-data CTest. All builds and test jobs are terminal.

## Still required for live Training

The Quest frame loop does not yet invoke a complete original actor scheduler.
The new phase must follow real stasis/owner/parity and main/blend animation work,
inside the genuine Level time/pause/live-slot/pending-batch ownership. It must
not be called as a substitute for those phases or used to publish a failed
startup. Controller weapon Fire, actual owned inventory/loot collection,
newborn/deletion renderer publication and automatic world scheduling remain
separate unfinished integration work. No new APK or headset acceptance is
implied by the CPU phase tests.
