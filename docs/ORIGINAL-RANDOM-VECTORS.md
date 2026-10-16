# Original random vectors for crate debris and drops

This backend checkpoint implements original Core.Object.VRand252. It is not
proof of live crate breakage, surviving loot, gravity, world startup or a new APK.
The generic Pawn-only damage helper is not widened to invent decoration health.

## Read-only primary evidence

Owned Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
No original DLL or executable is loaded or executed. Function schemas come from
the owned Core.u and native behavior from static disassembly.

- Core.Object.VRand: native252, flags0x2401, no inputs, sole ReturnValue
  StructProperty flags0x580, dimension1, actual Core.Object.Vector. The return
  destination requires validated canonical X/Y/Z Float declarations, not merely
  a value shaped like three numbers. Export thunk101031ed targets10139080.
- appFrand101232e0 returns an **unspilled** PC64 x87 result from the imported
  MSVCRT draw times Float bits0x38000100 (`32769 * 2^-30`). Reusing the Float
  result of script FRand195 would round too early.
- Each attempt draws X, Y, Z in order. Retained coordinate is exactly
  `(draw*32769 - 2^29) * 2^-29`. X/Y transform and store to Float; Z's store at
  101390b9 is FST, not FSTP. The rejection sum is therefore
  `PC64((Zretained*Zstored + Ystored*Ystored) + Xstored*Xstored)`.
  Reject strictly greater than Double1; equality is accepted. The pinned
  alternative engine's reversed rejection condition is not used.
- The raw triplet `{16384,16384,32767}` produces X/Y=2^-15 and stored Z=1,
  retained Z=1-2^-29. The mixed sum is exactly1 and must be accepted. Squaring
  only stored coordinates yields1+2^-29 and incorrectly rejects it.
- Normalization recomputes **stored** coordinate squares in z/y/x order and
  spills their PC64 sum to Double at10139101. appSqrt thunk101032ba targets
  10123290, `fld Double; fsqrt; ret`. Its square root remains PC64, not a Double
  return spill. Double1 divided by that root remains PC64, then each stored
  coordinate product is stored Float. There is no retail epsilon/zero guard;
  none of the32768 possible raw samples produces an exact zero coordinate.

## Portable implementation and safety

quest_script_random_vector.h uses the existing integer dyadic PC64 operations,
an explicit nearest-even Double spill and a correctly rounded integer square
root. The latter scales the positive Double into an exact127/128-bit integer,
finds the integer root in64 bounded steps and rounds upward iff the remainder
exceeds the floor root. An integer radicand cannot tie `(root+1/2)^2` because its
fractional part is1/4. No host long-double width or approximate reciprocal
supplies production arithmetic authority.

VRand consumes the same engine-owned saved MSVCRT stream as Rand/FRand. It does
not create a per-actor generator or persist an implementation-defined standard
library RNG. Map changes retain the stream; warm/cold save restoration retains
its exact continuation. A later callback failure restores every random draw
together with actor values, births, deletion and other root-transaction effects.

Retail rejection sampling is unbounded. Portable policy caps one call at4096
attempts and all nested VRand calls at one million raw draws per root VM
transaction. Exhaustion refuses/poisons the whole transaction, not a fabricated
axis, local reseed, dropped result or partial success. Detached helper refusal
does not itself rewind its caller's seed. Nonfinite/malformed math inputs and
zero normalization exposed only through detached tests refuse explicitly;
those are safety policy rather than invented original clamps.

## Remaining chain

The preceding sound-routing checkpoint let actual None-instigator fatal damage
reach Engine.Fragment.CalcVelocity:7 and direct genuine Destroy279 reach
Containers.Destroyed:681, both missing VRand252. The new native removes that
primitive boundary. Both actual fatal None-instigator damage probes now reach
GetMeshTexture1013 at DeusExDecoration.Frag:269 and restore the whole checkpoint,
including provisional fragments and consumed RNG. Legal nonnull Pawn input
still stops earlier at MakeNoise512:428; no NPC is substituted for the VR player.

Genuine native279 destruction in explicit isolated begun/non-startup fixtures
now completes Containers.Destroyed (218/215 instructions), creates one actual
DeusEx.Lockpick birth and deletes it through real Actor.PreBeginPlay relevance
because Level.Game=None. This is the original lifecycle of an unstarted world,
not a reason to mark the item game-relevant, directly add inventory or claim
surviving loot. The whole removed-crate/deleted-birth/RNG checkpoint round-trips;
repeated Destroy does not duplicate contents. A budget refusal one instruction
before completion restores the exact original checkpoint/revision/GC graph.

Original player possession/world startup, MakeNoise512's native clock/AI
hearing, GetMeshTexture1013, fragment lifecycle, item lifecycle/relevance,
falling/landing/Base support, bound-player native audio and whole committed
scene publication remain separate requirements. Tests that set isolated saved
begun/startup preconditions do not prove that the original world has started.

## Verification (2026-10-16)

- Pure350918 checks/16 refusal controls pass: all32768 retained coordinate
  samples,50000 independent x87 FSQRT/Double-spill tuples,20000 independently
  sampled VRand sequences, deterministic mixed-Z equality, endpoints and safety.
- Production generated RNG61608/49 passes, including raw/reflected/virtual/
  global/nested/Context calls, exact shared draws, warm/cold/map continuation,
  malformed canonical declarations, partial Vector/OUT and later actor-write
  rollback. A real script loop reaches the shared one-million-draw limit and
  restores the exact complete saved state; no test-only native budget bypass.
- Separately invoked actual Training damage152, genuine destruction347/two
  late-budget refusals and NPC280519/26 pass. All18 accepted waiting presentations
  are retained. Owned source bytes/timestamps and native destruction GC checks
  remain unchanged. The dropped Lockpick's real deletion is reported, not hidden.
- Coherent full host build succeeds; CTest100 passes/115 registered/15 explicit
  owned-data skips/zero failures in30.88s. Strict NDK ARM64 syntax passes the
  changed runtime/vector/RNG/destruction/damage fixtures.
- Source/tests only: no replacement APK or on-device crate acceptance is claimed.

Local ignored evidence: artifacts/crate-vrand-pure-initial-20261016.log,
crate-vrand-generated-budget-20261016.log, crate-vrand-full-build-20261016.log,
crate-vrand-ctest-final-20261016.log, crate-vrand-destruction-final-20261016.log,
crate-vrand-damage-final-20261016.log, crate-vrand-npc-final-20261016.log and
crate-vrand-ndk-syntax-20261016.log. Earlier failed diagnostic logs deliberately
retain the old expected native boundaries; final tests supersede those outcomes.
