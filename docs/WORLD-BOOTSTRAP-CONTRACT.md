# Original GOTY world bootstrap contract

Read-only original DLL/script audit, 2026-10-11. This is an implementation
boundary and regression specification, not proof of a started campaign.

## Authority and limits

Primary evidence is the user's unmodified
`D:/Steam/steamapps/common/Deus Ex/System/Engine.dll`, SHA256
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`,
and the installation's compiled Engine/DeusEx scripts. The original executable
was not run. Android NDK27 LLVM tools read exports, imports, vtable bytes and
bounded disassembly. Addresses below are static image VAs, Engine base
`0x10300000`, not runtime pointers or portable object layouts.

The locally pinned SurrealEngine `Engine::LoadMap` was inspected as a guide.
Its captured `loadActorCount`, PostPostBeginPlay-before-InitBase ordering,
manual GameInfo construction and Botpack fallback must **not** be copied into
an original GOTY bootstrap: they differ from the original body.

Ignored evidence is under `artifacts/world-startup-contract-20261011/`:
`original-loadmap.asm`, `original-spawnplayactor.asm` and
`original-first-startup-scripts.log`. The script log contains complete
Engine.GameInfo.InitGame and Engine.Pawn.PreBeginPlay inspections, followed by
an explicit missing direct DeusExGameInfo.PreBeginPlay export; that final
inspection failure is not a startup success. Related frame and Spawn evidence
is in `artifacts/spawn-frame-audit-20261011/verification.md`.

## Entry points and native identity

- UGameEngine::LoadMap exported thunk `0x103033a5` jumps to `0x1038c1f0`.
- ULevel's UObject vtable is `0x10428afc`. Its slots identify IsServer `+0x84`
  (`0x10303cd3`), DestroyActor `+0x94` (`0x10302568`), SpawnActor `+0x9c`
  (`0x103016db`), SetActorZone `+0xac` (`0x10302a77`) and UpdateTime `+0xe0`
  (`0x10301b18`). The mapping is from exact exported identities, not objdump's
  nearest unrelated symbol label.
- Actor virtual InitExecution is `+0x38`, independently tied to exported
  AActor::InitExecution `0x10302194` / body `0x103deb40`. The world pass invokes
  it on every nonnull actor slot, not only serialized HasStack actors.
- Core FindFunctionChecked import is IAT `0x10599a88`. Script startup calls
  resolve by original FName then invoke actor virtual ProcessEvent `+0x10`.
- AActor::ProcessEvent body `0x10369100` suppresses script events while the
  bound LevelInfo's `+0x4f0` bit `0x2` (bBegunPlay) is clear. This is not a
  reason to suppress native InitExecution or native Spawned.

## GameInfo creation precedes the world execution-frame pass

LoadMap performs this selection at `0x1038d4b5`--`0x1038d5c8` only when
ULevel::IsServer is true and LevelInfo.Game (`+0x544`) is null:

1. A nonempty URL `GAME=` option selects that class.
2. Otherwise a nonnull LevelInfo.DefaultGameType (`+0x540`) selects the map's
   class directly.
3. Otherwise StaticLoadClass loads `ini:Engine.Engine.DefaultGame` when
   UGameEngine.Client (`+0x44`) exists, or
   `ini:Engine.Engine.DefaultServerGame` without a client. The exact strings
   are at `0x1047ee14` and `0x1047edbc` respectively. LoadMap does not add the
   guide's Botpack.DeathMatchPlus fallback here.

The class is constrained by the original AGameInfo native class identity
`0x1057ba08`. LoadMap calls virtual ULevel::SpawnActor at `0x1038d59d` and
publishes its return into LevelInfo.Game at `0x1038d5a8`; it asserts nonnull.
It does not manually append an uninitialized GameInfo. A before-begun GameInfo
birth already receives its own native execution initialization, while its
script callbacks are suppressed by Actor::ProcessEvent. The later world
InitExecution pass initializes its frame again along with the other live
actors. Do not move InitGame ahead of that pass.

The standalone portable bootstrap needs an actual local-player/session
concept to choose DefaultGame, not accidental absence of a UViewport object
causing server-only class selection. Neither class precedence nor a styled
inventory screen proves player ownership.

The2026-10-12 [native-client bridge](NATIVE-CLIENT-BINDING.md) now provides that
engine-session presence independently of possession. The non-publishing probe
accepts both configured defaults and selects by actual client presence (even an
empty client viewport registry), after URL/map precedence. A missing required
configuration branch refuses rather than borrowing the other default. Saves do
not restore this session binding, and player login/possession remains unfinished.

## Fresh-world phases use live slots, not a captured loaded-actor count

Level.Actors is at `+0x2c`, its live Num at `+0x30`. Each outer loop rereads
Num and the slot array, captures the current nonnull receiver for its call,
then advances the integer slot index. Null slots are skipped. It does not
snapshot an actor vector or stop at the number of loaded actors. Nested births
append slots through SpawnActor and can receive their own complete Spawn
callbacks **and** later callbacks from the currently running outer world pass.
Do not deduplicate these as accidental double calls.

The fresh branch is entered only when bBegunPlay is clear, tested at
`0x1038d6f6`. The observed order is:

| Phase | Original body evidence |
| --- | --- |
| Reset world time and LevelInfo.TimeSeconds | `0x1038d74d`--`0x1038d7f1` |
| Clear the four actor touching entries | live loop `0x1038d7f7`--`0x1038d824` |
| Client-only actor-removal/Role swap handling | `0x1038d824`--`0x1038d869`; standalone IsServer bypasses this branch |
| All nonnull actors: virtual InitExecution | live loop `0x1038d86b`, call `0x1038d87f` |
| Publish begun-play and startup bits | LevelInfo `+0x4f0` OR `0x42` at `0x1038d894`--`0x1038d899` |
| GameInfo.InitGame(Options, out Error), if Game exists | FName `0x1058362c`; ProcessEvent `0x1038d95f` |
| All nonnull actors: PreBeginPlay | live loop `0x1038d99c`; ProcessEvent `0x1038d9c8` |
| All nonnull actors: BeginPlay | live loop `0x1038d9d0`; ProcessEvent `0x1038d9fc` |
| All nonnull actors: SetActorZone(actor,1,1) | live loop `0x1038da04`; virtual call `0x1038da1d` |
| All nonnull actors: LastRenderTime = -10.0 | live loop `0x1038da2d`, actor `+0x34` write `0x1038da3f` |
| All nonnull actors: PostBeginPlay | live loop `0x1038da47`; ProcessEvent `0x1038da73` |
| All nonnull actors: SetInitialState | live loop `0x1038da7b`; ProcessEvent `0x1038daa7` |
| Native world-base initialization | live outer loop `0x1038daaf`--`0x1038dbe0`, details below |
| Clear startup bit, keep begun-play | LevelInfo `+0x4f0` AND `~0x40` at `0x1038dbe3` |
| All nonnull actors: PostPostBeginPlay | live loop `0x1038dc6e`; ProcessEvent `0x1038dc99` |

The `0x40` bit is the startup flag in the level flag group. In particular,
PostPostBeginPlay is after native bases and startup-clear, not before them.
The wrapper builds writable string storage for InitGame's Error argument and
copies the output back at `0x1038d969`; the audited immediate path then proceeds
to PreBeginPlay without the guide's `if (!error.empty()) throw` check. Do not
claim that guide policy as an original LoadMap branch. Portable unsupported
native/VM failures still must fail the outer transaction with exact evidence.

For an already-begun loaded world, the branch jumps to `0x1038dbec`: it restores
world time from serialized LevelInfo.TimeSeconds, skips fresh InitExecution,
InitGame, Pre/Begin/PostBegin/SetInitialState and base passes, then converges
on **PostPostBeginPlay** at `0x1038dc65`. Therefore neither rerunning fresh
login nor omitting every loaded-world callback is justified by this audit.
Full save/travel/player restoration still needs its own contract.

After PostPostBeginPlay, `0x1038dcd5`--`0x1038df46` rebuilds Level.Actors:
preserve slots0/1, then nonnull bStatic actors from old slots2 onward in order,
then nonnull nonstatic actors in order; it records the first dynamic index at
Level `+0x110`. A faithful final bootstrap must account for this native
reordering/compaction and its consequences for persistent actor indices,
birth manifests, collision and subsequent Tick. It must not call that initial
order preservation after this phase.

## World AttachTag handling is not SpawnActor handling

The world-base pass checks each live actor's AttachTag (`+0xd4`). When nonnull,
it scans live Level slots from zero, comparing candidate.Tag (`+0xb0`) with
the outer actor's AttachTag. The **first** matching candidate is passed to
`outerActor.SetBase(candidate,false)` at `0x1038dafd`; the next action advances
the outer actor. It does not base every matching candidate on the outer actor.
If there is no match, the nonnull-AttachTag branch does not fall through to
FindBase. These are native FName identities, not arbitrarily case-sensitive
display strings.

Without an AttachTag, a null Base, bCollideWorld and a Decoration, Inventory or
Pawn class ancestry allow FindBase when Physics is None or Rotating. The exact
native class objects are `0x10578d48`, `0x10576f70`, `0x1058c660`. The FindBase
call is `0x1038dbb8`. If the resulting Base is the actual LevelInfo, LoadMap
clears it with SetBase(None,false) at `0x1038dbd5`.

Original SpawnActor's separate successful interval `0x10395612`--`0x103956a1`
has only the Base/bCollideWorld/class/Physics FindBase conditions, with its
FindBase call at `0x1039569c`. It has **no AttachTag scan**. The portable
SpawnBase helper now omits the guide-derived scan that used the reversed
receiver/target direction. Generated birth regression establishes matching
tags in actual BeginPlay and checks that neither the fresh actor nor its owner
is attached by that world-only rule, including cold save/restore.

The existing portable overlap-based FindBase is not proof of original BSP,
slope, standing-count or moving-base physics fidelity. Do not mark the native
base phase complete merely because the AttachTag correction passes.

## Next implementation and proof boundary

Implement a single staged **world** transaction over the real unmodified
Training map. Reuse actual state/frame, birth, owner/base, class-default,
collision, RNG and AI-manager journals; do not call public per-actor APIs that
independently commit phases. Validate the actual LevelInfo slot identity and
the chosen GameInfo class, invoke the real Spawn path, then advance the exact
fresh-world phase order above. Reacquire slot identity after callbacks;
preserve nested births and deletion holes until the audited compaction phase.
Bound total work independently of live array growth. Unsupported behavior is
a failure with phase, slot, receiver, actual function path, bytecode PC,
native/opcode and reason. Roll back all authored writes, frames/HasStack,
transient transition flags, births/names/slots, native attachments/topology,
collision registrations and RNG, without publishing a begun world.

First real-script dependencies, not substitutions:

- Engine.GameInfo.InitGame PC0 calls Log231; its argument first evaluates
  At_StrStr168 at PC1. Options use the original ParseOption/GetIntOption
  script functions and string natives. The base Mutator birth is at PC245
  (Spawn278); logging that object's identity needs ObjectToString opcode0x56
  at PC273. A missing primitive must not become a successful logging/no-op
  workaround that bypasses argument evaluation or original Mutator creation.
- Engine.Pawn.PreBeginPlay PC0 calls AddPawn529, then the real Actor super at
  PC3, tests bDeleteMe and calculates MinAngularSize through Tan189 at PC26.
  Skill uses FClamp246 at PC156. AddPawn requires the corresponding native
  RemovePawn530 and actual Pawn.Destroyed cleanup, not an unrelated map-scan
  list. Native list ownership, duplicate/removal semantics and rollback must
  be audited and tested.
- Loaded actors need native InitExecution in their own right; state-native
  calls must never fabricate frames for arbitrary frameless objects. Original
  InitExecution's latent-field initialization remains unproven; the portable
  Continue policy must remain explicit rather than asserted original memory.
- SetActorZone(1,1), native subclass Spawned/InitExecution overrides, world-base
  FindBase, initial Role/detail/editor filtering, native slot compaction,
  network/authority and subsequent Tick/animation/state/timer/physics ordering
  are distinct required contracts. They are not all established by this
  bounded LoadMap audit.
- A real player requires original Login, native SetPlayer ownership and
  most-derived possession, not merely spawning JCDenton. Exported
  SpawnPlayActor `0x1030209a` jumps to `0x10396bd0`; its observed Login
  ProcessEvent is `0x103970ed`, followed on success by SetPlayer
  `0x1039718e`. The rest of this player/travel sequence is not fully audited
  here. Extension.PlayerPawnExt.Possess requires InitRootWindow1052 at PC6;
  ConBindEvents2102 and actual player-root UI integration remain dependencies.

Generated phase/reentrancy tests must prove live-count growth, current/future
deletion, callbacks that change tags and states, proper startup-bit timing,
first-match world attachment direction, before-begun GameInfo birth and
frame reinitialization, restored-world exclusions, error evidence and total
rollback. They are necessary, not sufficient: the unmodified Training world
must complete all required phases and real login, with actual GameInfo and
JCDenton identities, native player binding/GetPlayerPawn, original inventory
and UI, then verified on-device behavior. A diagnostic that reaches the first
honest missing native is progress toward that boundary, not world completion.
