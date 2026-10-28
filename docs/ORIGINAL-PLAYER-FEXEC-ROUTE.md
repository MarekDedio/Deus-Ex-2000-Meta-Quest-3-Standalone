# Original UPlayer and GameEngine command-provider route

Verified2026-10-28. The genuine first-Training PlayerPawn.ConsoleCommand root
now passes the former UPlayer refusal and reaches the missing UEngine GSys
service. This is not successful startup, initialized input or live VR crates.

## Same-root runtime path

PlayerPawn.ConsoleCommand now receives its actual shared Execution rather than
entering a no-context native route. No nested public Execute, separate script
transaction, direct Core SET shortcut or partial player publication is added.
The existing return FString and Input/viewport/Console order remain intact.

After Console declines, UPlayer.Exec103D7ED0..103D8098 offers:

1. Actual current viewport.Actor.XLevel.Exec.
2. Actual reflected Actor.Level.Game.ScriptConsoleExec, executor=current Actor.
3. Actual PlayerPawn.myHUD.ScriptConsoleExec, executor=current Actor.
4. Current Actor.ScriptConsoleExec, executor=Self.
5. The captured live Inventory head and its linked Inventory objects, executor
   reread from viewport.Actor before every offer; next is reread AFTER callback.
6. Reread Actor.XLevel.Engine's native GameEngine command chain.

Actor is reread before every stage; optional actual-null relationships skip only
their own branch. Mandatory XLevel/Engine absence refuses. Each child receives
its own command-view value, so its parser cannot advance its caller's pointer.
Repeated identities are not deduplicated. Inventory traversal shares root work
and its4096-offer bound; no snapshot hides callback splices.

Field bindings use actual scalar ObjectProperty declarations, owner membership,
referenced Class and typed storage. Actor.Level is NOT substituted with native
ULevel.Actors[0] for the UPlayer Game offer. Native XLevel and Engine use the
current loader-owned edges, not reflected pointer-shaped patches.

The former unproven +628 provider is myHUD, NOT Pawn.Weapon (+3D0). The original
Engine.u PlayerPawn Children/Next prefix starts at actual Pawn size5D4:
Player5D4,Password5D8,DodgeClickTimer5E4,DodgeClickTime5E8,Bob5EC,LandBob5F0,
AppliedBob5F4,bobtime5F8,ShowFlags5FC,RendMap600,Misc1604,Misc2608,ViewTarget60C,
FlashScale610,FlashFog61C,myHUD628. myHUD is export36,ObjectProperty,dim1,flags0,
referenced Engine.HUD,declared on PlayerPawn export5,next1744. The separate
native quit-menu path10381521/2F corroborates Actor+628 ->HUD menu state.
Owned Engine.u SHA256:
1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA.

## Concrete provider authority and native discriminators

All56 audited original native Actor primary-vtable+48 exports independently
resolve to Core.UObject.ScriptConsoleExec: Engine47,DeusEx8,Extension1.
The runtime authenticates the nearest native ancestor against that closed set;
it does not treat a method name, suffix or arbitrary object as virtual authority.
The shared helper retains actual GIsScriptable,FNAME_Find,current selected-state
lookup,UFunction and FUNC_Exec checks. Matched commands still explicitly refuse
their unimplemented parameter/call service rather than falsely decline.

ULevel.Exec first offers native NetDriver and DemoRecDriver. The existing
loader-owned standalone services profile establishes that neither exists here;
no missing reflected tag or guessed NetMode supplies that authority. The original
DEMOREC/DEMOPLAY discriminators are then checked. Recognized bodies still refuse.

Selected DeusExGameEngine first checks SAVEGAME/DELETEGAME, then forwards the
original command pointer to inherited GameEngine. GameEngine checks OPEN,START,
SERVERTRAVEL,SAY,DISCONNECT,RECONNECT,GETCURRENTTICKRATE,GETMAXTICKRATE,GSPYLITE,
SAVEGAME,CANCEL. Unsupported recognized bodies refuse, including profile-dependent
branches whose complete behavior is not implemented; no successful execution or
complete command handling is claimed. The unknown-command route is implemented.

GameEngine then rereads its current native Level and offers Level.Exec again.
It rereads currentLevel/Actors[0] for Game.ScriptConsoleExec with executor=None,
before UEngine. These are genuine repeated offers, not deduplicated UPlayer calls.

Static evidence is retained in ignored artifacts/player-exec-script-vtable-
proof-20261028.log,provider-set-route-static-20261028.log and provider-static-
expanded-20261028.log. No original DLL/EXE was loaded or executed.

## Corrected UEngine dependency: GSys, not GExec

Fresh actual PE import resolution corrects the earlier audit:

- Engine IAT10599EB4 imports Core.GSys (USystem*). UEngine10380F03..25 offers
  its FExec subobject+28 BEFORE Core.StaticExec10380F44..4F.
- IAT10599EB0 is UObject.StaticExec.
- IAT10599EA8 imports Core.GExec (FExec*), offered only AFTER StaticExec and the
  cache provider at10380F6A..7E. No invented absent-GExec profile is needed to
  reach a genuinely handled Core SET, but actual GSys must be established first.

Current runtime deliberately refuses `Original UEngine GSys command provider is
not prepared: SET InputExt T Talk`. It does not fabricate GSys=None, install a
bare decline wrapper or jump to Core SET. Original System service is not yet
constructed/prepared by this batch.

Next service's verified static contract: appInit10149E2F calls1016D720, which
constructs an actual native USystem with Outer=Transient,NameNone,flags0,stores
GSys at1016D779 and AddToRoot. Registration1016BBE0..BC2C supplies native size6C,
SuperCore.Subsystem,WithinCore.Object,flags4 ->16,ObjectFlags04084004,Core/System.
Its instance constructor preserves copied defaults and installs UObject/FExec
vtables. StaticConstructor1016B500 registers six Config4000/dim1/Options fields:
PurgeCacheDays Int+2C,SavePath Str+30,CachePath Str+3C,CacheExt Str+48,Paths ArrayStr
+54,Suppress ArrayName+60. Actual defaults/config-copy/load ordering and rooted
indexed lifetime must be implemented, not replaced by an empty provider object.

USystem.Exec1016B790 recognizes MEMSTAT,CONFIGHASH,EXIT,QUIT,RELAUNCH,DEBUG
(DEBUG has CRASH/GPF/RECURSE/EATMEM subcommands). Its unknown-command decline
can be used only with real provider authority; this static list alone does not
construct the object or implement command bodies. Evidence: ignored global-exec-
authority-static and global-exec-system-{constructor,lifecycle,profile-proof,
default-copy}-20261028.log. No commercial command was executed.

## Verification

Pure route/discriminator tests pass460 checks/340 refusals, covering every
handled-stop, order/repeated providers, mutable Actor/head/next/executor reads,
copied command views, optional/malformed branches, callback failure, shared work
and traversal cuts. Providers there are explicit mocks, not native execution.

Actual Console/Actor prefix5503/36 covers LevelInfo,Doctor and crowbar method
misses/non-Exec selection, invalid Class-as-Actor refusal, non-interning and
whole save/frame/GC/lookup/revision preservation. Metadata22230/133,config9989/48,
Input classes10446/38,schema34368/1357,viewport Input10315/24,Canvas150/14,HUD11225,
Core SET9548/6,GC155/16 and all eight first-Training regressions pass.

Both two-Fire startup repeats reach the genuine GSys boundary at unchanged
271677 instructions/32553 writes,two notifies/four traces/two crate changes/nine
births,with complete save/state/revision/hash/RNG/client/lookup/Actor/GC undo.
Both doors repeats retain239536/19432,both56-unit moves/two ends,the exact
ReadInput refusal and full undo. These are rollback diagnostics,not live play.

Desktop/strict ARM64/Android-native pass. CTest233 executed/280 registered/
47 explicit owned-data skips/zero failures116.76s. All45 originals/649 frozen
compiler inputs retain length/UTCmtime/SHA256;all jobs terminal. No new APK,
live VR crate/loot/gravity completion,other-map/account UI work,accepted layout
or resource allowance change. Next is actual GSys construction/config/lifetime,
same-root Core SET fallthrough,initialized input and live weapon/world/render
publication. The full port goal remains incomplete.
