# First Training: original player Login prefix

Later noneditor-loader evidence,2026-10-22: the former Camera7 reuse below is
fixed by source-context admission, not a name blacklist. Actual first Training
now creates original JCDentonMale and reaches Login PC133/native3015
CreateDumpLocationObject in both full-rollback attempts (201628 instructions,
14642 writes,117 detached Spawn observations). Login still does not return
or possess a published player. See ORIGINAL-TRAINING-DUMP-LOCATION.md and
TRAINING-LOAD-CONTEXT-SAVE-PLAN.md; earlier all-export counts below are historical.

2026-10-22. Only `Maps/00_Training.dx` is the actual-map integration target.
This prerequisite does not finish crates, usable loot, gravity, world Tick or
the entire first Training map, and does not produce a replacement APK.

## Original authority and implemented boundary

`quest_spawn_play_actor_login.h` follows the reached original
Engine.dll `10396BD0..103970ED` prefix. See
`ORIGINAL-POST-PARTITION-CLIENT-SETUP.md` for static-only binary provenance.
It borrows the caller's root, work allowance and admitted scratch; it does not
start its own transaction or publish a player. It preserves:

- Cleared caller-owned OUT Error, then the actual UPlayer type predicate.
  The guarded native class is **UNetConnection**, not UViewport. The actual
  prepared standalone Viewport derives from Player, so sandbox remains None.
- First case-insensitive `CLASS=` option, including present-empty, then ordered
  `usr:DefaultPlayer.Class` and `ini:URL.Class` fallback. The selected result
  must be an authentic loaded PlayerPawn-derived UClass.
- Original ordered `?` option assembly in the native 1024-WCHAR buffer. Portable
  overflow policy refuses before copying; it never truncates options. CString
  inputs stop at their first UTF16 NUL. Unestablished non-ASCII CRT prefix
  equivalences refuse instead of guessing a locale.
- Original repeated slot-zero LevelInfo checks and current LevelInfo.Game
  reads, then dynamic original Game.Login(Portal, Options, OUT Error, Class).

`portable_player_login.inl` authenticates the actual most-derived Login
declaration, all five declared parameter/result fields and Class constraints.
The owned `DeusEx.u` Login.SpawnClass metadata has Core.Class reference and
Engine.PlayerPawn secondary constraint, not an unconstrained Object.
Dispatch uses the EXISTING VM execution/root. A returned genuine PlayerPawn
would enter the existing same-root SetPlayer/Possess path; the unimplemented
post-Possess role/travel continuation still explicitly refuses.

This is not a general StaticLoadClass implementation: a requested unmaterialized
class/package refuses, rather than pretending a failed load and falling back.
Network sandbox, full FURL construction/default expansion, successful null-pawn
native logging, remaining travel import, moving-brush tracker creation and
later renderer/audio/detail services are not certified by this prefix.

## Source and transaction ownership

The actual Training observation CLI binds the two already-decoded Portal/Op
fields explicitly to empty plain-map diagnostic values. This does NOT claim
that the original FURL constructor or default Name/Class option expansion has
been implemented. Class aliases come from explicitly read owned
`System/User.ini [DefaultPlayer] Class` and `System/DeusEx.ini [URL] Class`,
using bounded native-profile OrderedIni Scalar reads. Both files participate
in full source byte/size/timestamp checks along with the 38 shared script
packages and first Training map. No original commercial DLL/EXE is executed.

After the original Actor partition, the coordinator rereads the live native
client viewport list in ascending order and checks its lifetime/identity.
Per-viewport observations are bounded by the real registry limit. No head
tracking predicate chooses a different gameplay path.

Owned OUT String cells survive native stack unwinding and the outer VM undo
journal. Their reference closures do not borrow local callback contexts. The
host retains their actual live capacities across nested callbacks, including
InitGame's existing OUT cell. Growth is admitted before copying and uses a
staged old+new allocation peak; the cell never shrinks its capacity, so every
prior journal string fits during admission-free undo. Frame arguments and native options buffer retain
their combined scratch reservations through Login. Detached observation strings
are admitted and measured through the remainder of the root.

## Verification

Provider-mock coordinator tests execute **1871 checks / 174 cases**. They cover
literal ordered provider traces, class fallback/empty/first-match selection,
CString and native-buffer boundaries, actual NetConnection predicate branches,
None returns, original failure branches, all reached provider failure prefixes,
borrowed work exhaustion and exact scratch restoration. They do not execute
original scripts or prove playable startup.

The generated actual-runtime Spawn suite passes **11476 checks / 181 refusal
controls**, including a genuinely prepared native Client/Viewport and source
Login declaration. The isolated Login variants execute real script, mutate
RNG/property state, build a4096-byte local Error using eight512-byte literals
and native concatenation, then exercise owned OUT copyback, warm repeat, exact
last-copyback write-budget failure, a late native999 failure, class fallback,
unknown package refusal and malformed secondary-Class constraint. Original
EX_StringConst has a1023-unit appFromAnsi limit; an earlier fixture incorrectly
assumed one4096-byte literal survived unchanged. That fixture is corrected,
not the original string behavior. Every attempt compares the exact full save,
native order, actor identities, revision, GC, session and generated sources.

Actual first Training repeat2 executes **534158 instructions / 39825 writes**
per attempt and reaches the real `DeusEx.DeusExGameInfo.Login`. It returns
Object None with empty OUT Error, then the later client/player/detail gap
explicitly refuses. The full saved state, revision, native hash, RNG,
client/session, lookup, Actor identities and GC restore; **41 owned sources**
retain bytes/size/timestamps. Final native work is514100 cold /513728 warm of
the unchanged1048576 allowance. All1335 PPP visits,26 Spawn observations and
the exact1364→1334 partition/split1183 remain present.

A temporary diagnostic-only return/local trace, removed from source after
use, identifies WHY this is not a player:

- `Engine.GameInfo.FindPlayerStart` returns actual `00_Training.PlayerStart1`
  at PC118. The authored player start is found; do not replace it with a
  debug camera coordinate or invent another start.
- `Engine.GameInfo.Login` returns **`00_Training.Camera7`** at PC1536. Its
  existing unconnected-player reuse branch selected that actual map Camera.
- `DeusEx.DeusExGameInfo.Login` casts this to DeusExPlayer, gets None and
  returns None at PC544. There is no authenticated JC Denton birth/possession.

Evidence logs are `artifacts/training-player-login-final-20261022.log` and
`training-player-login-trace-20261022.log`; static bytecode listings are
`training-login-script-audit-20261022.log` and
`training-find-player-start-audit-20261022.log`. The next task is to establish
why the camera is eligible, including genuine loader/context authority, not
exclude it by actor name or force a synthetic Pawn. No downstream native267
or3015 failure has actually been reached by this Login attempt.

Independent read-only source-table/ULevel inspection confirms Camera4..7 are
exports3343..3346 at raw serialized slots1331..1334. Each has source flags
`0x02340000`: HasStack`0x02000000`, NotForClient`0x00100000`,
NotForServer`0x00200000`, LoadForEdit`0x00040000`, and NO LoadForClient/Server.
The source MyLevel has1337 slots including29None. These are authored editor-only
camera records; the current loader materializes their references without a
client/server load-context gate. The exact original linker/context predicate
still needs static authentication before implementing filtering; raw flags
alone do not justify deleting camera UObjects or changing serialized source.

The coherent full desktop rebuild passes. Shared CTest passes **165 executed /
195 registered**, with30 explicit owned-data skips, zero failures in47.75s.
Owned first-map isolated crate checks separately pass347/two contents refusals
and still explicitly do NOT certify surviving loot or live melee. The rebuilt
actual first-map movement suite passes122706/21, including real Doctor1
PlayWaiting clock preservation inside nonzero Move and durable-pose commit/
checkpoint restoration. It does not certify live falling/landing. Strict ARM64
runtime, coordinator/probe and final generated Spawn syntax checks pass;
Android native `deusex_quest` links. No APK is packaged/installed from this
backend increment. Logs are `training-player-login-build-final-20261022.log`,
`training-player-login-ctest-20261022.log` and
`training-player-login-crate-20261022.log` under `artifacts/`.
The final movement log is `artifacts/training-player-login-movement-final-20261022.log`.
