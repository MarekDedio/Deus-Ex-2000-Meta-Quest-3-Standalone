# Original AActor.IsOverlapping

Static source audit, detached predicate and typed runtime adapter, 2026-10-19,
scoped to the overlap dependency required for first `00_Training`
movement/touching. No commercial DLL/EXE was loaded or executed. Runtime checks
use only that first map in rollback-only diagnostics. This helper does not
establish playable Training, publishing native touching relationships, Move
or crate gravity.

Owned Engine.dll SHA256 remains
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.

## Identity and complete source boundary

Export1257 is `AActor::IsOverlapping(AActor const*) const`, thunk `103029BE`
to `10369430`. The complete normal body ends with true `1036951B` or false
`10369521`, both `ret 4`. This/ECX is source S; its only stack argument is
other O. Preferred PE32 addresses are evidence locations, not portable offsets.

Full statically disassembled source and its thunk were already captured in
the ignored `artifacts/training-level-encroachment-20261019/` files
`touching-body.asm`, `thunk-overlap.asm`, and
`engine-export-import-identities.log`. All instructions through both returns
were reread. The later BeginTouch body is separate and is not folded into
this predicate. Bounded reproduction:

```
llvm-objdump --disassemble --x86-asm-syntax=intel --no-show-raw-insn \
  --start-address=0x10369430 --stop-address=0x10369522 Engine.dll
```

`native/quest_actor_overlap_original.h` exposes:

```
struct OriginalActorOverlapQueries {
    function<bool(uintptr_t)> isBrush;
    function<uintptr_t(uintptr_t)> level;
    function<float(uintptr_t,unsigned)> location;
    function<float(uintptr_t)> radius, height;
    function<void()> work; // optional, SAME enclosing caller allowance
};
bool QuestVr::IsOriginalActorOverlapping(source, other, queries);
```

The real provider must authenticate the current native object/declared fields.
The kernel receives opaque object identities and does not create a geometry,
renderer, hash or collision-registration registry.

## Ordered gates and exact live reads

`10369431..462` first checks S.IsBrush, then `1036946C..49D` checks O.IsBrush.
The original brush predicate means raw Brush pointer nonnull AND actual
Engine.Brush ancestry (`ABrush::PrivateStaticClass=10582A28`). It does not
test bStatic/bMovable or select, validate or dereference the Brush asset's
Model payload. A nonnull Brush on an unrelated class is not sufficient.
Either brush returns false before Level or any geometry field is read.

Then `1036949F..4A5` returns false if O is pointer-identical to the CURRENT
S.Level (`+A8`, not XLevel or Base). No other actor Level is read. Otherwise
the exact provider sequence is:

1. S.Location.X, O.Location.X, retained subtraction;
2. S.Location.Y, O.Location.Y, retained subtraction;
3. O.CollisionRadius, S.CollisionRadius, retained addition;
4. retained Y-difference square, then X-difference square, then Y+X sum;
5. retained radius-sum square, strict radial comparison;
6. only when radial overlap succeeds: S.Location.Z, O.Location.Z, subtraction;
7. O.CollisionHeight, S.CollisionHeight, retained addition;
8. retained Z-difference square, then height-sum square, strict cap comparison.

Native fields are Location `+114/+118/+11C`, CollisionRadius `+1D4`,
CollisionHeight `+1D8`. Each read remains live; do not sample a complete
actor-cylinder aggregate before these gates. Radius is other-first; height
is other-first; coordinates are source-first. XY-clear must not inspect Z,
height, or require their inactive provider functions.

There is NO self, bCollideActors, bCollideWorld, block flag, delete/pending-kill,
Owner/Base, same-XLevel, Pawn/Player, DrawType or hidden filter. A nonbrush
can overlap itself when its signed dimension sums pass both strict tests.
Higher native movement/touch callers retain their own separate exclusions.

## PC64 arithmetic and signed dimensions

The whole numeric body `103694A7..513` uses retained x87 PC64 values. There
are no Float/Double stores of differences, sums, squared products or sums of
squares. With E meaning one established PC64 operation:

```
dx = E(S.x-O.x); dy = E(S.y-O.y)
r  = E(O.radius+S.radius)
radialSquared = E(E(dy*dy)+E(dx*dx))
if !(E(r*r) > radialSquared): return false
dz = E(S.z-O.z)
h  = E(O.height+S.height)
return E(h*h) > E(dz*dz)
```

The exact height branch evaluates dz-squared before height-squared. Both
comparisons require STRICTLY greater; radial or cap equality is false.
Native FCOMPP/FNSTSW/test AH41 also returns false on unordered input. The
portable active-finite refusal is explicit stronger policy below, not a claim
that retail validated the inputs.

Finite negative radii/heights are authentic admitted data: native squares
their **signed sum**. It neither individually clamps/rejects negative values
nor sums their absolute magnitudes. Opposite dimensions can cancel to zero;
two negative dimensions can produce the same squared coverage as positives.
Do not reuse the separate UPrimitive point/line Dimension checker, which has
a different positive-dimension safety policy. There is no binary32 store
that would justify rejecting a retained finite sum exceeding FLT_MAX, and
subnormal squared values must not be rounded to binary32 zero at comparison.

The implementation reuses existing checked finite loads and integer-dyadic
PC64 Add/Subtract/Multiply/Compare primitives. No host-double vector math,
FABS shortcut, square-root, guessed collision epsilon or inverse transform
is introduced. Signed zero compares numerically equal and remains strict.

## Lazy admission, failures and caller ownership

Source0 is explicitly refused. A source brush returns false before Other is
dereferenced, so Other0 is allowed on that inactive branch only. After a
false source-brush result, Other0 is refused before its brush provider. Real
runtime diagnostic admission can conservatively require actual nonnull actors;
the standalone provider kernel preserves the original lazy boundary.

A missing provider throws only when that particular field is reached. Once
present, optional work() runs immediately BEFORE each provider invocation;
it may throw and prevent that provider. The same work function/ledger belongs
to the enclosing root, not a resettable per-query allowance. Full numeric
overlap uses13 active provider calls; radial-clear uses9; brush/Level gates
use1/2/3. Provider failures propagate, never fabricate false overlap.

Only active Float loads are finite-checked. XY-clear admits inactive NaN Z or
heights, and a brush/Level gate admits every inactive geometry word/provider.
The kernel writes no actor, hit packet or relationship state, so there is no
partial numeric output to publish. Provider/work side effects are owned by
the caller: any refusal must poison/roll back its complete native/VM root,
not undo only an invented local collision record.

## Independent tests and integration boundary

`desktop/quest_actor_overlap_original_test.cpp` contains independently
specified gate/read/work transcripts, inactive/missing-provider controls,
all active nonfinite fields, signed-dimension and strict-contact cases,
every full provider/work failure prefix, live later-field mutation, and a
shared-ledger repeated-call refusal. Retained subtraction/sum/subnormal and
extreme-finite witnesses prevent accidental Float-spilled geometry policy.

On GCC x86/x64 it also compares with an independent actual hardware-x87
PC64 oracle: a finite signed small grid plus100,000 full-exponent finite
packets and25,000 coincident-pose cohorts. The oracle calls no production
numeric helper. Non-x87 targets retain deterministic/provider tests and
explicitly report the missing hardware differential. Test outcomes are
reported separately from authored coverage.

The actual runtime adapter in `native/portable_retail_actor_touch.inl` reads
raw declared Brush presence plus genuine Engine.Brush ancestry, declared Level
(not native XLevel), current per-channel native Location and signed Float
dimensions. It does not substitute cylinder-positive-dimension admission or
dereference an inactive Brush asset. Calls share the enclosing lifecycle work
ledger. Diagnostic actor-pair admission requires two current owned actors,
which is stronger portable policy than the kernel's lazy unused-Other branch.
The preview returns only the boolean and command index, never actor mutations
or a second collision registry. Full Move/moveSmooth providers, begun-world
callback execution and first-Training gameplay acceptance remain separate.

The executed optimized host suite passes131723 checks/92 explicit refusals,
five lazy transcripts and26 active failure prefixes. Its131561 independent
hardware-x87 comparisons include125000 wide-finite packets, with44436 overlap
and87125 clear results. Strict ARM64 runtime/test syntax and Android native
compilation/link pass. The actual first-map diagnostic adds14 overlap queries
(five true/nine false), with source/peer brush, declared Level, self and bounded
integer-geometry expectations independent of this numeric helper. It does not
claim broad actual-map floating-point numerical proof; that belongs to the
independent hardware oracle. Evidence:
`artifacts/training-overlap-unit-20261019.log` and
`artifacts/training-touch-owned-final-20261019.log`.
