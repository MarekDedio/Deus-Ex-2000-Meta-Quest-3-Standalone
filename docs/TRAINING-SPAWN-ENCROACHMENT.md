# First Training spawned-pickup collision

Source checkpoint,2026-10-21. Only `Maps/00_Training.dx` is the actual-map
integration target. This work connects the original post-PostBeginPlay Spawn
collision fragment to the real current-Level encroachment service. It does not
complete SpawnActor, world/player startup, live crate destruction, surviving
usable loot or gravity. No replacement APK is justified by backend tests alone.

## Original authority

Owned original `System/Engine.dll` SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
The exported SpawnActor thunk103016DB targets body10394F60. Static evidence is
`artifacts/spawn-frame-audit-20261011/original-level-spawnactor-disassembly.log`;
frame/callback identification is in that directory's verification.md.
Commercial files are data-only; no original DLL/EXE is loaded or executed.

The complete fragment10395591..103955EE follows PostBeginPlay1039558E:

- Native integer argument+34: ANY nonzero value skips the fragment, including
  all pose reads, collision and destruction. The currently exposed callers
  supply0; no script skip option is invented.
- Copy current Rotation Pitch/Yaw/Roll from Actor+120 FIRST, then current
  Location XYZ from Actor+114. These are post-callback fields, not original
  Spawn arguments. Raw Float words require no arithmetic/finite filter here.
- Call the SAME retained native Level's CheckEncroachment with the SAME birth,
  copied pose and literal touch0. This invokes the real lower query, not a
  source-flag-only approximation, render ray or zero-length line substitute.
- False proceeds to SetInitialState, the eligible FindBase pass and PPP.
- True calls that retained Level's DestroyActor(birth,0), ignores its result
  and returns None. Original destruction is a separate full dependency; the
  runtime explicitly refuses its unsupported path rather than silently leaving
  a birth alive or returning a fabricated clear result. See ORIGINAL-DESTROY-ACTOR.md.

No extra bDeleteMe gate, actor-array snapshot, private transaction or refreshed
work allowance is introduced. The higher service's original mask0D/moving-
brush fast-false admission does not eagerly require a Cylinder, Engine or
Actor slot0. Reached lower providers retain their live identity checks.

## Memory and observations

ActorEncroachmentCheck previously pre-reserved three hit records for EVERY
defined actor before querying. In the real NanoKey spawn this requested224280
bytes, exceeding the existing16MiB combined state limit by392 bytes. That was
an overestimated allocation envelope, not evidence that the collision query
must be skipped or the limit raised.

The collector now exposes optional pure host storage admission around explicit
vector growth. It reports old/requested capacity before reserve and old/actual
capacity immediately afterwards. The runtime charges the overlapping old+new
peak, then retains actual capacity in its lexical scratch ledger throughout
later primitive/Model/nested-query admissions. Empty collectors allocate no
records. Query encounter order, live Next reads, tags, callbacks, hit/work
limits and enclosing rollback ownership are unchanged.

The rollback-only startup result owns bounded detached Spawn observations:
actual identity, actual query pose/touch, completion/result, balanced native
hit storage and ordered state/base/PPP tail. Records are addressed by index
across callback growth, not by invalidatable vector references. Owning vector
capacity and growth peaks are charged; there is no publishing pose setter.
The observation CLI can require an actual completed clear Training NanoKey
check with `--require-spawn-encroachment`, and compares raw pose words and all
packet fields across repeated runs. That option is not a startup-success gate.

## Verification boundaries

Verified2026-10-21:

- Detached Spawn fragment:14368 checks,1759 branch/raw-pose cases and42
  refusal controls; generated actual host Spawn fixture:9919/164.
- Encroachment collector:112597 checks/47 refusals,304 independent queries,
  nested retained-capacity and every allocation-admission failure controls.
- Strict ARM64 runtime and all four changed/new test sources pass. Android
  native module/link passes; that is not an installable replacement APK.
- Coherent full desktop build passes. Shared CTest executes159 of184 listed
  tests with25 explicit owned-data skips and zero failures in71.56s. Those
  skips are not actual-map evidence; owned checks below run separately.
- Actual first Training startup repeats twice with26 identical Spawn packets.
  NanoKey1 completes its real clear active-hash check, SetInitialState, base
  and PPP; native hit marks/arenas balance0 before and after. It forwards
  Location(3454.31104,987.412842,-85.9000778), Rotation(0,0,0), touch0.
- Both attempts next refuse `Unsupported original active-hash DestroyActor
  ordering contract` at PickupDistributor0/PPP slot1195, original
  `DeusEx.PickupDistributor.PostPostBeginPlay` PC244/opcode97. Each uses
  533311 instructions,39710 writes and437451/1048576 shared native work.
  Exit2 reports this unfinished startup boundary, NOT a passing startup.
- Complete checkpoint/state/revision/hash/RNG/client/lookup/Actor/clock/GC
  rollback passes and all40 owned source files remain unchanged. No other
  map is loaded; no begun-play override, player possession or world is
  published. The earlier392-byte refusal is gone without raising limits.
- Separate focused actual first-map movement passes122706 checks/21 refusals,
  including the real upper/lower crate cap sweep and genuine Doctor clock/
  committed-Move restoration. Conversation binding passes156871/17. Original
  isolated crate callbacks pass347/two genuine contents refusals: Game=None
  still relevance-deletes spawned contents there. This is NOT surviving loot.

Ignored evidence: `artifacts/training-spawn-encroachment-*-20261021.log`.

`quest_spawn_encroachment_test` independently checks the detached fragment,
including nonzero skip, exact provider order/current reads, raw Float words,
ordinary false/true and ignored destroy result, missing/failing providers and
shared-work/caller-owned rollback. Its mock destruction is NOT a native
DestroyActor implementation.

`portable_actor_spawn_test` uses a generated original-schema fixture whose
PostBeginPlay changes pose. It checks the actual host coordinator's forwarded
pose and state/base/PPP order. This fixture exercises the original fast-false
branch; it is not active-hash or real-map acceptance.

Actual first Training startup must separately run the real enabled hash and
NanoKey, repeat its observed frontier, restore the complete save/VM/hash/RNG/
lookup/Actor/client/clock/GC state and preserve all40 owned source files.
Completed world/player startup, blocked-spawn destruction, live weapon damage,
collectible surviving loot, support/falling/landing and Quest acceptance remain
required. Existing dormant/general SpawnZone shortcuts are not repaired or
presented as complete by this fragment.
