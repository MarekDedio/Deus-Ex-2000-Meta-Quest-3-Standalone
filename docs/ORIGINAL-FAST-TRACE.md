# Original FastTrace and BSP visibility

This implements a prerequisite shared by container destruction, sound occlusion
and AI hearing. It does not complete crate damage, debris, dropped contents,
physics, player possession or the tutorial. No new APK is part of this checkpoint.

## Primary retail evidence

Static inspection only: the owned original Engine.dll is not loaded or executed.
Its SHA256 is `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

`Engine.Actor.FastTrace` is native548, flags0x401. Original Engine.u declares:

| Field | Original type | Flags |
|---|---|---|
| TraceEnd | Core.Object.Vector | 0x80 |
| TraceStart | Core.Object.Vector | 0x90 |
| ReturnValue | Bool | 0x580 |

The native thunk RVA0x2103 targets VA0x103e35e0. Required TraceEnd evaluates
into an uninitialized12-byte temporary. Only afterwards does the wrapper copy
the opcode receiver's current Location into TraceStart (VA0x103e3607..0x103e3627),
then evaluate the optional Start expression. Expressions use FFrame.Object, not
the possibly different opcode receiver. End is already copied when Start runs.
The wrapper calls the receiver's actual native XLevel.Model.FastLineCheck with
`(End,Start)` and writes the zero-extended native result byte into Bool storage.

Model.FastLineCheck's thunk RVA0x1a37 targets VA0x103f3280; the recursive body
starts at0x103f33a0. Empty Nodes returns actual RootOutside. Otherwise:

- Compute Start distance, then End distance, each as `(z*nz+y*ny)+x*nx-W`.
  Arithmetic instructions round to PC64, with a Float store only after the
  completed distance (VA0x103f33ea/0x103f3418).
- Classify each stored Float's **signed integer bits**, not numeric distance
  with an epsilon. Positive zero is front; negative zero is back.
- Only NodeFlags bit0 participates. Neither NumVertices nor other node/polygon
  flags enter the solidity test. Bit0 set preserves incoming outside; otherwise
  front is outside and back is solid.
- Same-side endpoints descend that child. For a split, retain PC64 fraction
  `StartDistance/(StartDistance-EndDistance)`. Each midpoint component is
  `((End-Start)*fraction)+Start`, with one Float store at the end.
- Visit the **End-side child first**, with reversed segment arguments
  `(End=midpoint,Start=oldEnd)`. A blocked result stops immediately. Otherwise
  tail-visit the Start-side child with `(End=midpoint,Start=oldStart)`.
- A missing child returns its updated outside state. There is no hit location,
  actor collision, mover transform, extent or render-triangle query here.

Ordinary simulation precision is independently corroborated by Core1016e050/
1016defa's control-word setup. Render temporarily enables FastMath24 and restores
PC64 afterwards (10b14629/10b14723..10b1472c). This adapter implements ordinary
simulation PC64, not arbitrary script invocations inside an original render hook.

## Implementation and transaction boundaries

`quest_model_fast_line_check.h` uses integer dyadics for PC64 addition,
multiplication, division and exact Float store boundaries. It does not depend on
host Long Double width. Its signed general division normalizes both operands
to64 bits, uses a128-bit dividend and exact remainder/tie rounding. No original
binary code is called.

Traversal is iterative and reproduces the original End-first/reversed order.
It reuses only the existing BSP layout/cycle/work validation, not the different
flags6/epsilon/NumVertices behavior of Model.LineCheck. Nonfinite/overflow,
cyclic/malformed data, undefined signed-zero0/0 splits and exhausted work bounds
explicitly refuse. These are portable safety policies, not invented retail
answers. The standalone query has fixed maximum bounds that callers may tighten.

The runtime validates actual Engine.Actor.FastTrace and its exact three-field
schema. `CoreVector12` is trusted only after checking the original Core.Struct
owner/layout and the complete ordered X/Y/Z FloatProperty chain. Shape alone
cannot authorize an uninitialized native destination. End must receive a
complete Vector or full12-byte Context clear; no-write/partial input refuses
before Start's side effects. Start uses the existing sequential default hook.
Ordinary/raw/reflected/direct548 inputs are copied before the next input runs.

Model ownership follows immutable actor.nativeXLevel, validated by NativeLevelInfo,
and serialized ULevel.Model. The decoder requires a unique Level export; this
must be the already-proven actual Engine.Level. A foreign additional Level is
ambiguous, not a substitute. Selected Model must be actual Engine.Model even
when a native asset export legitimately has no serialized UClass pointer.
The per-host cache must match selected export/path. Reflected Level/XLevel
overlays and a misleading first brush Model cannot redirect it.

Model decode is bounded to64MiB scratch. Validation/query work is shared across
all548 calls in the same root transaction (8m/16m steps respectively), rather
than resetting the work budget on every callback. The query itself writes no
actor state or revision. Earlier script writes still join the normal journal
and roll back if a later trace or callback fails. Immutable decoded tables/model
may be cached; no renderer/OpenXR handles or audio resources are accessed.

## Verification scope

The pure suite uses hand-computed BSP regions, all256 node flags, absent polygon
vertices, both RootOutside values, signed zeros, crossing/end-first pruning,
deep iterative trees and refusal controls. Its independent x86 host oracle
selects PC64 nearest and uses volatile Long Double after every operation; it
does not call the original DLL or the production dyadic math.

Generated integration fixtures contain real serialized UE1 packages, ULevel and
UModel records, exact canonical Vector metadata and compiled original-style
bytecode. They test invocation forms, defaults, side effects, copied aliases,
full/partial destination writes, model ownership, malformed namespaces and full
save/revision/GC rollback. Owned Training probes separately test the actual
declaration, native level's selected Model and both clear/blocked rays. Their
helper comparison establishes adapter integration, not an independent retail
oracle; that authority comes from static evidence and the separate math suite.

Full tutorial/campaign behavior, dynamic actors/movers, general Actor.Trace,
physical support/gravity, actual sound/AI delivery and destructible crates remain
separate unfinished work. See SOUND-RUNTIME-CONTRACT.md for the earlier damage
and sound/noise boundaries.

## Checkpoint results (2026-10-15)

- Coherent full host build succeeds; the final negative-fixture-only rebuild
  also succeeds. CTest:96 passes/110 registered,14 explicit owned-data skips,
  zero failures,39.54s.
- Pure BSP:72081 checks/10 refusals, including independent PC64 host-oracle
  arithmetic and10000 generated acyclic BSP comparisons.
- VM:695581 checks/838 refusals, including373/54 new548 temporary/alias controls.
- Generated production integration:2190 checks/47 refusals. Both lazy reflected
  declaration handling and cold/warm local-versus-qualified model cache paths
  were exercised and corrected before acceptance. Malformed map/Vector load
  exceptions are accepted only at their specific expected refusal boundaries.
- Actual Training integration:250 checks,16 clear/32 blocked supplied paths,
  plus16 receiver-Location defaults; original source bytes/timestamps and complete
  saved state/revision/GC remain unchanged.
- Strict NDK ARM64 syntax checks pass for VM/runtime and the pure/integration
  fixtures; the final malformed-fixture exception handling also passes separately.
- Original NPC regression retains all18 waiting presentations (280519 checks,
  26 refusals; no software-preview-output checks in this run). Original crate152
  and lamp probes preserve the earlier264/512 refusal/rollback boundaries.

Ignored local logs: `artifacts/crate-fast-trace-full-build-final-20261015.log`,
`crate-fast-trace-negative-fixture-build-20261015.log`, `crate-fast-trace-ctest-final-20261015.log`,
`crate-fast-line-math-20261015.log`, `crate-fast-trace-vm-20261015.log`,
`crate-fast-trace-generated-final-20261015.log`, `crate-fast-trace-owned-final-20261015.log`,
`crate-fast-trace-ndk-syntax-final-20261015.log`, `crate-fast-trace-fixture-ndk-syntax-final-20261015.log`,
`crate-fast-trace-npc-regression-20261015.log`, `crate-fast-trace-damage-regression-20261015.log`
and `crate-fast-trace-lamp-regression-20261015.log`.
