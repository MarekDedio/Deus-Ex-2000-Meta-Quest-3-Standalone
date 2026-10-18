# Original FCollisionHash.ActorEncroachmentCheck

Read-only static audit, 2026-10-18, limited to the prerequisites for the first
`00_Training` movement path. No commercial DLL/EXE was loaded or executed.
Engine.dll SHA256 is
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
This is a contract and integration proposal, not an implemented encroachment
adapter, complete Move266/SetRotation299, gravity, or playable crate acceptance.

## Identity and bounded evidence

Export ordinal794 identifies
`FCollisionHash::ActorEncroachmentCheck(FMemStack&,AActor*,FVector,FRotator,DWORD)`.
Thunk `10301497` jumps to `10358D50`; the complete normal body ends at
`10359051` (`ret 24`). The hash vtable at `1042A044` has this thunk at `+1C`.
Addresses are preferred PE32 virtual addresses, not portable struct offsets.

Ignored, reproducible static evidence is in
`artifacts/training-actor-encroachment-20261018/`:

- `actor-encroachment-body.asm`: entire body, including following unwind/catch
  fragments through `10359083`.
- `actor-encroachment-thunk.asm`: exact exported jump.
- `brush-predicates.asm`: original IsBrush/IsStaticBrush/IsMovingBrush bodies.
- `hash-vtable-grid-constants.asm`: exact hash slot and grid constants.
- `primitive-vtable-point-slot.asm` and `mesh-vtable-point-slot.asm`: concrete
  PointCheck virtual dispatch bindings.
- `exception-handler.asm` and `exception-function-info.asm`: bounded SEH/C++
  exception metadata; not evidence of a temporary-pose cleanup destructor.
- `export-import-identities.log`: export identities, relevant ordered IAT
  operands, and unchanged owned Engine.dll hash.

Use NDK27 `llvm-objdump --disassemble --no-show-raw-insn
--start-address=0x10358d50 --stop-address=0x10359084 Engine.dll` to reproduce
the body. For bounded raw data, MinGW `objdump -s -j .rdata` honors the
specified start/stop addresses. Nearest unrelated disassembler symbol labels
do not identify these bodies; exported thunk bytes and vtable words do.

## Entry, temporary pose, and source bounds

The stack arguments are MemStack `+08`, source `+0C`, Location `+10..18`,
Rotation `+1C..24`, final DWORD `+28`. The source must be nonnull; the original
asserts it at `10358D71..97`. The final DWORD is **not read anywhere in this
body**. The incoming MemStack reference at `+08` is likewise not read: hit
allocation uses the imported global GMem below. The DWORD is not an observed TraceFlags, collision-block mask, or primitive
flags filter. Do not add behavior based on its exported type alone.

At `10358D9A..10358DED`, the source's actual Location (`+114..11C`) and
Rotation (`+120..128`) are replaced with the supplied Float/int bit patterns.
The old values are preserved in the by-value argument stack slots. There is
no arithmetic, transform interpolation, RemoveActor, AddActor, or ColLocation
write here. The source remains in its preexisting hash buckets while queried
at its proposed pose.

Next, `GetActorExtent(source,...)` executes at `10358E0F`. It selects the
source's actual Brush, else Mesh, else XLevel.Engine.Cylinder and calls its
GetCollisionBoundingBox(source). Its existing exact bounds/grid contract is
in `ORIGINAL-COLLISION-HASH.md`: a brush uses its proposed owner transform,
not a candidate cylinder box. Bounds must execute while the temporary source
pose is authoritative to the provider.

Only **after** source bounds finish, `10358E14..23` increments the shared
32-bit process CollisionTag, with wrap and no actor-reset scan. This ordering
differs from ActorLineCheck, which increments before forming its sweep box.
Even an empty resulting range has incremented the tag before returning.

## Cells, candidate tags, and gates

The loops visit inclusive X outer, Y middle, Z inner (`10358E20..10359002`).
Hashing and encoded-cell construction are the same permutation XOR and
shifted-add operations already audited for the real hash. Each selected
bucket is traversed from its current head, in live Next order.

For each link, `10358E8B..EA2` first compares its actor's CollisionTag to the
**current shared global tag**, then compares its stored encoded cell to the
current cell. A tag-equal or wrong-cell link is skipped. A wrong-cell actor
is not tagged. At `10358EA9`, a qualifying actor receives the current tag
**before** packet construction or either gate below.

1. `10358EE6..10358F16` skips a candidate that is an original moving brush:
   candidate.Brush is nonnull, its genuine UClass ancestry reaches the
   exported `ABrush::PrivateStaticClass` at `10582A28`, and its authored
   bStatic bit is false. The inlined condition agrees with original
   IsMovingBrush `1034C450..1034C48F`. A nonnull Brush on an unrelated class
   is not enough; Mover leaf naming is not the test. Static brushes remain
   candidates. Native Actor `+28` bit1 is bStatic, not UObject.ObjectFlags.
2. `10358F1C..20` skips pointer-identical source itself, after the moving-brush
   gate. The source, like a skipped moving brush, has already been tagged.

There is no additional bCollideActors, bCollideWorld, block-flag, pending-delete,
hidden, Owner/Base, Pawn, or source-family filter in this function. Registration
history determines which actors have links; it must not be repaired by an
all-actors fallback or filtered render registry. In particular, this function
does exclude self at this precise gate; that is not permission to change
MultiLineCheck's source-anchor admission.

The candidate can be reached repeatedly if a nested operation changes the
shared global tag. Preserve the existing hash kernel's shared, live tag and
link semantics rather than a private query-local visited set.

## Fresh packet and the direction of PointCheck

For each cell/tag-qualified candidate, the original prepares all44 bytes at
`10358EB0..EDF`:

| Field | Initial value |
| --- | --- |
| Next, Actor, Location XYZ, Normal XYZ, Primitive | zero |
| Time | Float1 (`3F800000`) |
| Item | int-1 (`FFFFFFFF`) |

The zero Normal FVector is constructed by the exact Core import at
IAT `10599B28`. Time is **1**, unlike ActorLineCheck's Time0 seed. The packet
is prepared before the moving-brush/self gates; it has no script side effect.

At `10358F26..44`, the candidate supplies an extent of
`(candidate.CollisionRadius, candidate.CollisionRadius,
candidate.CollisionHeight)`. X is a raw radius word copy; Y and Z pass through
Float load/store. These operations matter if later support includes exceptional
Float encodings. At `10358F69..F92`, candidate.Location is sampled as the
PointCheck location. Do not substitute source dimensions/location.

The primitive being queried belongs to the **source**, selected live again at
`10358F3E..61`: source.Brush, else source.Mesh, else source.XLevel.Engine.Cylinder.
Its virtual `+54` is called at `10358FA0` as:

```
sourcePrimitive.PointCheck(freshHit, source,
                          candidate.Location,
                          (candidate.Radius, candidate.Radius, candidate.Height),
                          0)
```

This is not candidatePrimitive.PointCheck(sourceExtent), and not a line sweep
with identical endpoints. The flags argument is always literal0; the entry's
final DWORD is not forwarded. The concrete vtables prove UPrimitive and UMesh
share PointCheck thunk `10301848`; UModel uses `10301F5F`. A source Mesh therefore
does not require invented mesh-triangle overlap, but Model PointCheck remains
its distinct native implementation. The raw cylinder/Model PointCheck numerical
contracts are separate audits, not established by this dispatcher alone.

## Hit ownership, order, and normal restoration

On a nonzero/clear PointCheck return, append nothing. A zero/blocked return
executes `10358FA7..FD8`:

- Overwrite hit.Actor with the candidate, regardless of the provider's Actor.
- Overwrite hit.Primitive with None (the zero return register).
- Allocate/copy the complete44-byte packet, alignment8, through the actual
  imported global GMem/PushBytes operands `10599BD8`/`10599BD4`.
- Append through the current **tail** pointer, then move the tail to the new
  record's Next. This is not ActorLineCheck's prepend operation.

Therefore returned hit links preserve successful primitive encounter order.
The final tail.Next is zeroed at `1035903C`. No sorting or64-hit cap is present.
Location/Normal/Time/Item remain the provider's initialized-or-written fields;
Actor and Primitive are the explicit overrides above. In particular, a provider
that leaves Item untouched yields -1, not an invented map node.

After callback and possible hit copy, the current link's **live** Next is read
at `10358FE1`. Do not cache Next before provider invocation or restart at a
snapshot bucket head. Preserve existing storage-generation, work, cell/link,
hit-allocation and nested-root refusal guards.

At `10359007..35`, the old source Location and Rotation words are restored on
the normal completion path, unconditionally whether hits were found or not.
This precedes tail termination and return of the head. Neither the temporary
pose nor a copy to ColLocation survives a normal return. No original Remove/Add
or membership refresh accompanies this restoration.

That normal restoration is **not evidence of original exception safety**.
The observed function-info unwind entries have zero cleanup actions; the
following catch/unwind fragments at `10359054` and `10359068` do not restore
Location/Rotation. This bounded audit proves the normal path and those observed
handlers, not all possible OS/fault recovery. A portable explicit refusal must
restore the temporary pose and roll back all tags/hash/RNG/state in its same root;
it must not imitate native fault leakage or claim that atomicity is original.

## Proposed portable seam, not an implementation

Keep the existing `OriginalCollisionHashWorld` authoritative. A separate
encroachment-query provider struct avoids changing the meaning or positional
initialization of existing LineCheck providers. A suitable minimal contract is:

```
sourceBounds(source) -> exact OriginalCollisionHashBox
movingBrush(candidate) -> exact live Brush/ABrush/bStatic predicate
candidateCylinder(candidate) -> live Location plus radius/radius/height
sourcePoint(hit, source, candidateLocation, candidateExtent, flags0) -> clear
```

The kernel method takes actual source identity and the existing owned hash;
an enclosing runtime native-pose scope supplies the proposed source transform.
It bounds/captures source bounds first, increments the existing shared tag,
walks existing cells/links, tags before gates, calls these providers in the
observed order, overrides Actor/Primitive, appends hits, and retains its one
outer root until the runtime restores the pose and all remaining VM work ends.
There must be no second root, copied hash, private actor registry, or independent
query commit. Provider failures poison that root just as existing hash callbacks
do. All visited links and fixed/nonfixed work still need explicit admission.

The runtime scope must resolve the genuine current-generation source and exact
declared `Engine.Actor.Location` and `Engine.Actor.Rotation` schemas. The proposed
pose must be visible to the same authoritative declared reads used by bounds,
source PointCheck, owner coordinates, and supported nested providers. A private
argument passed only to one transformed-box helper is insufficient, and a
permanent raw transform write is wrong. Ordinary bytecode's active-hash write
guards must remain intact.

One safe design is a bounded, nested native-pose overlay scoped to these two
declared fields, read before ordinary live/frozen/default/authored storage only
for the matching actual actor. It must restore the previous overlay on normal
return and exceptions without allocating, preserve exact original bits, and
never serialize or mark the temporary pose as a committed script overlay. An
alternative uses precharged, noexcept-restorable owned property-map entries;
absence, frozen/default source, prior values and nesting must all be restored,
not merely the numerically equal final Float values. Either design needs
explicit reentrancy/late-failure tests before integration.

Do not temporarily Remove/Add the source or update native ColLocation merely to
satisfy existing mutation guards: that changes the original query and can lose
self-tag/candidate history. Do not permanently refresh the hash actor's live
Location to the proposed pose without equally scoped restoration. Only getters
that authentically observe the proposed raw pose should see it. The outer root
still owns all unrelated provider side effects and failure rollback.

Remaining acceptance work includes the real Model PointCheck provider, typed
source pose visibility/restoration, moving-brush predicate, candidate sampling,
forward hit-tail ordering, nested tag/link mutation and bounds/point/late-output
refusal tests. Higher ULevel.CheckEncroachment and its EncroachingOn/EncroachedBy/
Touch callbacks remain separate. This seam must not be advertised as completed
MoveActor, native startup, crate gravity/destruction/loot, or a headset build.
