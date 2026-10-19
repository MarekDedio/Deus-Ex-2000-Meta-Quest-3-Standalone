# Original AActor.moveSmooth push dependency

Bounded static audit, 2026-10-19, for the first `00_Training` encroachment push
path. No commercial DLL/EXE was loaded or executed. No map was loaded, no
build was run, and no gameplay/runtime implementation is supplied by this
audit. In particular, this does not establish playable crates or full Move.

Owned immutable source hashes, rechecked during inspection:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

## Exact identities and complete evidence boundary

Engine export ordinal2052 is `AActor::moveSmooth(FVector)`, thunk `10301532`
to `103C9020`. The COMPLETE normal body ends at `103C92AA` (`ret 0Ch`).
The following logging/rethrow catch fragments end at `103C92DC`.
Arguments are this/source S in ECX and original Delta D at stack `+08..10`.
Native EAX is the integer result; it is not recomputed from the final pose.

The full earlier dump is
`artifacts/training-level-encroachment-20261019/movesmooth-body.asm`.
New ignored evidence in `artifacts/training-move-smooth-20261019/` contains:

- `movesmooth-full.asm`: bounded complete body including its catch fragments.
- `moveactor-thunk.asm`, `level-vtable-slot88.asm`: exact downstream movement
  identity; the previously captured moveSmooth thunk pins its own export.
- `twowall-thunk.asm`, `twowall-body.asm`: exact downstream helper and its
  complete normal body, including both returns.
- `safenormal-thunk.asm`, `safenormal-body.asm`: exact imported Core helper.
- `numeric-one.asm`, `numeric-zero.asm`: original constants, not inferred
  decimal approximations.
- `original-hitwall-schema.log`: Engine.u HitWall declaration read by the
  existing package inspector, not executed bytecode or a commercial module.
- `exception-handler.asm`, `exception-function-info.asm`: its own EH binding.

Export/import identities are in the earlier
`training-level-encroachment-20261019/engine-export-import-identities.log`.
Reproduce the main body with NDK27 LLVM18:

```
llvm-objdump --disassemble --x86-asm-syntax=intel --no-show-raw-insn \
  --start-address=0x103c9020 --stop-address=0x103c92dd Engine.dll
```

Engine MoveActor export1298 thunk `10303C60` reaches `103990E0`. Export722's
ULevel UObject vtable is `10428AFC`; its `+88` word at `10428B84` is precisely
`10303C60`. TwoWallAdjust export1696 thunk `10301C8A` reaches
`10323770..103239A0`, with void return/`ret 14h`. Its four Vector references
plus Float argument are described below. Core SafeNormal import IAT
`10599B80` is `FVector::SafeNormal() const`; Core export1164 thunk `101031C0`
reaches `10144BE0..10144C5E`. Neither normalizer identity is a guessed
replacement-engine implementation.

## One hit packet, at most three real MoveActor calls

`103C9045..90A8` constructs one FCheckResult on the local stack: all44 bytes
are zero except Time Float1 and Item int-1. It includes zero Next, Actor,
Location, Normal and Primitive. The imported FVector constructor at IAT
`10599B28` supplies the zero vectors. This SAME packet H is used for every
subsequent MoveActor call; it is **not reset between calls**.

Call1 at `103C90C7` is:

```
S.XLevel.MoveActor(S, originalDelta, S.currentRotation, H, 0,0,0,0)
```

The four options are the original bTest, bIgnorePawns, bIgnoreBases, bNoFail
contract pinned by `ORIGINAL-ACTOR-MOVE.md`. Actual XLevel is native Actor
`+AC`, not reflected Actor.Level `+A8`. Its virtual `+88` is called. Rotation
is copied from the then-current native three-int field `+120..128`.

Store call1's EAX at `103C90D6`; **every normal return restores this exact
stored integer** at `103C929A`, regardless of the results of calls2/3.
The first result does not gate sliding. Instead, compare H.Time to Double1
at `103C90CD..DE`: when H.Time is an ordinary ordered value >=1, return it
immediately. The original FNSTSW/test checks C0 only, so unordered Time also
takes the continuation branch. A portable nonfinite/domain refusal must be
explicit, not invented retail false behavior.

No own bStatic/bMovable/bDeleteMe, begun-play, class, Physics, zero-Delta or
collision-flag gate exists in moveSmooth. Full MoveActor supplies its own
gates and effects. In particular, MoveActor's genuine small-motion branch can
leave H unchanged; reuse of a previous H.Time on later small calls must not
be hidden by resetting the packet or skipping an exact-zero slide.

## First-wall projection: retained and stored operations

Use F(x) for one binary32 store/reload and E for the original retained x87
arithmetic (the port's established original precision contract), not host
double or freely reassociated SIMD. N is call1's stored H.Normal.

At `103C90E4..90F8`, compute retained dot in this order:

```
dot = E(E(D.z*N.z + D.y*N.y) + N.x*D.x)
```

Then `103C90FA..9125`:

```
projectZ = F(N.z*dot)
projectY = F(N.y*dot)
projectX = E(N.x*dot)                 // NO Float store
rejectZ  = F(D.z-projectZ)
rejectY  = F(D.y-projectY)
rejectX  = E(D.x-projectX)            // retained, not F(rejectX)
remaining = E(Double1 - H.Time)
```

`103C912B..9149` multiplies rejectZ, rejectY, rejectX by that same retained
remaining fraction. Store slide.z then slide.y to Float. Store slide.x to
Float with **FST**, retaining its unrounded value on the x87 stack. Thus all
three packet arguments for call2 are Float words, but the next gate's X
product uses retained slideX, not the stored slide.x.

At `103C914C..9167`, compute the forward-direction gate as retained
`E(E(D.y*storedSlideY + D.z*storedSlideZ) + retainedSlideX*D.x)`.
The Y product is evaluated first, then Z, then the retained X product.
Compare to Float+0 at `10426D54`. Negative OR unordered results return the
saved first MoveActor result. Ordered zero passes: this is **>=0**, not >0.
An epsilon or zero-slide skip changes which real movement callbacks execute.

Original Double1 at `104298A0` is bits `3FF0000000000000`; the zero Float is
bits `00000000`. Neither factor is a new arbitrary clipping tolerance.

## Slide move, second hit, HitWall, two-wall move

When the forward gate passes, copy the first H.Normal's three raw words into
OldHitNormal (`103C9178..918E`). Set DesiredDir to imported
`SafeNormal(originalDelta)` at `103C9191`. This is the ORIGINAL Delta, not
slideDelta or velocity. The input Delta is never overwritten by this body.

Call2 at `103C91E2` is actual `S.XLevel.MoveActor(S, storedSlideDelta,
S.currentRotation, sameH, 0,0,0,0)`. It rereads XLevel and copies current
Rotation, rather than reusing the first call's three-int values. Its native
return is ignored. At `103C91E8..91F6`, compare the CURRENT H.Time against
the same Double1 with the same C0-only condition. Ordered >=1 returns the
saved first result. Otherwise:

1. At `103C91FC..9230`, dispatch **S.HitWall(H.Normal,H.Actor)** using a copy
   of the second hit Normal and Actor pointer. No nonnull-Actor gate occurs.
   There was no explicit moveSmooth HitWall for the first hit. MoveActor can
   independently emit its own Bump/BumpWall/touch/zone events.
2. At `103C9233..9249`, invoke native
   `S.TwoWallAdjust(DesiredDir,slideDelta,H.Normal,OldHitNormal,H.Time)`.
   The first FOUR arguments are nonconst Vector references, in that order;
   the last is the second H.Time passed by value after HitWall returns.
   The Normal reference points to H, not the event's temporary parameter copy.
3. Call3 at `103C9291` uses the adjusted slideDelta, again current Rotation,
   again current actual XLevel, the SAME H and four0 options. It runs even
   if the adjusted vector is zero. Its return and final H.Time are ignored.
4. Return the saved FIRST result. No fourth movement, recursive retry,
   velocity assignment or extra explicit HitWall follows call3.

HitWall's Engine.Actor declaration is native0/functionFlags0800, scalar
HitNormal StructProperty constrained to Core.Object.Vector and scalar HitWall
ObjectProperty constrained to Actor; both parameters have flags0080 and
there is no ReturnValue. Export944's event-name global is `10583714`.
The body calls imported FindFunctionChecked (`10599A88`) and actual receiver
ProcessEvent virtual+10. Its empty base `Return Nothing` does not permit
skipping derived/state dispatch. Read current actor fields after synchronous
callbacks; do not snapshot XLevel/Rotation once at entry.

## Helper dependencies: exact identities, not implementation parity

The imported SafeNormal is already source-audited in
`ORIGINAL-ACTOR-LINE-TRACE.md`: retained `(x*x+y*y)+z*z`; a Double squared
length store while the retained sum is compared to Double1e-8; below threshold
returns exact +0; otherwise appSqrt of the stored Double, retained reciprocal
and Float component stores. It has no exact-unit fast path and no Double
root spill. Its precise existing scalar kernel may be reused only with that
same policy, not a generic Normalize routine.

The entire TwoWallAdjust body was captured/read to verify the call identity
and branch dependencies. It does not access this actor's fields or emit
events; it mutates the Delta reference. The branch decision uses new-normal
dot old-normal. For ordinary ordered dot<=0 it uses SafeNormal of their
cross product and projects Delta along that line, choosing sign against
DesiredDir. For dot>0 it projects along the new wall and zeroes a resulting
non-forward vector. Both paths use `(1-HitTime)`, but their Float spill points
and comparisons differ. Unordered comparisons follow explicit x87 status
bits, not an assumed common finite formula.

At this audit's original checkpoint, the TwoWallAdjust numeric adapter was
still absent. The follow-on `ORIGINAL-TWO-WALL-ADJUST.md` now pins its full
cross-product/store/association/factor/alias contract and supplies the detached
kernel. Its independent numerical validation is recorded there and in STATUS;
the high-level equations above are not an ARM numeric oracle or permission to
import a different engine's slide order.

## Root, exceptions and remaining first-Training seam

moveSmooth has local hit/vector scratch but no independent GMem mark,
hash/domain initialization, committed transform write or local undo list.
Its real MoveActor calls and HitWall may mutate actors, collision links,
relationships, script state, clocks and RNG. All nested movement/events must
join the enclosing first-Training VM/hash/work root. A runtime refusal after
call1 cannot undo just its local packet while publishing earlier movement.

EH stub `10421D30` selects function info `1044D8E0`; the observed unwind map
action words are zero. Catch fragments log/rethrow. No observed generated
movement/actor restoration action establishes retail exception atomicity.
Portable poisoning/rollback on exceptions and bounded memory/work/recursion
admission remain deliberate stronger safety requirements. Normal native
return false is not itself authorization to discard successful earlier moves.

Next complete integration still needs full current-XLevel MoveActor used by266/299,
verified TwoWallAdjust arithmetic, actual HitWall dispatch,
same-root nested native scratch and generation/rollback controls. The higher
encroachment forward/reverse calls ignore moveSmooth's result, as pinned in
`ORIGINAL-LEVEL-ENCROACHMENT.md`. None of these source contracts authorizes a
zero-only Move success, a direct pose-write replacement or another map's
integration as a substitute for first-Training acceptance.

## Detached complete protocol implementation (2026-10-19)

`native/quest_actor_move_smooth.h` now implements the complete bounded
moveSmooth call protocol and original first-slide numeric schedule. It owns
the by-value original Delta, constructs exactly one fresh hit record, reuses
that record through all attempts, preserves call1's exact signed native int,
and samples current Rotation/XLevel for every real movement attempt. It does
not collapse native results to Bool, reset the packet, clamp Time, suppress
zero slides or manufacture a native movement implementation.

The provider seam exposes each Rotation channel and native XLevel read. The
source load order is R0/Level/R1/R2/Level for call1,
Level/R0/R1/R2/Level for call2, and R0/Level/R1/R2 for call3. Calls1/2 retain
the first Level's virtual-dispatch identity and the second Level as receiver;
call3 uses one Level for both. There is no original synchronous callback
between these raw loads. A real adapter must authenticate the actual native
Level dispatch class and generation, not treat detached integer tokens as
native pointers or silently redirect dispatch to another Level. All four
MoveActor options stay literal0.

First-slide calculation keeps the X projection and subtraction retained,
stores/reloads Y/Z projections/residuals, retains (1-Time) and slideX through
the forward gate, and passes separately Float-stored XYZ to call2. It invokes
the existing exact Core.SafeNormal(originalDelta) only after that gate passes.
HitWall receives copied raw Normal/Actor parameters (including Actor=None).
The helper then reads the same hit record's current Normal/Time and uses the
full TwoWallAdjust adapter before sampling the third move. No fourth move or
postvalidation of the unused final hit packet is introduced.

FCOMP/C0 control is preserved for Time: ordered >=1 returns immediately,
including positive infinity; unordered continues. An unordered second Time
can reach HitWall and be repaired by the enclosing synchronous provider before
TwoWallAdjust reads it numerically. Active unsafe arithmetic still refuses.
Delta/Normal are not eagerly checked on a first clear-return path. Negative
finite Time remains valid; no [0,1] clamp is added. The portable numeric domain
requires finite active operands and finite required Float stores; unavailable
active providers, null actual XLevel, unsafe numeric operations or shared work
failure throw. Inactive providers are not required.

This is a detached protocol, not a publishing runtime adapter. Its supplied
MoveActor provider must implement the COMPLETE original movement operation,
and its HitWall provider must supply actual override/event dispatch. Both must
join the caller's existing native/hash/VM root and work/memory/generation
guards. The kernel propagates refusals; it cannot atomically restore external
provider side effects by itself. Normal native return0 is not a rollback
signal. No script native266/299 is enabled by including this header in the
runtime build. Actual first-Training startup, higher encroachment/pushing,
support/falling, live destruction and surviving usable loot remain unfinished.

## Detached numeric/protocol verification (2026-10-19)

`desktop/quest_actor_move_smooth_test.cpp` passes79005 checks/5968 explicit
refusals. Its separate hardware-x87 projection protocol compares36100
successful first-slide outputs and forward gates (17560 forward/18540
rejected). It does not use production dyadic/projection/trace helpers to
predict those values. The cohort includes broad finite exponents, subnormals,
signed zeros, nonunit normals, Time outside[0,1] and raw-word boundary grids.
Three explicit retained-X cancellation fixtures first demonstrate a different
decision from the deliberately wrong Float-reloaded-X gate, then verify the
production wrapper actually executes or omits MoveActor2 accordingly.

Twenty-four protocol controls verify exact first integer return, same live
hit-record address/untouched fields, owned original Delta despite callback
changes to the caller's input, all field-read transcripts, distinct dispatch/
receiver Level tokens, copied HitWall parameters/Actor=None, second unordered
Time repaired by the event, postevent Normal/Time, zero-distance attempts2/3
and an ignored final unsafe packet. Both null dispatch-Level and separately
reread receiver-Level cases refuse at their active boundary. Eighteen injected
active-provider prefixes stop at the exact refused read/callback.

Three explicit MOCK caller-root tests verify propagated errors can poison and
roll back coupled provider effects, normal native zero does not imply rollback,
and nested calls cannot reset the caller's shared work allowance. They do not
supply or prove the actual runtime's full MoveActor transaction. Strict ARM64
test syntax passes; on non-x86 hosts the independent hardware oracle is
explicitly skipped while portable protocol controls remain. Host/Android
compilation/link and full coherent regressions are recorded in STATUS. No
original commercial code was executed and no gameplay native was enabled.
Evidence: `artifacts/training-smooth-*20261019.log`.
