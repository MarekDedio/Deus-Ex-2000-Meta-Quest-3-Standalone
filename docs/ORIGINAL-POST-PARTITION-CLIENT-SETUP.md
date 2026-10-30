# Original post-partition client/player setup (2026-10-22)

Continuation2026-10-30: real first-Training SpawnPlayActor now returns from its
actual Login, SetPlayer/Possess, travel callbacks, PostLogin and captured-pawn
tail. Two expected-boundary diagnostics agree at266870 instructions/37932 writes
and verify full rollback/43 unchanged sources. They then hit the explicit
remaining client/player/detail guard; no startup publication is claimed. The
nearest unfinished normal branch is now NON-ENTRY GNewBrushTracker, followed
by subsystem/viewport reset/render/detail handling below, not player creation.
The historical review and static source evidence are retained below.

Static prerequisite research for **only the first `Maps/00_Training.dx`**.
This is not implementation or acceptance evidence for player login, complete
startup, Tick, original HUD ownership, crates, gravity or a new APK. No original
commercial DLL/EXE was loaded or executed. The only executable inspectors used
were the external MinGW `objdump.exe` and the port's existing, read-only
`script_bytecode_inspect.exe`, which parses `.u` tables/bytecode without running
the scripts. No build, runtime initialization, map test or other-map integration
was performed by this review. The documentation file is its only edit.

Addresses below are original PE32 virtual addresses and byte offsets, not
portable object layouts. Previously recorded context is in
`ORIGINAL-WORLD-ACTOR-ORDER.md`, `WORLD-BOOTSTRAP-CONTRACT.md`,
`PLAYER-HUD-BOOTSTRAP.md` and `NATIVE-CLIENT-BINDING.md`.

## Owned source identity

All paths are beneath `D:/Steam/steamapps/common/Deus Ex`.

| File | SHA256 |
| --- | --- |
| `System/Engine.dll` | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| `System/Core.dll` | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| `System/Extension.dll` | `93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C` |
| `System/Engine.u` | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| `System/DeusEx.u` | `257F5C166739B3D13884A8B8E98A47DDA458B325FA929B3B3B99BE056CB74480` |
| `System/DeusEx.ini` | `B6538EF8ED23E518FCFD88F0108C6B59ADF59FDC7AC3831463AD2A7E94F2AECC` |
| `System/User.ini` | `A276E5261238631A1DEF4AC769A35FF934F0A61877273F2D8AC331E0C478B531` |
| `Maps/00_Training.dx` | `BB66D95AAFC6AAE4179F8444E98661F3D5D27DAB06D13D7DDE7F4B5F4E24068D` |

These were fingerprinted, not modified. Engine/Extension hashes were reread
after static inspection and matched. Existing primary artifacts are
`artifacts/world-startup-contract-20261011/original-loadmap.asm`,
`original-spawnplayactor.asm`, and
`artifacts/world-native-startup-20261011/engine-exports-imports.log` /
`extension-exports-imports.log`. Fresh bounded `objdump` reads independently
confirmed the instructions and vtable entries cited below.

## Exact continuation after the stable native Actor partition

`UGameEngine::LoadMap` is exported thunk `103033A5 -> 1038C1F0`. The partition
ends at `1038DF46`; it is not the end of LoadMap or permission to publish a
begun playable world. The next normal selected sequence is:

| Original range | Observed behavior |
| --- | --- |
| `1038DF54..1038DF59` | Test actual Engine `+44` Client pointer; absent jumps to `1038E037` |
| `1038DF5F..1038DF76` | Call current Level `IsServer`; choose Engine `+68` current Level if true, `+6C` Entry Level if false |
| `1038DF76..1038DF7F` | Call helper `1038BF10(Client, selected Level, original FURL)` |
| `1038DF84..1038DFC0` | Compare current Level's outer package FName with `Entry`; non-Entry creates `GNewBrushTracker(current Level)` and stores result at Level `+FC` |
| `1038DFC6..1038DFDB` | If Engine `+4C` subsystem pointer exists, call its virtual `+84`, then pass that return to its virtual `+5C` |
| `1038DFDE..1038E005` | Ascending live Client viewport loop: call viewport `+60` object's virtual `+68`; if viewport `+64` exists, call its virtual `+68(1)` |
| `1038E037..1038E053` | Call current Level virtual `+D4` with actual slot-zero LevelInfo `+4F0` bit3 |

The subsystem virtual calls above must not be replaced by guessed successes.
This review does not claim the complete concrete audio/render implementation
of those two virtual contracts. The original concrete subsystem bindings and
the Quest embedding's actual equivalent services require separate evidence.

The first guard is **client existence**, not a viewport-count check, head
tracking state, possession, or an actor flag. An absent client legitimately
skips this block in a dedicated/headless path, but that does not implement the
user's standalone VR player. A real Quest viewport/client must take its
selected local-player path. Even the no-client branch still reaches the later
Level detail-change call.

### IsServer and the standalone choice

The exported ULevel UObject vtable starts at `10428AFC`:

- `+84` (`10428B80`) contains thunk `10303CD3 -> 1039EE20`, exported
  `ULevel::IsServer`.
- `+A8` (`10428BA4`) contains `1030209A -> 10396BD0`, exported
  `ULevel::SpawnPlayActor(UPlayer*, ENetRole, FURL const&, FString&)`.
- `+D4` (`10428BD0`) contains `1030315C -> 1039CF30`, exported
  `ULevel::DetailChange(int)`.

`IsServer` reads Level `+3C`; if nonnull and that object's `+38` is nonnull,
it returns zero. It makes the same test for Level `+88`; otherwise it returns
one (`1039EE20..1039EE47`). These are the native network/demo-driver/server
connection predicates, not the reflected LevelInfo.NetMode byte. For the
established standalone host with genuinely absent native network/demo drivers,
the result is true and `1038DF6E` selects the current Training Level. This does
not authorize guessing absent native storage merely from missing property tags.

### Helper 1038BF10: original per-viewport player creation

The helper is a cdecl three-argument routine. `1038BF38` reloads Client `+30`
count and `+2C` data every iteration, takes viewport `i`, creates an empty
temporary error FString, then at `1038BF88..1038BF95` calls selected Level
virtual `+A8` with:

```
SpawnPlayActor(viewport, ENetRole(2), originalURL, error)
```

The original Engine.u Actor.ENetRole enum has five ordered entries (see
`ORIGINAL-DESTROY-ACTOR.md`); value2 is `ROLE_SimulatedProxy`, not
ROLE_AutonomousProxy. The helper logs a null returned pawn through the original
error/logging service at `1038BF9F..1038BFC3`; it then destroys the temporary
FString and continues the live viewport loop (`1038C038..1038C039`). The string
at `1047EC38` is UTF-16 `Spawning new actor for Viewport %s`.

A portable root must own these viewport identities, original callback effects,
OUT error state, births, pairing edges, RNG and ensuing allocations together.
Spawning a named JCDenton, invoking Possess separately, or directly creating a
HUD does not execute this native call sequence.

## SpawnPlayActor: class authority, real Login and SetPlayer

Fresh static observations in the existing complete body artifact:

1. **Corrected class identity:** `10396C69..10396C8F` tests the supplied UPlayer
   against **UNetConnection**, not UViewport. The earlier Viewport label was
   incorrect. Export `UNetConnection::PrivateStaticClass` is ordinal1424,
   RVA`290C40` (`10590C40`, the literal pushed at `10396C75`). Registration
   `10402E30` supplies class-name literal `104837B6` (`NetConnection`, after the
   leading native `U` at `104837B4`), native
   size`3EB8`, and constructs that exact class storage at `10402E7C..10402E84`.
   The distinct UViewport class export is ordinal1439, RVA`281068`
   (`10581068`); its StaticClass thunk `10302C98 ->103484A0` and registration
   `10372370` are separate authority. The original initializes sandbox to None,
   reads player`+A8` **only for an actual NetConnection-derived player**, and
   passes that captured value to all ensuing class loads. An actual standalone
   Quest Viewport is a Player, not a NetConnection, so this positive type
   predicate leaves sandbox None; no Viewport sandbox field or fake service is
   required. Complete NetConnection sandbox semantics remain unaudited.
2. `10396C94..10396CC8` queries FURL `CLASS=`. When present it attempts that
   PlayerPawn-derived class load; failure falls through to the default chain.
3. `10396CD0..10396CEB` tries UTF-16 `usr:DefaultPlayer.Class` at `1047FB58`.
   `10396CED..10396D06` then tries `ini:URL.Class` at `1047FB34`. The expected
   base is original PlayerPawn UClass `1058C160`, with load flags2 and the
   retained sandbox argument. These are actual ordered load attempts, not an
   invented constant class or just the GameInfo's DefaultPlayerClass.
4. The original URL option strings are assembled in order with `?` separators
   at `10396D34..10396DA6`; URL portal/options/error plus selected class are
   passed to the real slot-zero LevelInfo's Game `Login`. Game is read at
   native LevelInfo `+544` (`10397078..1039707A`). The FName at `10583674` is
   exported `ENGINE_Login`; its ProcessEvent call is `103970ED`.
5. Null Login result follows `10397150..10397183`'s failure path. Success calls
   actual `APlayerPawn::SetPlayer(viewport)` at `10397188..1039718E`, whose
   thunk `103037EC` targets `10368C90`. That body disconnects any previous
   Player.Actor pair, writes Pawn `+5D4` Player and Player `+30` Actor, then
   ProcessEvents the most-derived **Possess** (`10368CD7..10368D06`, FName
   `10583684`). Pairing plus callback is not two independent commits.
6. Only after SetPlayer returns does SpawnPlayActor write Role=4 at Actor
   `+31`, RemoteRole=input byte at `+32` (`10397193..1039719A`), perform
   additional native fields/conditions, and invoke **TravelPreAccept** through
   exported FName `10583640` at `103971DE..103971F4`. TravelPreAccept is not a
   second Possess. This review does not certify the later travel-import and
   remaining SpawnPlayActor tail through its final return.

Owned `User.ini [DefaultPlayer]` currently has `Class=DeusEx.JCDentonMale`;
`DeusEx.ini [URL]` has that same Class and `Name=Player`. An explicit URL CLASS
remains higher priority. Owned `DeusEx.ini [Engine.Engine]` selects
`GameEngine=DeusEx.DeusExGameEngine`, `DefaultGame=DeusEx.DeusExGameInfo`,
`DefaultServerGame=DeusEx.DeathMatchGame`, `Input=Extension.InputExt`,
`Console=Engine.Console`, `Canvas=Engine.Canvas`,
`Root=DeusEx.DeusExRootWindow`, and the configured render/audio classes.
GameInfo selection still follows the separate already-audited URL GAME / map
DefaultGameType / client-sensitive configured-default chain. The actual begun
Level.Game receiver, not a new dummy GameInfo, owns Login.

Static package inspection independently shows
`DeusEx.DeusExGameInfo.Login` overrides Engine.GameInfo.Login: it calls
ApproveClass, substitutes JCDentonMale only when approval fails, calls its
compiled super Login at PC38, and for a nonnull result without the `Loadgame`
option invokes ResetPlayerToDefaults at PC112. The succeeding original path
includes compiled native3015 and optional dump-location operations; this
review has not established those services or chosen their runtime branches.
The super Login performs actual FindPlayerStart and player reuse/spawn logic.
None of these can be replaced with the demo camera/spawn-location shortcut.

## Later required native services and detail event

### Training selects moving-brush tracker creation

`1038DF90` reads current Level `+18` Outer, then Outer `+20` FName, obtains
its actual name and compares it through imported case-insensitive string
comparison (`10599BB0`) with UTF-16 `Entry` at `1047E768`. A zero/equal result
jumps to `1038DFC6`; a different package calls `GNewBrushTracker` and stores its
result at Level `+FC`. First Training's source package is `00_Training`, not
Entry, so this is a **positive selected creation branch**.

`GNewBrushTracker` export thunk `103021EE -> 1037C860` allocates `0x68` bytes
through the actual allocator, then constructs the tracker with the ULevel at
`1037C89E..1037C8A4`; allocation failure returns null. Its internal moving-BSP
ownership/query/update semantics are not established by this bounded read.
The current retail actor collision hash, a static Model, or working demo door
motion is not proof that this distinct tracker service exists.

### Input reset has an exact configured-class inheritance path

The viewport `+60` object receives virtual `+68` at `1038DFF3`. The UInput
UObject vtable `10428C18+68` points to exported ResetInput thunk
`10302A45 -> 10394910`. Its body clears key/button/axis state, not just one
controller stick value. Viewport construction at `103731E4..10373223` loads
`ini:Engine.Engine.Input` and stores the constructed input at viewport `+60`.
The current owned config chooses Extension.InputExt.

The original Extension XInputExt UObject vtable is `1005C3EC`; its `+68`
entry (`1005C454`) is thunk `10054642`, which jumps through IAT `100592D8`.
The static IAT value points at import-name record RVA`786D8`, named
`UInput::ResetInput`. Thus this configured InputExt inherits the real reset
operation here, not an unproven empty override. Input construction/registration
and its rollback still need a genuine port-owned service.

Viewport `+64`'s conditional virtual `+68(1)` follows the reset. The earlier
detail selection directly tests that object's `+64` HighDetailActors storage.
The complete render-device operation behind this virtual slot remains an
explicit audit/integration boundary, not permission for a no-op.

### DetailChange is a real event, even when high detail is enabled

LoadMap initially ORs slot-zero LevelInfo `+4F0` with8 at
`1038D6B3..1038D6BB`. It clears that bit only if Client exists, its viewport
count is nonzero, the first viewport's `+64` render device exists, and that
device's `+64` is zero (`1038D6C3..1038D6E3`). Original RenderDevice's native
StaticConstructor (`10302BBC -> 103713C0`) registers Bool `HighDetailActors`
(UTF-16 literal `1047BE10`) at native offset64 with Config flags4000 and mask1
at `103714E9..10371526`. Owned configured Glide/Software sections currently
set HighDetailActors=True. A Quest embedding must establish its actual
render-device/setting equivalent; reading a desktop INI is not proof of a
live native renderer pointer or permission to invent its state.

After partition/local-player setup, `1038E044..1038E053` re-reads the current
LevelInfo bit and calls `ULevel::DetailChange`. That body writes only bit3 of
actual slot-zero LevelInfo `+4F0` (`1039CFC2..1039CFD8`); when its `+544` Game
is nonnull it finds exported `ENGINE_DetailChange` FName `10583618` and invokes
GameInfo ProcessEvent at `1039D0BC`.

Engine.u's compiled `GameInfo.DetailChange` has 125 logical bytes and reads
the exact declared `LevelInfo.bHighDetailMode` ref1266. In low-detail mode it
iterates Actors and calls Destroy on bHighDetail && !bGameRelevant actors
(PC20..89). **Regardless of that branch**, PC90..122 iterates ZoneInfo and
calls its original LinkToSkybox. That 105-byte script iterates SkyZoneInfo
twice, sets SkyZone, and prefers those whose bHighDetail matches the actual
LevelInfo.bHighDetailMode. Therefore setting the flag or skipping the event in
high-detail mode omits required original world mutations. Named property
binding must still authenticate declaring schema/native packing, not infer
a false/default value from an absent serialized tag.

## Nearest next integration and honest remaining boundary

The historical nearest-service paragraph below is superseded by the2026-10-30
SpawnPlayActor return. Fresh bounded static inspection confirms GNewBrushTracker
thunk103021EE ->1037C860. Its factory allocates native size68hex at1037C88D,
then invokes constructor thunk1030168B at1037C8A4 with the actual Level. This is
not an authorized null tracker, opaque success or proof that moving brush/BSP
state can be omitted. The later services below remain required in order.

The nearest selected service after partition, for a real standalone Quest
client, is the **per-live-viewport SpawnPlayActor call on the current actual
Training ULevel**. It must use the ordered URL/user/config class authority and
real current GameInfo.Login, then SetPlayer/most-derived Possess under the
same bounded caller root. Existing isolated native pairing/root-window tests
are prerequisites, not a substitute for this real LoadMap continuation.

Do not mark startup complete after that one call. The remaining selected
tracker, subsystem rebinding/reset/flush, detail-change callbacks, the complete
SpawnPlayActor travel tail and later LoadMap continuation still need their
own original behavior, bounded ownership, exact failure rollback and actual
first-Training evidence. Missing services must remain named refusals. No-client
or empty-viewport diagnostic branches do not prove a playable standalone VR
world, original inventory/HUD ownership, movement/physics or crate gameplay.

## Static reproduction

Run only these external/port inspectors, never the original DLL/EXE entrypoints:

```powershell
$postObjdump = 'C:\ProgramData\mingw64\mingw64\bin\objdump.exe'
$postEngine = 'D:\Steam\steamapps\common\Deus Ex\System\Engine.dll'
$postExtension = 'D:\Steam\steamapps\common\Deus Ex\System\Extension.dll'
Get-FileHash -Algorithm SHA256 -LiteralPath $postEngine,$postExtension
& $postObjdump -p $postEngine
& $postObjdump -d -Mintel --start-address=0x1038df54 --stop-address=0x1038e059 $postEngine
& $postObjdump -d -Mintel --start-address=0x1038bf10 --stop-address=0x1038c09e $postEngine
& $postObjdump -s --start-address=0x10428afc --stop-address=0x10428bd4 $postEngine
& $postObjdump -d -Mintel --start-address=0x1039ee20 --stop-address=0x1039ee48 $postEngine
& $postObjdump -d -Mintel --start-address=0x10396bd0 --stop-address=0x103971f7 $postEngine
& $postObjdump -d -Mintel --start-address=0x1039cf30 --stop-address=0x1039d0d0 $postEngine
& $postObjdump -d -Mintel --start-address=0x103713c0 --stop-address=0x10371616 $postEngine
& $postObjdump -s --start-address=0x1005c44c --stop-address=0x1005c460 $postExtension
& $postObjdump -d -Mintel --start-address=0x10054642 --stop-address=0x10054648 $postExtension
& $postObjdump -p $postExtension
& '.\artifacts\prerequisites-20261009\build\script_bytecode_inspect.exe' `
  'D:\Steam\steamapps\common\Deus Ex' 'Engine.GameInfo.DetailChange' `
  'Engine.ZoneInfo.LinkToSkybox' 'Engine.GameInfo.Login' 'DeusEx.DeusExGameInfo.Login'
Get-FileHash -Algorithm SHA256 -LiteralPath $postEngine,$postExtension
```
