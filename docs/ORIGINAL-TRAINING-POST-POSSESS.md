# First Training: real SpawnPlayActor continuation (2026-10-29)

Source contract and current partial implementation, not permission to publish
the rollback diagnostic player or claim completed startup. Original native
evidence uses static disassembly/package bytecode inspection only; never
load/execute the commercial DLLs/EXEs.

The integration seam is portable_player_login.inl after SetSessionPlayer and
observation.paired=true, now calling portable_startup_player_tail.inl. The same
host/Execution/root and captured Login-returned pawn are preserved. Never
return current viewport.Actor in place of that original pawn.

## Verified reached prefix

Subsequent bounded-name continuation: native1024 now returns in both actual
first-Training runs. The next boundary is the genuine missing constructor-owned
PlayerPawnExt.FlagBase service, RestoreSkillPoints PC55/opcode1,265660 instructions/
37824 writes. Repeated full rollback/42 unchanged sources pass, tracked in ORIGINAL-STRING-TO-NAME.md;
see ORIGINAL-PLAYER-FLAGBASE.md for source/default-allocation/GC integration evidence.
The native1024 and native544 observations below are retained historical checkpoints.

The latest native544 batch now returns from original ResetKeyboard and advances
through TravelPostAccept into DeusExPlayer.RestoreSkillPoints. Two normal
first-Training expected-boundary runs reach PC23/opcode100/native1024 at265657
instructions/37823 writes, with complete rollback and42 unchanged owned sources.
The next service is actual Extension.ExtensionObject.StringToName and subsequent
FlagBase restoration, not another ResetKeyboard bypass. Full desktop/strict
ARM64/Android-native builds and CTest241 executed/290 registered/49 explicit
owned skips/zero failures199.98s pass. See ORIGINAL-INPUT-RESET-CONFIG.md.
Startup and live controller/crate/loot/gravity acceptance remain incomplete.

The following native544 observations are earlier checkpoints, before that batch.

The final compatible-snapshot first-Training two-Fire diagnostic repeats2 at
272616 instructions/37200 writes. Both runs apply roles/display/admin, execute
TravelPreAccept, select the actual loader-owned empty travel map, return from
original GameInfo.AcceptInventory, and enter the original TravelPostAccept
ancestry. Both stop at DeusExPlayer.TravelPostAccept PC6/native544 ResetKeyboard.
The exact-boundary diagnostic exits0 with full save/state/revision/hash/RNG/
client/lookup/Actor/GC undo and41 unchanged owned sources. This is NOT successful
startup or live controller/crate/loot/gravity acceptance. PostLogin/tail return
remain wired but unreached, not verified by that observation.

After adding the incoming-replacement checkpoint guard, the final guarded
normal first-Training startup separately reaches the same native544 PC6 at
264064 instructions/36592 writes with full undo and41 unchanged sources. Full
desktop/strict ARM64/Android-native builds pass; CTest238 executed/286 registered/
48 explicit owned skips/zero failures140.19s. Generated Spawn13803/231 includes
eight incoming-checkpoint refusal controls. This final normal run is not the
earlier two-Fire diagnostic and does not claim live attacks or startup success.

Pure provider tests cover prefix order/admin short-circuit, actual ParseParam,
loadonly/NAME selection, duplicate-last lookup, present-empty values, fresh
slot-zero fallback, provider faults and shared work cuts. Runtime dispatch uses
the existing original native Actor checked lookup/ProcessEvent adapter for
both pawn and Game callbacks, with authenticated Pawn/PlayerPawn input packets.

## Original normal standalone order

1. Engine10397193..103971A7 writes actual Role=4, RemoteRole=caller role2,
   ShowFlags=480Chex and RendMap=5, in that order. Role/RemoteRole are actual
   scalar ByteProperty flags20/21; ShowFlags/RendMap are scalar Int flags0.
   Native RendMap+600 follows ShowFlags+5FC.
2. 103971AD..103971D8 calls appCmdLine then ParseParam(alladmin). Set actual
   bAdmin if true, or otherwise if actual native Level+3C NetDriver is null,
   preserving other packed bits. Do not skip the earlier command-line query
   merely because this is standalone. Native bAdmin is PlayerPawn+69C bit1.
3. 103971DE..103971F4 checked dynamic TravelPreAccept on the captured pawn,
   no params/result. Actual Actor event flags800/two logical bytes remains a
   real dispatch even when this player ancestry adds no override.
4. 103971F7..10397A71 selects/imports actual travel data. loadonly skips the
   import branch, NOT the later callbacks. Otherwise query NAME= with genuine
   FURL.DefaultName fallback, not GameInfo.DefaultPlayerName. FURL.DefaultName
   is1058F3C0/ordinal865; StaticInit reads URL.Name at103F6D78..103F6DF5.
   Search native Level+E4 travel map; fallback considers fresh slot-zero
   LevelInfo.NetMode0 and first entry. Nonempty imports need actor creation,
   travel-flagged field import, reference fixups and reverse traveller callbacks;
   refuse that branch explicitly until implemented.
5. Even with no travellers,10397AB6..10397B48 revalidates fresh slot-zero Game
   and checked AcceptInventory(pawn), original declaration flags802/Pawn object
   parameter80/no result. This invokes original AddDefaultInventory.
6. 10397B8B..10397BA9 checked pawn TravelPostAccept, no params/result.
7. 10397BAC..10397C34 revalidates fresh slot-zero Game AGAIN, then checked
   PostLogin(pawn), original flags802/PlayerPawn parameter80/no result. Do not
   cache Game from Login or AcceptInventory.
8. Cleanup temporary travel arrays. 10397D99..10397DA9 returns the original
   captured Login pawn, ret10hex, without rewriting caller OUT Error.

## Empty travel is actual loader authority, not an omitted service

Native v68 serialization reads Level's travel map after FirstDeleted and the
following16 references at1039D5ED..1039D602. The map count is FCompactIndex
(103A16D2..103A16E3). Independent first-Training byte inspection finds count0
at file offset4370527. ReadPortableLevel68DeferredState now decodes that actual
compact count and ordered FString pairs, preserving empty/duplicate keys,
ANSI zero-extension, UTF16 units, embedded NULs and exact source offsets. Its
working-byte admission includes body, vector capacity and string heap capacity.

The native Level tick context shares immutable loader-owned travel state.
LoadPortableRuntimeMap's optional PortableMapTravelSource independently binds
incoming absence/replacement, preserving1038D094..1038D0BF. A missing source
does not authorize the empty branch. The Training probe explicitly supplies
no incoming replacement. Tick changes/undo and same-map checkpoint restoration
preserve this immutable ownership; map retirement releases it. The native
state ledger counts allocations before publishing slot0/Level.Model authority.
No other campaign map is required for this first-Training path.

Portable checkpoints do not yet archive incoming replacement data. Capture,
validation and restoration therefore explicitly refuse whenever the actual
loader profile contains a replacement (including a present-empty replacement).
Load refuses before the boundary garbage collector. This prevents scripts from
replacement A being restored over retained replacement B. Authored data with
either no source or explicit no-incoming source keeps ordinary checkpoint
behavior; immutable same-map travel ownership survives tick/checkpoint resets.

FURL.HasOption uses full case-insensitive CString equality, not prefix matching
(loadonly=1 is different). GetOption selects the first NAME= prefix; genuine
FURL.DefaultName is queried even when NAME is present. Loader rehash makes the
last duplicate serialized key win; standalone lookup miss selects the first
pair only after three fresh slot-zero reads and NetMode. Selected nonempty
import still explicitly refuses until actor import/reference fixups/reverse
traveller callbacks are implemented. Never inject loadonly to avoid that work.

The explicit probe engine-session command line is empty; probe CLI control
arguments are not virtual engine appCmdLine. FURL.StaticInit's original
Editor-profile GetStr uses1024 UTF16 units, so the probe binds URL.Name through
OrderedIni.PropertyScalar (missing empty, newest duplicate, max1023 units), not
an unrestricted raw Scalar or GameInfo.DefaultPlayerName.

## Reached script dependencies remain real

JCDentonMale.TravelPostAccept121 bytes calls compiled super DeusExPlayer545,
which calls PlayerPawn24. PlayerPawn restores default Health only if Health<=0.
DeusExPlayer reaches native544 ResetKeyboard at logicalPC6, followed by theme,
charged pickups, skills/augmentations, conversation termination, skin/carried
decoration/FOV/scope restoration, conditional MissionScript creation and
BaseEyeHeight. JC finishes PlayerSkin texture selection. Empty travel data is
not permission to omit these calls.

ResetKeyboard is NOT UInput.ResetInput. Engine103021B2 ->103B96D0..103B9756
checks real Player->Viewport->Input and calls UObject.ResetConfig(Input.Class)
through IAT1059A098. Its configured-class/default/cache effects must have a
typed real service, not empty keyboard state. See ORIGINAL-INPUT-CONFIG-INSTANCE.md.

Native544 is now implemented and verified; its source contract remains: original
gates are Player None, non-Viewport, then Input None; otherwise reset the actual
Input.Class. It does not require Input.Viewport/Init. A separately pinned
read-only DefUser cache is needed, not a writable User-backend alias. ResetConfig
overlays only the selected class section, reloads selected/descendant CDOs first
then live instances, and does not Flush. Actual DefUser lacks Extension.InputExt,
so this profile has zero overlay calls but still requires real reloads preserving
the five prior SET bindings. Cache dirtiness must stay distinct from explicit
pending Flush snapshots; do not accidentally publish later cache-only changes.
These effects now pass owned integration and the actual startup return; see
ORIGINAL-INPUT-RESET-CONFIG.md. Native1024 is also implemented in the subsequent
bounded-name batch. The constructor-owned FlagBase and initial empty CheckFlag
path are now implemented from actual CDO/constructor authority, not an optional
None property. The real Training continuation reaches ClearAugmentationDisplay
PC15/native1455 GetTopChild; see ORIGINAL-FLAGBASE-CONSTRUCTOR-IMPLEMENTATION.md.
Positive flag nodes and native archive/travel remain separate required work.

After SpawnPlayActor returns, LoadMap still requires moving-brush tracking,
subsystem rebinding, per-viewport ResetInput/render services and DetailChange.
Finishing this tail alone does not prove completed startup or live gameplay.
