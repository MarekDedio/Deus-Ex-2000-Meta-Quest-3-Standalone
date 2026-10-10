# Original script random stream (2026-10-10)

The portable actor runtime implements original `Core.Object.Rand` native167
and `FRand` native195. This is isolated interpreter/save work, not automatic
world startup, live NPC AI or newly accepted Quest gameplay. The installed
seated-player APK is unchanged.

## Original contract

Read-only disassembly of the user-owned GOTY `Core.dll` establishes that both
natives use the same imported `MSVCRT.rand` stream. The installed 32-bit CRT
independently supplies its fixed-width recurrence:

```text
seed = uint32(seed * 214013 + 2531011)
raw = (seed >> 16) & 32767
```

`Rand(Max)` returns zero without drawing for Max <= 0. A positive bound consumes
one draw and returns `raw % Max`, including Max1. Bounds above32768 are not
clamped or rejection-sampled. `FRand` multiplies one raw draw by the original
binary32 constant with bits `0x38000100` (rounded 1/32767). Its stored float range
is inclusive 0–1; raw32767 returns exactly1.0. This differs from the pinned
reference engine's C++ distributions; host libc and implementation-defined
standard-library engines are not used.

Original `Core.u` declares `RandRange` as a script function, native index0:
`Min + (Max-Min)*FRand`. Its unchanged bytecode runs through the VM. Equal and
reversed finite endpoints use the ordinary script arithmetic and existing numeric
validation; even equal endpoints consume a draw. Native1033
from the reference engine is not substituted for this original script.

The original Core initializer calls `srand(time(nullptr))`. Quest explicitly
supplies a seconds-since-epoch seed once when constructing its script runtime;
actor calls and ordinary map changes never reseed. The portable embedding API
defaults to deterministic seed1 for tests/replays. A nonbaseline seed, including
zero, is retained immediately, before any draw, so startup validation checkpoints
and failed map transitions cannot erase it. An absent seed record means exactly
the unconsumed seed1 baseline, not hidden current time or prior session history.

Source evidence:

- Original Core.dll SHA-256:
  `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
  appRand body101232d0 and appFrand101232e0 share rand IAT102ab4fc;
  execRand10134960 contains the nonpositive early return. appFrand's multiplier
  is at10176d70. Core initialization1016d720 calls srand at1016d7c0.
- Installed `C:\Windows\SysWOW64\msvcrt.dll`, version7.0.19041.3636, SHA-256
  `C1BE919E7DE267ED2FCFBB4DA52AAD912CB468069B58B6A00198FF3C525373A0`:
  rand RVA5c650, srand RVA5c680, thread-data seed at+0x14, initial value1.
- The local reference-engine pin is
  `677ee14c5b83486e6634687953779aafb7973ad6`; `Native/NObject.cpp` and
  `Utils/Random.cpp` describe different distribution behavior.
- Exact commands/disassembly and original script inspection are retained locally
  in ignored `artifacts/random-state-20261010/original-rng-binary-evidence.log`
  and `original-random-inspection.log`. No commercial files are redistributed.

The installed CRT uses fiber/thread-local CRT data; one engine-global portable script stream
models the game-thread calls across actors. This does not prove identical
initialization time, all future native draw order, the exact 2000-era CRT binary,
or original savegame compatibility.

## Transactions and persistence

The existing root execution transaction journals the optional uint32 seed.
Nested calls share it, and unsupported operations, instruction/write exhaustion
or failed callbacks restore every draw together with actor properties, completed
OUT writes and native clocks. Count/type checks happen before drawing. These
receiver-independent Core operations remain available in genuine nested Context
calls on presentation-inactive actors; the explicit public inactive-actor entry
guard remains unchanged. RNG-only commits do not republish render geometry.

Codec7 appends exactly five bytes after the codec6 sections: algorithm identifier1
and a little-endian uint32 seed. All 32-bit seeds are valid. RNG-only records have
empty object/default/birth/AI sections; a codec6 record still requires an AI
manager. Codec1–6 wire bytes remain unchanged when the seed is absent.

Runtime envelope10 requires codec7 and its seed record. Every earlier envelope
keeps its exact old codec requirement. Unknown algorithms, truncated/extra bytes,
wrong map/schema and codec/envelope mismatches reject without publishing a seed.
Read-only validation does not change the live stream. Successful load publishes
the prepared seed only after allocating/schema preflight, alongside the staged
actor graph. Legacy envelopes1–9 contain no RNG evidence and explicitly reset
to seed1; they do not merge current random history.

The global stream survives ordinary map replacement/unload and does not itself
activate the per-map archive guard. Committed actor/class-default/AI state still
blocks travel until explicit supported restoration; saving does not clear that
guard. Shutdown discards the whole engine session. See
[SCRIPT_STATE_SAVE.md](SCRIPT_STATE_SAVE.md).

## Verification scope

`portable_script_random_test` combines a uint64 recurrence/exact rational float
oracle with real generated UE1 metadata and production runtime calls. Controls
cover nonpositive/Max1/large bounds, raw/declaration/nested calls, shared receivers,
inclusive endpoints, argument validation, completed OUT and unsupported rollback,
instruction/write limits, inactive receivers, pre-first-draw saves, warm/cold
restoration, malformed saves, global map carry and the unchanged actor travel
guard. Codec controls independently check literal wire bytes, legacy1–6, mixed
sections, ordering, algorithm rejection, truncations and cumulative budgets.

The original-data integration executes actual Core Rand/FRand declarations and
scripted RandRange, with reversed/equal ranges and exact endpoint seeds. Actual
Bird.TweenToWaiting executes its original compare/assignment/Tween branches for
Idle1 and Idle2, including selected-write failure after a draw and mixed
property/native-clock/RNG checkpoint restoration. This is not an authored
campaign playthrough or live ticking.

Local build/test/APK evidence is retained under ignored
`artifacts/random-state-20261010/`. The backend-only APK is archived at
`artifacts/release-random-state-20261010/DeusExQuestVR-random-state-20261010.apk`.
No new installation, headset capture or device performance acceptance is claimed.

Final verification on the production source:

- All 49 ordinary host tests pass; the three optional original-data CTest entries
  skip without a game root, and all three pass separately with the read-only
  owned installation. The original actor suite covers Training, UNATCO Island
  and Intro, including the real Pigeon0 idle branches.
- Random controls: 58,942 checks / 37 rejection controls. Structural save-codec
  controls: 6,337 / 5,634 rejections, preserving codec1–6 wire behavior.
- Explicit original StartUp now reaches unsupported Sleep256 at
  `DeusEx.ScriptedPawn.StartUp:6`, opcode0x61, after evaluating FRand. Actor counts,
  roots, world revision and full serialized state remain unchanged after refusal.
- Android ARM64 build and APK v2 signature verification pass. The 16-entry APK
  contains no commercial game packages; both port native-library hashes change.
  See [QUEST-TEST-BUILD.md](QUEST-TEST-BUILD.md) for archive sizes/hashes.
- Evidence includes `host-build-final-verified.log`, `ctest-final.log`, `codec-controls.log`,
  `random-controls-agent-run1.log`, `original-actor-final.log`,
  `original-runtime-final.log`, `original-defaults-final.log`,
  `android-build-final.log`, `apk-signature.log`, `apk-hashes.log` and
  `apk-entries.log`. Initial compiler warnings, terminator-only generated actor
  fixture failure and the stale FRand startup assertion remain in earlier logs;
  they were corrected without weakening production gates or rejection controls.

Sleep/latent scheduling, automatic world/player startup, AI processing, complete
native coverage, live animation, campaign progression and per-map dynamic saves
remain unfinished. This increment does not enable unsupported waits or fabricate
world startup success.
