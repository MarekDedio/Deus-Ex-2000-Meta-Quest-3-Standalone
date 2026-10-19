# Original AActor.TwoWallAdjust numeric and alias contract

Static audit and detached numeric implementation, 2026-10-19. Scope is the
push/slide dependency required by the first `00_Training` path. No commercial
DLL/EXE was loaded or executed; no other map was loaded or integrated. This
does not establish MoveActor, moveSmooth, live encroachment or crate physics.

Owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Owned Core.dll SHA256:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.

## Complete source and public adapter

Export1696 `AActor::TwoWallAdjust(FVector&,FVector&,FVector&,FVector&,float)`
has thunk `10301C8A` to `10323770`. The complete normal body has returns at
`103238C8` and `103239A0`, both `ret 14h`; following catch fragments are
separate. All instructions through both returns were read. Reproducible
bounded evidence is
`artifacts/training-move-smooth-20261019/twowall-body.asm` and
`twowall-thunk.asm`. The earlier artifact also contains later padding/a
different function: this contract ends at `103239A0`.

Native stack arguments are DesiredDir reference `+08`, Delta reference `+0C`,
HitNormal reference `+10`, OldHitNormal reference `+14`, HitTime Float value
`+18`. The actor/this object is not read or mutated by the normal body.
Only the Delta-referenced object is written. The first, third and fourth
references can still observe these writes if they alias Delta.

`native/quest_two_wall_adjust.h` supplies:

```
void QuestVr::AdjustOriginalActorTwoWalls(
    const PortableModelVec3& desired, PortableModelVec3& delta,
    const PortableModelVec3& hitNormal, const PortableModelVec3& oldNormal,
    float hitTime);
```

Read-only C++ views on the other arguments do not assert independent storage.
All15 exact-reference alias partitions of the four Vector arguments are in
the supported domain. No actor identity, geometry selection, hash, event,
normal normalization, Time clamp or movement publication is added.

## Constants and arithmetic conventions

The original zero Float at `10426D54` has bits `00000000`; Double1 at
`104298A0` has bits `3FF0000000000000`; Float-1 at `104298F0` has bits
`BF800000`. The constant dumps and complete Core.SafeNormal evidence are in
the same ignored artifact directory.

Below E means one original retained x87 PC64 operation; F means an explicit
binary32 store/reload. Products/additions/subtractions execute in the written
order with round-to-nearest-even, not host-double/vector reassociation.
The implementation reuses existing ActorLineTraceDetail dyadics and its
source-audited Core.SafeNormal path, not platform-dependent long double.

Let N = HitNormal, O = OldHitNormal, D = current Delta, W = DesiredDir, T =
the by-value HitTime. The first test at `10323793..7BD` is:

```
normalDot = E(E(N.y*O.y + O.z*N.z) + N.x*O.x)
```

Ordered `normalDot > 0` takes the face branch. Ordered <=0 takes the corner
branch. Retail's FCOM/test AH41 also takes the corner branch on unordered
data; the portable active-nonfinite refusal below is explicit stronger policy.
There is no normalization or finite/length test on either incoming normal.

## Corner branch: normalized crease and retained factor

`103237C3..7F6` computes the cross product N x O in Z,Y,X evaluation order,
then stores the three results to Float in X,Y,Z order:

```
crossZ = E(N.x*O.y - N.y*O.x)
crossY = E(O.x*N.z - N.x*O.z)
crossX = E(N.y*O.z - N.z*O.y)
cross  = {F(crossX),F(crossY),F(crossZ)}
axis   = Core.SafeNormal(cross)
```

Core.SafeNormal (`10599B80` import; Core thunk `101031C0`, complete body
`10144BE0..10144C5E`) is already pinned by `ORIGINAL-ACTOR-LINE-TRACE.md`.
It computes retained `(x*x+y*y)+z*z`, FST-stores a Double squared length,
compares the still-retained sum against Double1e-8, returns exact +0 below
threshold, else uses appSqrt of that stored Double, retains reciprocal root,
and Float-stores each scaled component. There is no exact-unit shortcut or
Double root spill. Parallel/opposed input normals are not separately rejected.

At `10323815..835`:

```
projection = E(E(axis.z*D.z + axis.y*D.y) + axis.x*D.x)
remaining  = E(Double1-T)              // NO Float factor store
scale      = E(projection*remaining)   // NO Float scale store
```

`10323835..85B` writes the staged Delta using the exact temporary/store order:
compute/store F(axis.x*scale); compute retained axis.y*scale; write D.x;
store Y; compute retained axis.z*scale; write D.y; store Z; write D.z.
Axis and scale are local; no original normal/old-Delta read occurs during
these writes. Do not multiply by a Float(1-T) factor in this branch.

AFTER all three writes, `10323863..882` reads DesiredDir and the now-written
Delta for retained `(W.z*D.z + W.y*D.y) + W.x*D.x`. If ordered <0, negate
Delta by multiplying each component by Float-1 with Float stores. Read all
three old Delta components before writing their negated values. Ordered zero
does NOT negate or clear the corner result. Retail unordered also takes the
negation path; the portable helper refuses active nonfinite data instead.

## Face branch: two projection spills and a stored factor

At `103238CE..8E2`, the projection's association is DIFFERENT:

```
projection = E(E(N.y*D.y + D.x*N.x) + D.z*N.z)
projectZ   = F(projection*N.z)
projectY   = F(projection*N.y)
projectX   = E(projection*N.x)         // retained through X subtraction
residualZ  = F(D.z-projectZ)
residualY  = F(D.y-projectY)
residualX  = F(E(D.x-projectX))
remaining  = F(E(Double1-T))           // genuine Float store at1032391A
```

The Z/Y projection products spill before subtraction; X's does not. ALL
three residuals spill before multiplication by the stored remaining factor.
`1032391D..946` evaluates scaled residual Z,Y,X first; stores X then Y;
writes D.x; stores Z; writes D.y then D.z. No original-reference read occurs
between those writes.

Only then, `1032394C..96B` computes the SAME postwrite DesiredDir dot order
`E(E(W.z*D.z + W.y*D.y) + W.x*D.x)`. Ordered >0 retains the result. Ordered
<=0 writes exact (+0,+0,+0) to Delta. Retail unordered also clears; portable
nonfinite refusal remains distinct. Thus equality differs from the corner
branch: the face branch requires strictly forward movement.

## Aliases: postwrite DesiredDir is not the entry snapshot

There is no restrict/noalias declaration, identity test or early copy of W in
the original body. Its DesiredDir loads occur only AFTER the three Delta
stores. Therefore W == D reads the NEW Delta, not its entry value. This
remains true when the same alias group additionally includes N or O.
If W aliases N or O but not D, it still reads that unchanged separate object.
N == D, O == D, N == O, all-distinct and all-identical arguments otherwise
follow the same native schedule; no normal is re-read after Delta output
starts. HitTime is already a by-value Float and does not become live aliased
component storage when the caller supplied a component expression.

The adapter maps the four argument addresses to at most four fixed local
Vector slots, sharing one staged slot for each exact identity. Arithmetic and
component writes run against those remapped references. Its postwrite gate
therefore naturally sees updated aliases. Only after complete success does
the adapter assign the staged Delta back to the original Delta object; all
other original objects remain unchanged except by natural exact aliasing.
Making four independent copies, or retaining the original W reference while
staging only D, would both produce wrong alias behavior.

The C++ supported memory domain is distinct valid Vector objects or exact
shared-object references. Unequal starting addresses whose Vector byte ranges
overlap are explicitly refused; this is a structural portability policy,
not a claim that retail inspected address ranges. It does not narrow any of
the15 whole-reference alias partitions.

## Portable failure and integration boundary

Only numerically ACTIVE Float loads are checked finite. In particular W is
not eagerly validated at entry: initial normal/cross/store/normalization/
projection work can refuse before the original first DesiredDir load. Raw
staging copies do not numerically validate inactive words. HitTime accepts
any finite value, including outside [0,1], and normals need not be unit or
positive. Every original required Float store is retained, including late
stores that could overflow after earlier staged Delta components changed.

Any nonfinite active input, binary32 overflow, unsafe overlap or shared
arithmetic-domain error throws with ALL original caller objects unchanged.
This atomic refusal is deliberate portable safety, not the original partial
write/EH behavior. The original catch tail only logs/rethrows; it supplies no
Vector restoration guarantee. An enclosing movement coordinator must still
poison/roll back its complete native/VM root on refusal. This helper's work is
fixed and local; there is no new independent resettable traversal allowance.

The header is a detached complete numeric implementation. Its presence does
not implement the caller's actual MoveActor/hash, HitWall, touching, gravity or
first-Training startup; those remain separately integrated and verified
boundaries.

## Independent numeric validation (2026-10-19)

The separate `desktop/quest_two_wall_adjust_test.cpp` and test-only
`quest_two_wall_adjust_oracle.inl` pass64827 checks/3544 explicit refusals.
The oracle transcribes the original hardware-x87 PC64 sequence independently:
it uses no production dyadic, normalization, projection or trace arithmetic.
It compares28853 successful packet outputs, including24000 alias packets from
1600 seeds across ALL15 reference partitions.4096 additional broad-exponent
queries include active overflow refusals as well as successful packets. Every
refusal must preserve all original backing-slot bits, even after the original
numeric protocol would have written part of Delta.

The successful oracle census is11857 corner/16996 face,773 SafeNormal-zero/
11084 normalized axes,4303 sign flips and6131 face zeroings. Deliberate
threshold/exponent controls total1462. Three hand-derived retained-gate
fixtures reject a deliberately wrong Float-spilled comparison; a separate
factor cohort has791 observable wrong-spill witnesses. Signed zeros, subnormal
products, cancellation, nonunit normals, Time outside[0,1], postwrite
Desired==Delta and late Desired failure are explicit controls. Unreferenced
backing slots can contain23 poisoned raw entries without eager validation.

Both optimized direct GCC and the coherent CMake helper target pass the same
counts. Strict ARM64 test syntax passes; on non-x86 platforms the test's
hardware oracle is explicitly skipped and only portable deterministic/refusal
controls remain. This is bounded numerical evidence, not native runtime
movement or a playable-physics acceptance claim. Full platform/host regression
results are recorded in STATUS. Evidence:
`artifacts/training-two-wall-*20261019.log`.
