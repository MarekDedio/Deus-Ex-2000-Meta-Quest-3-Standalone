# Original ordinary-game Level cleanup boundary

First-Training integration,2026-10-25. This implements the reached ordinary
`CleanupDestroyed(0)` prefix and below-threshold return in the enclosing Level
frame root. It is not forced/editor cleanup, high-count scalar teardown,
deferred-queue save/rebind support or headset acceptance.

## Independently checked source

Owned Engine.dll SHA256:
`0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4`.
Core.dll:
`8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4`.
Only external objdump/static bytes and existing source were read; no commercial
DLL or EXE was loaded or executed.

Actual native Level vtable10428AFC has virtual+7C=10302248, whose E9 thunk
reaches103963E0: exported `ULevel.CompactActors`. This is a real selected
service, not an unknown no-op. CleanupDestroyed thunk10301811 reaches
103965A0. In game mode with force0,103965CC..103965DC invokes CompactActors
BEFORE103965DF reads FirstDeleted. Therefore even a None deferred head does
not permit skipping native array compaction.

The normal Tick tail103A5B35..103A5B99 first logically toggles Level parity,
clears inTick, restores the two scratch/global marks, then calls virtual+98
with force0. Ordinary DestroyActor does not call this boundary itself.

## Exact compaction order

The complete normal CompactActors body103963E0..1039650D establishes:

1. Capture FirstDynamic(+110) as both destination and source cursors at
   1039640A..10396410. The split itself is never rewritten.
2. Test ascending source against the LIVE actor count each iteration. Read
   that live source slot. Null advances only source.
3. For a nonnull actor, read bDeleteMe(+28 bit80). A clear bit copies that
   retained pointer to destination, then advances both cursors. Surviving
   dynamic actors therefore retain their original relative order.
4. A set bit takes the10396430..1039644C logging branch, then advances only
   source. Static bytes at1047F770 contain the UTF16 literal `Undeleted %s`.
   The portable warning uses the canonical object path rather than claiming
   an implemented original GetFullName/output-device protocol. It does not
   invoke DestroyActor, a scalar destructor or any script callback.
5. After the scan, if destination differs from the current count, the native
   undo-aware array removal drops only [destination,current count) at
   10396452..103964F7. The reserved/static prefix remains untouched. There is
   no bStatic test, new LoadMap partition or FirstDynamic recomputation.

`portable_world_actor_order.inl::ReorderNativeWorldActors` implements the
different LoadMap partition and MUST NOT be substituted here. Its existing
`JournalNativeActorOrder` is reused only as the shared-root before-image:
it retains the complete prior slots, reverse indices, split and changed bit.
The cleanup method journals before the first changed slot/cache/resize,
updates reverse membership indices, borrows the same work and retained-state
budget and performs no independent transaction/commit. Rollback remains the
existing swap/index restoration before provisional actor retirement.

## Queue count, retained return and unimplemented branch

After compaction,103965DF reads actual FirstDeleted. None returns. Otherwise
103965F1..10396600 follows actual Actor.Deleted(+F8) links and counts the entire
chain. No per-node pending-delete assertion belongs to this counting loop.
At10396600..1039660B, count<128 with force0 returns, retaining all head/link,
UObject lookup/index and AI relationships. A small retained deletion queue is
original behavior, not a per-frame scalar deletion that should be invented.

The runtime method follows authenticated native Level/Deleted ownership and
uses the caller's work bound to refuse malformed/cyclic/excessive traversal.
It deliberately refuses the reached count>=128 branch. Such refusal must
roll back the whole enclosing frame, including preceding compaction; it is
not permission to drain/discard the queue or claim unlimited frame support.

The missing high-count/forced branch is materially broader:

-10396618..10396633 walks ascending LIVE actor count/slots and invokes each
 nonnull actor's actual Class virtual+78, `UStruct.CleanupDestroyed(actor)`.
 Core export-name index602/thunk101022F7 reaches10127170..101273BF. Its normal
 reference/struct chains differ from ExitProperties; a generic all-field
 clear or existing ConditionalDestroy is not equivalent.
-Outside editor,10396680 onward asserts each actual head pending-delete,
 captures its Deleted successor and advances FirstDeleted at103966A8..B4.
 It then obtains/casts actual slot0 LevelInfo and, when its native manager is
 present, calls UEventManager.DestroyActor at1039679A before the source
 scalar-deleting destructor virtual+0C(argument1) at103967A7..AD.
-The AI-state DestroyActor helper exists, but its runtime first-touch journal,
 GC edges and exact boundary publication still need joining. Original object
 destruction must unhash before freeing/reusing its object index; marking a
 detached actor removed or retiring an unrelated ConListItem cannot replace it.

Deferred Actor capture/rebind/map-unload guards remain explicit. A genuine
under-threshold cleanup return does not make queued checkpoints supported.
Only first Training gameplay integration is prioritized; no other map is
needed to justify this implementation.

## Verification status

The new method is included in the production joined normal dynamic-actor
phase. Targeted desktop and repaired Android native builds pass. Generated
production-driver510 checks/17 refusals exercise ordinary empty-head cleanup,
dynamic null-hole compaction, unchanged prefix/split and shared-root native
work-prefix rollback after the frame toggle. Original crate415/5 and deferred
ownership429/21 and animation588/24 pass separately; they do not establish this
cleanup method's nonempty/128-node paths. Final full desktop/shared regression
passes203 executed/242 registered/39 explicit owned-data skips/zero failures
124.33s. All45 checked originals are unchanged; all jobs are terminal.

The actual first-Training frame observation currently stops before cleanup at
the closed Engine.Mover native-dispatch allowlist, with whole-root undo. See
ORIGINAL-LEVEL-DYNAMIC-TICK-PHASE.md, including the subsequent static evidence
that Mover's Tick/physics entries use the same AActor thunks. Nonempty queue,
threshold/scalar teardown and live frame integration still need verification/
completion. No standalone helper result is claimed as a complete Level frame,
live crate/loot gravity or a playable APK.
