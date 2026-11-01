# Original brush dependency and flush helpers (2026-10-31)

Continuation2026-11-01: typed helpers and the actual host noninitial Update
provider now pass generated and first-Training same-root rollback gates; see
ORIGINAL-BRUSH-NONINITIAL-UPDATE.md. The static contract below is retained as
source evidence. Renderer consumption, earlier-root COW, startup publication,
destructor/travel/archive and crates/loot/gravity remain unproven.

Read-only static follow-up while the fresh-constructor regression runs. This
document is not implementation, a passed update test, travel/destructor proof,
startup completion, crates or an APK. No original DLL/EXE was loaded/executed.

Authority is owned `D:/Steam/steamapps/common/Deus Ex/System/Engine.dll`, freshly
SHA256 verified as
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Addresses are native PE32 virtual addresses/offsets, not portable layouts.
Bounded external objdump read:

```powershell
& C:/ProgramData/mingw64/mingw64/bin/objdump.exe -d -Mintel --start-address=0x1037c4f0 --stop-address=0x1037c85b 'D:/Steam/steamapps/common/Deus Ex/System/Engine.dll'
```

## Dependency propagation:103032AB ->1037C4F0

The selected node index is the sole argument. `1037C518..C53D` reacquires
the actual world node and returns immediately when NodeFlags bit0x20 is set.
Otherwise it sets that bit BEFORE recursion and owner/actor work. It then
recurses in **Front(+24), Back(+20), PlaneChild(+28)** order, skipping -1.
Child fields are read from the same live node between recursive returns.
This helper does not follow node+30 (the distinct mover-root chain).

After descendants, `1037C570..C589` fetches the owner pointer from tracker
node-owner entry[index-originalNodes]. A null owner or already-assimilated
owner returns. An unassimilated owner receives bAssimilated=true, then an
8-byte stack pending entry `(owner,previousHead)` is allocated/prepended to
tracker+3C. Finally current Location is copied to OldPos and current Rotation
to OldRot (`1037C614..C64C`). This is postorder enqueue, not preorder owner
collection or a sorted/deduplicated independent set. Its visited node bit and
actor assimilation bit are different guards, both owned by the outer update.

The helper unconditionally indexes dynamic owner storage after descendants.
The intended selected dynamic subtree therefore needs authenticated owner/
index authority. It is not permission to subtract originalNodes from a static
or free node and index arbitrary memory. A portable corrupt-topology refusal
must restore flag/assimilation/pose/pending writes under the same caller root.

## Full Update pending scheduling (static continuation 2026-11-01)

A separate bounded read of Update establishes its pass scheduling. At
`1037A613`, pass1 captures the current pending head once. For each entry it
walks that mover's OLD root chain using node+30 and invokes dependency on each
node (`1037A61A..A648`). It advances the original entry's next pointer at
`1037A64D`, not the possibly changed tracker head. Entries prepended during
dependency are therefore NOT revisited by pass1's outer pending traversal.
The recursion itself discovers the selected node descendants; do not invent
a fixed-point worklist or scan newly queued movers' disconnected roots.

Pass2 reloads the expanded head at `1037A652`, flushes each pending mover and
advances entry.next (`1037A659..A666`). Pass3 reloads that SAME final head at
`1037A668` and rebuilds each mover. Its saved current entry.next is read at
`1037A9F6`; it does not restart from tracker head. For initial list A->E,
dependency discoveries B,C,D while processing A prepend D->C->B->A->E.
Pass1 next visits E. If E discovers F, passes2 and3 both run F,D,C,B,A,E.

The full/null Update walks live Actor slots ascending and enqueues selected
nonnull Brush/ABrush/nonstatic actors without filtering unchanged poses.
Update's selection does not repeat Setup's checked Mover cast. The supplied-
Mover path compares Location against OldPos x/y/z (`1037A53A..A571`), then
Rotation against OldRot (`1037A573..A5A1`), BEFORE the bAssimilated check. An
unchanged supplied mover is skipped; a changed already-assimilated one is
also skipped. Original x87 comparisons inspect C3 equality, so ordinary C++
float != is not identical for unordered NaN. Retain finite-value admission.

Each enqueue sets assimilation, allocates a stable pending entry, prepends it,
then copies Location/Rotation to OldPos/OldRot. The outer stack marker retains
entries through all three passes; per-mover transformation/filter scratch has
its own marker. Stable typed indices/links must survive pending-storage growth.
Final bit0x20 clearing walks only each mover's NEW rebuilt root chain; old freed
node flags/geometry remain untouched. This is static evidence, not implemented
noninitial Update or passing runtime/rollback tests. The fresh -1-root short
path must not substitute for any of these selected operations.

## Flush:10303F03 ->1037C6D0

Input is the actual mover. The helper reads mover.Brush+178 and its root+F8.
Root -1 takes the short branch, writes Brush.Root=-1 again and returns.
Otherwise it walks the mover-root chain using each node's **+30** word.
For each current dynamic node:

1. Read recorded parent index from its node-owner pair and reacquire that
   parent in the world Model. Compare parent.Front to current first; on a
   match set Front=-1. Otherwise compare Back and clear it on a match;
   otherwise compare PlaneChild and clear it on a match. The exact ordered
   branch `1037C71C..C735` clears at most ONE matching field, not every equal
   index. No match simply leaves all three links unchanged.
2. Read current VertPool and unsigned-byte NumVertices. For each vertex in
   **ascending** run order, read its PointIndex, replace point bookkeeping at
   `[PointIndex-postSetupPoints]` with current freePoint head, then set freePoint
   head to that absolute PointIndex (`1037C738..C783`). This removes the typed
   active point owner and creates a LIFO free list; geometry XYZ is untouched.
3. Clear exactly NumVertices vertex-owner entries in ascending order, beginning
   at VertPool-originalVerts (`1037C78C..C7BB`). Do not clear FVert bytes, reset
   PointIndex/Side or shrink either native Model array.
4. Zero both current node's owner and parent words (`1037C7C0..C7CF`). The
   node geometry/flags/zone masks/children/bound words are not zeroed here.
5. Read current node+30 as the next root-chain index and repeat. Only after
   the chain terminates does the helper write mover.Brush.Root=-1.

The helper does not reset allocation cursors, shrink Nodes/Verts/Points,
recalculate ancestor masks, reparent surviving children, or repair arbitrary
malformed graphs. Those changes must not be invented as a cleanup shortcut.
The separate dependency pass exists before this flush pass. Array/source
ownership and root/parent correspondence must remain authentic, with no
truncated pointer bookkeeping or permanently retained historical owners.

## Implementation implications still open

Current fresh-constructor code legitimately reaches only the two helpers'
empty old-root cases. Implementing later updates requires actual typed
dependency recursion/visited flags, the exact pending scheduling above, ordered unlink/
free transitions, Model/Polys/tracker copy-on-write and beforeimage edge roots,
cache writes and same-root rollback. A freed dynamic record must stop being
readable through the active view even though its old geometry bytes remain.
The original destructor/travel/archive sequence is a separate service, not
proven by flushing one mover or discarding the native tracker owner.

None of this authorizes a production no-op, traversal into free/static owner
slots, unauthenticated validation bypass or advancing later startup guards.
