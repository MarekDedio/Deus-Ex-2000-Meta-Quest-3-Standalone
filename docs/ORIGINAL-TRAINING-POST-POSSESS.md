# First Training: real SpawnPlayActor continuation (2026-10-27)

Read-only next-step evidence, not implemented startup or permission to publish
the rollback diagnostic player. Original Engine.dll/Engine.u/DeusEx.u/Training
hashes match the recorded owned originals. Static disassembly/package bytecode
inspection only; never load/execute the commercial DLLs/EXEs.

The integration seam is portable_player_login.inl after SetSessionPlayer and
observation.paired=true. Preserve the same host/Execution/root and captured
Login-returned pawn. Do not simply remove the post-Possess refusal or return
current viewport.Actor in place of that original pawn.

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
at file offset4370527. Existing ReadPortableLevel68DeferredState stops before
it and therefore does not establish a runtime empty-map capability yet.

Extend bounded loader parsing/ownership for that actual data before selecting
the empty branch. Preserve LoadMap's optional incoming travel-map replacement
at1038D094..1038D0BF; incoming absence must be an explicit embedding input,
not inferred from the authored zero count. No other campaign map is required
for this first-Training path.

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

After SpawnPlayActor returns, LoadMap still requires moving-brush tracking,
subsystem rebinding, per-viewport ResetInput/render services and DetailChange.
Finishing this tail alone does not prove completed startup or live gameplay.
