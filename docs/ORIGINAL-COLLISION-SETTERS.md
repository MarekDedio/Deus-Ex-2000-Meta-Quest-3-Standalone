# Original Actor collision setters

Read-only static GOTY audit, 2026-10-18, supporting first Training startup and
physics work. This records source contracts, not passing runtime integration,
playable crates, falling, or an APK. No original commercial EXE/DLL was loaded
or executed. Original files under `D:/Steam/steamapps/common/Deus Ex/System`
had these unchanged SHA256 values before and after inspection:

| Source | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |

Addresses below are preferred PE32 image virtual addresses, not portable object
offsets. NDK27 llvm-objdump/llvm-readobj inspect bytes; the package inspector
and existing PowerShell property-chain reader inspect Engine.u data only.

## Identity and declared fields

| Operation | Exported thunk | Normal body |
| --- | --- | --- |
| Actor.execSetCollision262 | `10301393` | `103E1900..103E19AE` |
| Actor.SetCollision | `10301A5F` | `10369200..1036929F` |
| Actor.execSetCollisionSize283 | `10302491` | `103E19F0..103E1A66` |
| Actor.SetCollisionSize | `10303D64` | `10369320..103693AE` |

The exported `FCollisionHash` vtable at `1042A044` has actual slot+08
`10302027` (AddActor, body `103580B0`) and slot+0C `1030199C` (RemoveActor,
body `103583A0`). PE section-mapped word reads and named thunk exports establish
these identities independently of objdump's unrelated nearest-symbol heading.
See `ORIGINAL-COLLISION-HASH.md` for the separately audited hash operations.

Engine.u Actor.SetCollision export4576 is native262, flags`401`. Its three
BoolProperty parameters NewColActors, NewBlockActors and NewBlockPlayers have
dimension1 and flags`90` (optional). Actor.SetCollisionSize export4574 is
native283, flags`401`: NewRadius and NewHeight are FloatProperty, dimension1,
flags`80` (**required**); ReturnValue is BoolProperty, dimension1, flags`580`.

The actual Actor-owned scalar fields are:

| Field / export | Core property type | Flags |
| --- | --- | --- |
| CollisionRadius / 32 | FloatProperty | `23` |
| CollisionHeight / 39 | FloatProperty | `23` |
| bCollideActors / 1538 | BoolProperty | `23` |
| bCollideWorld / 204 | BoolProperty | `21` |
| bBlockActors / 2302 | BoolProperty | `21` |
| bBlockPlayers / 1109 | BoolProperty | `21` |

All have dimension1 and no referenced subtype. Flags are hexadecimal; export
indices and native numbers are decimal. Native Actor+1DC masks1/2/4/8 are
respectively bCollideActors/bCollideWorld/bBlockActors/bBlockPlayers; radius is
at+1D4, height+1D8, actual XLevel+AC, and Level.Hash+F8. These offsets identify
the audited source accesses, not a portable layout or reflected Level alias.

## SetCollision262

The exec wrapper does not seed all optional arguments together:

1. Read current bCollideActors and seed NewColActors, then evaluate argument1
   (`103E1911..103E1931`).
2. After that evaluation, read current bBlockActors and seed NewBlockActors,
   then evaluate argument2 (`103E1934..103E195D`).
3. After argument2, read current bBlockPlayers and seed NewBlockPlayers, then
   evaluate argument3 (`103E1960..103E1989`).
4. Finish the arguments and call native void SetCollision (`103E199F..103E19A4`).
   The wrapper does not write a return value.

Earlier argument expressions can change the later seeds. Capturing all three
defaults before evaluating any expression does not reproduce this ordering.

Native SetCollision performs:

1. If old bCollideActors is set, read actual XLevel.Hash and, when nonnull,
   call Hash.RemoveActor(self) **before changing fields** (`1036922A..10369246`).
2. Read the current flag word after removal, replacing exactly bits1/4/8
   (`10369249..10369271`):

   ```
   flags = (currentFlags & ~0x0D)
         | (NewColActors & 1)
         | ((NewBlockActors & 1) << 2)
         | ((NewBlockPlayers & 1) << 3)
   ```

   The native integer contract takes each input's **low bit**, not arbitrary
   nonzero normalization. Other bits, including bCollideWorld, are preserved.
3. If resulting bCollideActors is set, reacquire actual XLevel.Hash and, when
   nonnull, call Hash.AddActor(self) (`10369277..1036928C`).
4. Return void.

There is no equality fast path. A true-to-true call still removes and adds,
even if all requested values match, or only the blocking flags change. Native
XLevel/Hash are accessed only in the relevant colliding branches; there is no
unconditional hash dereference on the false-to-false path.

## SetCollisionSize283

The exec wrapper initializes a local radius buffer to Float+0 before evaluating
the first argument (`103E19F8..103E1A1A`), then initializes the height buffer
to Float+0 before evaluating the second (`103E1A22..103E1A3E`). These buffer
initializers do **not** make the source flags`80` required parameters optional.
It finishes arguments and calls native void SetCollisionSize at `103E1A54`.
After a successful native return, the wrapper writes Bool1 unconditionally at
`103E1A5F`; it does not return a fit/encroachment result.

Native SetCollisionSize performs:

1. If current bCollideActors is set, read actual XLevel.Hash and, when nonnull,
   call Hash.RemoveActor(self) (`10369349..10369369`).
2. **Re-read bCollideActors after removal**, before either size store
   (`1036936C`). Capture the supplied radius/height words, store radius
   (`10369378`), test the sampled collision bit (`1036937E`), then store height
   (`10369380`).
3. Using that post-removal sample, if colliding, reacquire actual XLevel.Hash
   and, when nonnull, call Hash.AddActor(self) (`10369386..1036939B`).
4. Return void.

There is no unchanged-size shortcut. There is also no native nonnegative-size,
finite-number, fit, overlap or encroachment test in this body. A portable refusal
of unsafe numeric inputs must be labelled a supported-domain policy, not a
recovered retail false return. The original stores the supplied float words.

## Integration boundary

Neither normal native setter body directly calls Touch/UnTouch, SetBase, zone
refresh, overlap events or render/mesh-cache invalidation. Neither has a
bStatic, bDeleteMe or begun-play eligibility gate. Hash Add/Remove still retain
their own separately audited behavior; do not invent setter-level callbacks or
replace them with the pinned-engine overlap registry.

A portable adapter must authenticate the actual declared fields and native
level/hash, preserve Remove-before-field-write/Add-after ordering (including
unchanged calls), and journal fields, hash state and any throwing work in the
same outer VM transaction. That transactional rollback is a portable safety
contract, not a claim that the original DLL implements modern atomic rollback.
This audit alone does not prove startup, Move266, support/falling, Destroy,
loot, save/hash reconstruction, or live first-Training crate acceptance.

## Retained bounded evidence

Ignored logs under
`artifacts/training-collision-setters-20261018/`:

- `native-setcollision-body.asm`
- `native-setcollisionsize-body.asm`
- `exec-setcollision-wrapper.asm`
- `exec-setcollisionsize-wrapper.asm`
- `original-parameter-schemas.log`
- `original-actor-field-schemas.log`
- `vtable-exports-thunks.log`
- `source-hashes-after.log`

The first six logs contain only the bounded bodies or selected source schemas;
the vtable log records mapped words, named export RVAs and the six relevant
thunk jumps. Before hashes were also inspected directly and match the retained
after log. No original file bytes were changed.
