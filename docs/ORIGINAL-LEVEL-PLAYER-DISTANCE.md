# Original Level player-distance prepass

First-Training integration,2026-10-25. This implements the actual native
Level.Tick distance phase needed by Actor.InStasis. It is not full Level.Tick,
automatic physics, a player/account screen or completed startup. The active
gameplay target remains first Training's crates, usable loot and gravity.

## Original static authority

Read-only owned Engine.dll103A55C1..103A5696 precedes the actor update loop.
Engine SHA256 remains
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Core.appSqrt import10599CA4 names `?appSqrt@@YANN@Z`, whose actual Core
export/thunk101032BA enters10123290..96: load Double argument, x87 FSQRT,
return extended result. No commercial DLL/EXE is executed or loaded.

```powershell
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x103A55C1 --stop-address=0x103A5698 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
& 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe' -d -Mintel --start-address=0x10123290 --stop-address=0x10123297 'D:\Steam\steamapps\common\Deus Ex\System\Core.dll'
```

The exact lazy sequence is:

1. Actual native Level.Actors[0].NetMode byte nonzero skips the whole phase.
2. Follow Level.Engine, actual Engine.Client, Client.Viewports[0], then that
   viewport's Actor. A real null Actor skips; do not invent a PlayerPawn from
   controller/HMD tracking or another viewport. Missing active dereference
   services explicitly refuse the unsafe original access.
3. Actual PlayerPawn ancestry and its actual nonnull native Player pointer are
   required; false/null skips without reading FirstDynamic/Location/slots.
4. Load native FirstDynamic at103A5620 **before** copying the Pawn.Location
   X/Y/Z Float bits at5626..5634. Capture these coordinates once for the pass.
5. Compare the index against live Actors.Num on every iteration. Read its live
   slot; null skips. There is no new bDeleteMe/active/physics/cohort filter.
6. Load that source Actor's Location in Z/Y/X order, subtract the captured
   player components in PC64, then compute `(Y*Y + Z*Z) + X*X` with each native
   arithmetic operation retaining PC64 precision. Spill the sum to a Double
   argument for original Core.appSqrt. Its return stays extended: the FST
   Double at5689 does **not** pop/reload the root before the Float store5692.
7. Reread the live Actors data/index at568C..568F to select the **destination**
   Actor after appSqrt, rather than assuming it is still the source pointer.
   Store DistanceFromPlayer Float, increment the native index and repeat.

This corrects the earlier prerequisite description's FirstDynamic/capture
ordering and its omitted destination-slot reload. It also explains why the
original lower-crate editor distance6376.48926 must not be treated as a fresh
current-player measurement when this phase has not yet run.

## Runtime ownership and arithmetic

`quest_level_player_distance.h` retains the exact lazy protocol and uses the
existing host-independent PC64 dyadic arithmetic/FSQRT implementation. Zero
distance produces Float+0. Active nonfinite coordinates, unrepresented
overflow and unsafe indices/array sizes explicitly refuse; they do not
publish saturated or arbitrary distances. Captured unused Float bits are not
eagerly rejected if the actual native loop never performs arithmetic.

`portable_level_player_distance.inl` authenticates actual current native
Level/Engine/Client/viewport/Player edges, actual NetMode enum, Player pointer,
native FirstDynamic and live slots. Actual declaring Location and scalar
DistanceFromPlayer fields are retained. Native stores use the existing
Execution journal and saved-property path, with no unrelated animation-clock
refresh. No native process pointer is serialized by the new phase.

The actual native Engine.Client edge now joins the existing session UObject
when either Engine or Client is prepared first. Publication occurs only after
the existing staged allocations/index checks, and teardown clears the edge
before releasing Client/session roots. Engine's existing GC traversal retains
the actual Client. Revocation is checked as revoked, not converted into a
legitimate empty/unpossessed viewport. Client/viewport/session pointers remain
transient and are never restored from a map checkpoint.

`AdvancePortableLevelPlayerDistance` is the ordinary committing phase API.
It is not called automatically by Quest world scheduling yet. Its null native
viewport Actor path can succeed without a native partition or field stores.

## Bounded first-Training diagnostic

`portable_training_startup_probe --observe-player-distance` invokes this real
service after genuine SetPlayer has paired the actual Login-created JC and
viewport, but **before** original Possess. That diagnostic insertion is not
the original frame schedule. It does not publish partial startup or inject a
Player, pose, distance or fake begun state. At most16 requested sample Actors
are detached; the CLI requests the two genuine Training crates and reports
their before/after values with captured JC.Location and native loop counts.
The enclosing startup still rolls back at its actual later unsupported route.

The CLI also checks actual Engine.Client identity, the genuinely unpossessed
no-store path, revocation refusal and cleared Engine.Client after teardown.

## Verification and remaining work

Standalone strict GCC helper passes2678 checks/50 explicit refusals: all
nonzero NetMode bytes, null Actor/Pawn/Player lazy branches, exact capture and
live-array order, changed destination after source read, appended/shrunk slots,
integer-distance oracles, subnormals, overflow/nonfinite refusal and shared
work/store prefixes.

Final completed verification:

- Full desktop build, strict ARM64 syntax for both new helper/probe translation
  units and actual Android native link pass. Every build process is terminal.
- Shared CTest241 registered/202 executed/39 explicit owned-data skips/zero
  failures in69.56 seconds. Owned data tests below run separately.
- Genuine first-Training distance diagnostic repeat2 retains220285 instructions
  and25676 writes, exactly237 more than the unchanged normal startup25439.
  FirstDynamic192,238 visited slots,one null,237 writes,1670 provider steps;
  captured actual JC.Location=(-1149.24353,825.843933,-65.1034927). Original
  crate distances6376.48926/6371.33154 become2434.82324/2435.43677 on both
  repeats. Client identity, genuine unpossessed skip, revocation refusal and
  cleared Engine.Client teardown pass. Whole save/state/revision/hash/RNG/
  client/lookup/Actor/GC rollback and all41 fixture sources are unchanged.
- The separate aimed two-Fire regression repeat2 retains228837 instructions/
  26047 writes, original lower-crate HP10->4->-2/removal, six WoodFragments
  and surviving Lockpick birth. The known bound-player ConsoleCommand FExec
  refusal still ends both startup modes; no playable startup is published.
- Actual dormant Training animation588/24 refusals, actual crate415/5,
  dormant Engine collision/config15156/31 and focused owned timestamp/
  stasis/save fixture pass separately. The crate415/5 fixture does not prove
  surviving contents; those are observed only in the initialized-world Fire
  diagnostic above, still under full rollback.
- All45 checked original script/config/map files retain their exact size,
  modification time and SHA256 after these checks. No other gameplay map is
  loaded by the new diagnostic, and no commercial binary is executed.

This does not provide renderer actor/zone timestamp publication, owner/parity/
pending/newborn scheduling, full Actor/Level Tick, deferred cleanup, live
controller weapon/loot input or render/save/audio publication. Complete
startup still has the known bound-player ConsoleCommand FExec gap. No live
crate destruction/gravity, finished tutorial, new APK or headset acceptance
is established by the phase helper or rollback-only diagnostic. Other-map
gameplay integrations remain deferred.
