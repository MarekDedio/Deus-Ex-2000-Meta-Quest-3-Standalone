# Original Actor.LastRendered native723

This bounded native query supports the first Training NPC foundation. It is
not renderer timestamp publication, a substitute for NPC Tick/perception,
completed crate/loot/gravity work, or proof that a first Training NPC has
actually reached this call. It integrates no other map. The surrounding live
runtime binding and reached-call evidence remain the root caller's work.

## Static owned-source authority

Read-only audit on 2026-10-26 used
`D:/Steam/steamapps/common/Deus Ex/System`. The commercial EXE/DLL files were
never executed, loaded as modules, patched or linked. Existing port inspection
tools read package data and GNU objdump statically reads the original PE32.

| Owned file | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

The actual declaration is **Engine.Actor.LastRendered**, native index723,
Function flags401 hex, logical bytecode length0. Its only child parameter is
`Actor.LastRendered.ReturnValue`: scalar `FloatProperty`, flags580 hex.
There are no inputs, optional parameters, thresholds or default arguments.
`DeusEx.ScriptedPawn` inherits this Actor function; the owned package has no
separate `ScriptedPawn.LastRendered` declaration.

The Engine export-table index1946/ordinal1947 is
`?execLastRendered@AActor@@QAEXAAUFFrame@@QAX@Z`, RVA21DA. Preferred-image
thunk `103021DA` jumps to the complete body `1036DE30..1036DE6E`.
The separately exported registration cell `intAActorexecLastRendered`,
index2237/ordinal2238/RVA17CBFC, is at `1047CBFC` and contains DWORD103021DA.
The initializer at `1037F460..1037F478` pushes that cell, then literal2D3 hex
(723), and calls the imported `GRegisterNative` through10599E6C. Thus the
native723 declaration, exported registration and machine body are joined;
an inferred function name or unrelated nearest-native class is not authority.

Addresses/offsets here are the original PE32 preferred-image layout, not
portable C++ addresses or permission to reinterpret a portable object.

## Exact packet and arithmetic

The wrapper does the following, with no external/virtual calls:

1. At1036DE30..1036DE38, load its FFrame parameter and increment the Code
   pointer at frame+C by1. This consumes the no-input EndFunctionParms byte;
   there is no expression evaluation or argument/default read in the wrapper.
2. Read the receiver's actual native XLevel pointer at Actor+AC.
3. `FLD m32` of receiver `LastRenderTime` at Actor+34.
4. `FSUBR m64` of retained native ULevel TimeSeconds at ULevel+DC, producing
   **Double native time minus loaded Float last-render time**, rounded to PC64.
5. `FCOM m32` against Float positive0 at10426D54. Independent `.rdata`
   bytes at that address are `00000000`. `FNSTSW; TEST AH,1` checks C0 alone:
   a negative **or unordered** retained result takes the clamp branch.
6. The clamp branch pops the age, loads Float positive0 and `FSTP m32`s it
   to the caller result. Otherwise `FSTP m32` stores the retained age directly.

The caller's VM packet adapter must authenticate this exact no-input/Float
return declaration, consume the original argument terminator, and publish a
written Float result. The pure helper below does not claim to parse FFrame
bytes. A malformed packet or substituted declaration must be refused by that
adapter before native provider reads. Subsequent script comparisons consume
the **stored Float**, not retained PC64 or a host Double.

There is no five-second comparison, upper cap, elapsed-time accumulation,
render-visible test, clock advance, timestamp store or special never-rendered
return value in723. In particular:

- A genuine initialized last-render timestamp-10 at native time0 returns10.
- Exact negative zero is not less than0: it is returned as negative zero.
- Negative ages (including tiny retained negatives that would Float-round to
  negative zero) return positive0.
- Quiet/signaling NaN or invalid Inf-minus-Inf yields unordered and returns
  positive0. This still reads the native Double memory operand: a NaN timestamp
  cannot short-circuit that required access.
- Ordered-positive infinity and positive Float overflow return positive Inf;
  the wrapper does not add a finite-input/output policy or saturation.

`quest_native_scalar.h` models this arithmetic using `Load(lastFloat)` for
the actual FLD, `Memory(nowDouble)` for the raw FSUBR memory operand, retained
`Subtract`, C0-compatible comparison176 and one final `StoreFloat`.
The chosen profile is masked x87 PC64/nearest-even. For native time
`1+2^-24`, last-render time `-2^-63`, the PC64 sum is just above a Float
midpoint and returns Float bits3F800001. An eager host-Double subtraction
incorrectly loses the small term and returns3F800000.

## Native fields and initialization

The reflected declaration is actual `Engine.Actor.LastRenderTime`, scalar
FloatProperty flags0, no referenced type. Its original native offset+34 and
the separate native XLevel/ULevel Double clock are corroborated in
`ORIGINAL-ACTOR-STASIS-PREREQUISITES.md` and `ORIGINAL-LEVEL-CLOCK.md`.
The portable runtime already has the genuine optional native last-render
slot/reflected-save mirror and loader-owned native Double clock; this query
must read those exact initialized bindings.

Original LoadMap's loop loads DWORDC1200000 (-10 Float) at1038DA28 and writes
Actor+34 for each nonnull actor at1038DA3F. Spawn independently stores the
same literal at10395572. Those are separate authentic initialization phases,
not a fallback/default implemented by `LastRendered`. Absent/uninitialized
native storage must refuse instead of substituting a CDO zero, synthetic-10,
LevelInfo.TimeSeconds Float or renderer visibility/camera heuristic.

The native XLevel edge is distinct from named script `Actor.Level` and
script-visible/patchable XLevel overlays. The edge is captured before the
Float read, then retained for the later Double access. Root bindings must
validate native level identity, current lookup/map generation and raw scalar
storage; this helper authenticates no pointer lifetime itself.

## Helper/provider contract

`native/quest_actor_last_rendered.h` exports
`QueryOriginalActorLastRendered(actor,queries)` and:

- `OriginalActorLastRenderedQueries`: `nativeLevel(actor)->identity`,
  `lastRenderTime(actor)->Float`, `nativeTime(capturedLevel)->Double`,
  mandatory shared `work()`.
- `OriginalActorLastRenderedResult`: `returned` Float, `clamped`, and
  `steps/levelReads/lastRenderReads/nativeTimeReads` counters.

Each reached provider first charges the same caller-root work authority.
Raw Float/Double provider results preserve all source bits, including
signaling NaNs. They are not preconverted or arithmetically sanitized before
the helper performs the actual x87 load/memory-operand conversion.

The receiver must be nonnull. The normal order is native edge, last-render
Float, then native Double, with all three reads even for unordered inputs.
A null native edge is refused at its would-be Double dereference, after the
preceding Float load. Missing providers are demanded only when reached; a
missing work service cannot execute providers. All failures propagate without
a local transaction, rollback or fabricated successful Float. Any prior
provider effects must unwind with the caller's genuine root. The helper does
not charge the returned VM Float as a world-property write.

## Tests and remaining acceptance

`desktop/quest_actor_last_rendered_test.cpp` covers exact source/read/work
order and counts; initialization-10 example; no timestamp/clock mutations;
negative/equal/signed-zero/subnormal boundaries; the PC64-vs-Double witness;
Float overflow; quiet/signaling NaNs; captured native edge/last value versus
fresh clock; null receiver/level, missing providers/work and every reached
provider/work failure prefix. Explicit mock-caller rollback is not proof of
the production journal. No pure-helper test purports to authenticate a VM
packet or native field descriptor; those require adapter tests.

An independent oracle executes only its own GNU x87 FLD/FSUBR/FCOM/FSTP
instructions under masked PC64/nearest-even. It does not use production scalar
arithmetic to compute expected results. It compares final Float bits, C0
selection and full provider/work order for624 Cartesian exceptional/boundary
pairs, two retained-precision witnesses and32768 deterministic raw-bit pairs:
33394 oracle cases. Unsupported hardware/toolchains fail instead of silently
skipping numeric acceptance.

The runtime now binds723 in `portable_actor_last_rendered.inl`. It fully
authenticates `FunctionFor(Engine.Actor.LastRendered)` independently, because
numeric native calls supply no declaration and named calls can supply only a
lazy identity. Any supplied identity/schema is additionally checked. The
receiver must have an actual initialized native LastRenderTime mirror and an
authenticated native ULevel Double clock; neither a CDO timestamp nor reflected
LevelInfo.TimeSeconds is substituted.

Helper verification passes167,096 checks/14 refusals, including the33,394
independent oracle cases. The production driver invokes actual decoded723
bytecode, tests native-Double versus decoy reflected-Float time, PC64 witness,
negative/NaN clamping, unchanged timestamps/clocks, malformed input/return
schema and absent native mirror, with full caller-root undo on failure.
Desktop, Android native and strict ARM64 checks pass. This does not establish
renderer timestamp publication or installed-build/headset acceptance.

The original560-byte `DeusEx.ScriptedPawn.Tick` script contains
conditional723 calls (offset116 and372), but their mere existence does not
establish that the current first Training NPC path reaches either call.
