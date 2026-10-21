# Original ULevel.DestroyActor contract

Static audit checkpoint, 2026-10-20. This is a prerequisite for the first
`Maps/00_Training.dx` Spawn/encroachment and destructible-crate paths. It does
not implement DestroyActor, claim successful startup, or prove working crate
destruction, loot, gravity, saves or campaign travel. No other map was loaded.
Commercial DLLs were read as PE bytes/disassembly, never loaded or executed.

## Authority and evidence

Owned originals under `D:/Steam/steamapps/common/Deus Ex/System`:

| File | SHA256 |
| --- | --- |
| Engine.dll | `0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4` |
| Core.dll | `8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4` |
| DeusEx.dll | `B91002F3E7658C0562D28BCCC887CF9DB79A47A19F5E09E8BE460C9D0C0BEE04` |
| Engine.u | `1742D16F49492A02391FA6A9CA25887F0CC4F30EC9964925226217D8858737DA` |
| Core.u | `4FB22EE4E3A6868DFABB2B46B15FAD8E5094F5FDA5927A14FF11670A744EC36C` |

These fingerprints were rechecked after the inspection. The source files were
unchanged. Android NDK27 LLVM18 supplied static `llvm-readobj --coff-exports`
and `--coff-imports`, and bounded `llvm-objdump --disassemble
--x86-asm-syntax=intel --no-show-raw-insn` output.

Engine export ordinal895 is
`ULevel::DestroyActor(AActor*,int)`, thunk `10302568`, body `10395BA0`.
All normal paths were read through `10396120`; the following interleaved/tail
fragments are exception reporting, not extra normal gameplay steps. Objdump's
nearest-symbol `??2AInventory` labels are not function identities.

Original full evidence remains in
`artifacts/collision-hash-save-audit-20261018/engine-destroyactor-body.asm`.
New bounded evidence is in ignored
`artifacts/training-destroy-actor-audit-20261020/`:

- `engine-destroy-actor-normal-body.asm`: entire normal-body interval, including
  the interleaved exception fragments, ending before the later tail catches.
- `engine-get-actor-index.asm`: actual native ascending actor-index search.
- `core-end-state-name-registration.asm`: primary hardcoded-name registration.
- `core-bool-property-link.asm`, `core-bool-property-serialize.asm`: native
  computed Bool layout versus ordinary package serialization.
- `core-conditional-destroy.asm`, `engine-actor-destroy.asm`,
  `core-object-destroy.asm`: immediate native teardown, distinct from deletion.
- `source-metadata-and-vtables.log`: bounded direct source observations.
- `source-sha256.log`: the five original fingerprints above.

Import identity is independently recoverable from the Core IAT starting at
`105999F4`, with four bytes per symbol, in
`artifacts/spawn-frame-audit-20261011/original-engine-imports.log`:

| IAT slot | Imported service |
| --- | --- |
| `10599C9C` | UObject.IsValid |
| `10599C94` | GIsEditor |
| `10599B0C` | GUndo |
| `1059A02C` | UObject.IsProbing(FName) |
| `10599BB8` | FName(EName) constructor |
| `10599A88` | UObject.FindFunctionChecked |
| `10599B08` | UObject.ConditionalDestroy |
| `10599AE0` | UObject.Destroy |
| `10599AE8` | UObject.Modify |

## Arguments, entry checks and refusal results

ECX is retained Level `L`; stack `+08` is source `S`; `+0C` is the integer
force argument `F`. The body returns native integer Bool and `ret 8`.
The word at `+0C` is subsequently reused for locals; later accesses to that
stack slot must not be interpreted as repeated reads of `F`.

1. Assert `S != None` (`10395BC5..BE7`), then call actual UObject.IsValid and
   assert its nonzero result (`10395BEA..C0B`). Invalid native pointers are not
   a successful false result; a portable adapter must refuse safely.
2. Read actual GIsEditor. Nonzero goes directly to the common body at
   `10395D1D`, bypassing the following ordinary-game entry filters.
3. Outside editor, read native actor flag DWORD at `S+28`. If mask `05` is
   nonzero, return false (`10395C1F..24`, `10395CD8..CEA`). This precedes the
   pending-delete check: do not globally move idempotence to the first step.
4. If mask `80` is set, return true immediately (`10395C2A..2C`).
5. Continue only if native Role byte `S+31 == 4`, or `F != 0`, or the earlier
   actor-flag DWORD has bit `01000000`. Otherwise return false at `10395C44`.
6. Actual PlayerPawn ancestry enters the connection test below. Non-PlayerPawn
   sources continue directly to the common body.

### Network-player branch is not a Viewport shortcut

At `10395C61..6F`, native Class/Super comparison uses exported
`APlayerPawn::PrivateStaticClass = 1058C160`. If this ancestry matches, inspect
the source's native Player pointer at `+5D4`. A null Player continues normally.
The nonnull pointer must have actual `UNetConnection` ancestry, whose exported
PrivateStaticClass is `10590C40`, to enter the remaining special branch.

For such a connection, if its pointer at `+E64` is nonnull and its DWORD at
`+60` is not1, invoke that pointed object's virtual `+5C` and return false
(`10395CC3..CD8`). Null `+E64`, value1 at `+60`, or non-NetConnection Player
continues normally. The precise native names and full contract of the `+E64`
object, `+60` state and `+5C` operation remain unaudited here. They must not be
replaced with an arbitrary close callback or treated as absent from a reflected
field's missing value. A standalone provider can establish real absence of a
NetConnection; it cannot pretend an actual network connection is a Viewport.

### Field names, enum authority and Bool-mask limits

The raw offsets/masks above are the immediate DLL authority. Engine.u's actual
Actor.Children/Next property chain, recorded in
`artifacts/collision-hash-save-audit-20261018/engine-actor-properties.log`, has
consecutive Bool declarations beginning `bStatic`, `bHidden`, `bNoDelete`,
`bAnimFinished`, `bAnimLoop`, `bAnimNotify`, `bAnimByOwner`, `bDeleteMe`.
`bNetTemporary` is the twenty-fifth Bool in that initial chain (zero-based24).
This corroborates the expected mask names `05 = bStatic | bNoDelete`,
`80 = bDeleteMe`, and `01000000 = bNetTemporary` under the original packed
layout. It is not evidence that a portable .u descriptor contains a native
bit mask or offset.

Core's exported UBoolProperty.Link thunk `10101230` reaches
`10166060..10166111`. When its owner-packing condition permits an immediately
preceding Bool and that mask has room, it doubles the previous mask and reuses
the preceding property Offset. Otherwise it starts bit1 in a DWORD-aligned new
slot. Exact relevant stores are `101660CA..E2` and `101660E4..F7`.
The owner packing/layout predicate must not be guessed from that excerpt.

UBoolProperty.Serialize thunk `10101DF7` reaches `10166160..101661C1`.
After UProperty.Serialize, it serializes the native BitMask DWORD at `+5C`
**only when both archive IsLoading and IsSaving are false** (`10166196..AE`).
Ordinary .u loading therefore does not supply such a serialized mask. The
portable `PortablePropertyDescriptor` correctly has no Bool-mask payload.
A future native field adapter must authenticate the exact declaring fields
and its supported layout; it must not read nonexistent trailing mask bytes.

Role4 is independently resolved from original Engine.u, not recalled from a
different UE version. Export reference1352 is Actor.ENetRole, Outer reference2,
Class reference-29, payload offset120795/size15:
`00-00-51-11-05-44-10-40-10-42-10-41-10-68-0E`.
The complete payload decodes as property terminator None, UField base0,
Next1105, five names: ROLE_None, ROLE_DumbProxy, ROLE_SimulatedProxy,
ROLE_AutonomousProxy, **ROLE_Authority**. All15 bytes are consumed.
Actor.Role is the original scalar ByteProperty constrained to that enum,
flags20; RemoteRole is distinct, flags21.

## Common body: exact normal order

### Saved slot, undo and EndState

At `10395D1D..31`, call actual `L.GetActorIndex(S)` and retain that index `I`
through all later callbacks. Its exported thunk `10301C71` reaches
`10346030..103460A1`: scan native live slots in ascending order, returning the
first pointer-identical match. Missing source logs and returns-1; a portable
adapter must refuse before using an invalid slot, not invent an appended slot.

If GUndo is nonnull, call its virtual `+04` to save the one actor-array item
at `I`, with the native serializer/destructor arguments (`10395D28..55`).
Call source virtual `Modify` at `10395D5C`. The original AActor vtable's `+1C`
entry `1040C734` jumps through Core's UObject.Modify IAT slot `10599AE8`.
Editor/undo state is native authority, not an inferred UnrealScript property.
Follow-up primary audit,2026-10-21: AKeypoint has that same `Modify` entry.
Core thunk101014D8 reaches normal body10150170..101501C2. It reads exported
GUndo101E3318 and calls its virtual+0(source) only when that pointer is nonnull
AND source.ObjectFlags bit1 is set. With authentic GUndoNone it returns without
any object/flag writes; no dirty/lifecycle flag or synthetic editor journal is
introduced. See ORIGINAL-CONDITIONAL-DESTROY.md.

At `10395D65..D90`, continue to EndState only when the actual StateFrame is
nonnull, its Code pointer at frame`+18` is nonnull, and actual
`S.IsProbing(FName(13D))` is nonzero. This tests running Code, not merely the
selected state or HasStack flag. If admitted, resolve the selected EndState
with FindFunctionChecked and invoke source ProcessEvent with null parameter
and result pointers (`10395D92..DB0`). After this call, read source pending
delete; if set, return true immediately. No native InEndState/GotoState
transition wrapper or state clearing appears at this site.

The index13D is conclusively **EndState**. Core has UTF16 `EndState` at
VA `101ACCE8` (file offsetACCE8). Registration at `1014D80E` pushes None link,
flags04000000, index13D, and that string, then calls exported
AllocateNameEntry `10101D52` and Hardcode `10101B45`. The existing portable
probe table's `300 + 17 = 317` independently agrees, but is not the primary
evidence. Do not leave this event mislabeled as an unknown literal.

### Support detach, hash removal and Destroyed

At `10395E01..20`, if source.Base is nonnull, call
`S.SetBase(None,true)`. After that callback-capable service, if source is now
pending-delete return true. A null Base skips both the call and that check.

Read StandingCount once at `10395E22`; only a nonzero Byte enters the scan.
For slots0 upward, reread native Level actor-array length and storage live.
Skip null slots. If candidate's direct Base equals source, call
`candidate.SetBase(None,true)`. Do not use a reverse support list, cache the
slot count, recursively detach indirect bases, or add a pending-delete filter.
There is **no** source pending-delete check inside or after this scan.

Read actual `L.Hash` at `10395E91`. If null, skip hash work. Otherwise, read
source's live bCollideActors; if set, call Hash.RemoveActor(S). Then reload
actual `L.Hash` and call its virtual `+20` CheckActorNotReferenced(S) regardless
of that collide bit (`10395EAA..EB6`). The audited retail implementation of
this particular virtual is a no-op; its identity still must be established.
A disappeared Hash after removal is not an original clear/skip branch.

Resolve selected Destroyed using exported ENGINE_Destroyed at `1058372C`,
then call source ProcessEvent with no arguments (`10395EB9..ED9`).
Read source pending-delete after the call; true returns native true at once.
The flag was not pre-set by this outer DestroyActor operation.

### Live ownership/touch scan

At `10395EF2..FBB`, scan **every native Level slot** in ascending order,
rechecking current length/storage each iteration. Capture each nonnull
candidate pointer `C` for the current iteration; do not walk childActors.

- If `C.Owner == S`, call `C.SetOwner(None)`. Read source pending-delete;
  if set return true. Then advance to the next Level slot. This branch skips
  C's touching scan even if SetOwner changed the relationship again.
- Otherwise, for indices0..3, read C.Touching[index] live. Each match with S
  calls **S.EndTouch(C,1)**, then reads source pending-delete and returns true
  if set. Continue to the next touch index even if callbacks alter slots.

This reverses the tempting but incorrect source.Touching scan. It also relies
on the original EndTouch semantics, not the legacy portable UnTouch helper:
suppressSelf1 prevents source's own UnTouch event; the reciprocal candidate's
UnTouch still executes when its first matching slot exists. No history-bit
filter or all-duplicates compaction is invented. See ORIGINAL-ACTOR-TOUCH.md.

At `10395FC0`, capture source.Owner in `O`. If nonnull, resolve and dispatch
**captured O.LostChild(S)** using ENGINE_LostChild at `10583724`.
Read source pending-delete after that callback and return true if set.
There is no `SetOwner(S,None)` and no source.Owner store in this body.
Later callback mutations do not retarget the captured event receiver.

### Driver notifications and native slot deletion

At `1039602F..40`, if actual native L.NetDriver (`+3C`) is nonnull, call its
virtual `+74` NotifyActorDestroyed(S). Then inspect actual DemoRecDriver
(`+88`); call its virtual `+74` only when it is nonnull and its native
ServerConnection (`+38`) is null (`10396043..57`). A standalone port must
establish driver absence from its actual engine service configuration.
These are not empty callbacks inferred from absent reflected properties.

At `1039605A..80`, assert the actor at the retained entry index I is still S.
Then reload actor-array storage and clear that exact slot at `1039608F`.
Do not replace this assertion with a new first-match search, remove/compact
all matches, reuse a callback-mutated slot, or reset the array's count.

Only **after clearing the slot**, set source's pending-delete bit80 at
`10396096..9C`. All preceding early true returns preserve whatever nested
deletion/callback side effects actually occurred; true is not root rollback.

### Audio, ConditionalDestroy and deferred deletion

At `1039609F..AC`, read actual `L.Engine` (`+40`), then Engine.Audio (`+4C`).
If Audio is nonnull, invoke its virtual `+7C` with source. Do not infer a null
native AudioSubsystem from the renderer's or portable voice mixer's state.
The audio operation's complete implementation remains an explicit provider.

Call imported **UObject.ConditionalDestroy(S)** at `103960B1`, ignoring its
result. This is neither an ordinary script Destroyed call nor the UObject
destructor/free-index/unhash operation.

- Core thunk `10102270` reaches `1014FDB0`. If native index is-1 or
  RF_Destroyed00800000 is already set, return false. Otherwise write
  `(oldFlags & 7F7FFFFF) | 00800000`, explicitly clearing the old
  RF_DebugDestroy80000000 marker as well as setting RF_Destroyed. Invoke
  selected native virtual Destroy at vtable`+24`, then inspect the base-destroy
  debug marker. An absent marker logs through Logf and STILL returns true;
  that diagnostic is not an assertion or a native false result.
- Base AActor.Destroy thunk `10301F8C` reaches `10368F60..10368FBC`.
  If auxiliary UObject pointer `S+314` is nonnull, call its ConditionalDestroy
  and then clear that field. Next call Core UObject.Destroy via `10599AE0`.
  Engine.u's transient Actor.RenderInterface declaration corroborates this
  auxiliary slot's intended field, but a live native layout binding must be
  authenticated before using the reflected name as offset authority.
- UObject.Destroy thunk `101025F4` reaches `1014FA50`. It sets
  RF_DebugDestroy80000000, calls ExitProperties(S,actual Class), detaches the
  linker with `(None,-1)`, and caches actual Outer's object index (or0) in its
  later unhash field. ExitProperties follows actual Class.ConstructorLink+68
  and property.ConstructorLinkNext+54, calling selected DestroyValue+94 for
  each reached property at its native Offset. It is not an all-field clear.
  SetLinker first verifies and nulls the old live ExportMap[index].Object
  attachment, when a linker exists, then writes None/-1. Immutable source
  provenance is not that live attachment. Full details and actual first-map
  constructor-field census are in ORIGINAL-CONDITIONAL-DESTROY.md.

Static exported-vtable checks show Engine Actor, Brush, Decoration, Inventory
and Pawn all use `10301F8C` at slot`+24`. A later bounded PE read confirms
Keypoint also uses10301F8C, covering PickupDistributor's native ancestor.
DeusExDecoration and ScriptedPawn use
`1001AFA4`, a thunk through IAT `1002727C` whose import is Engine AActor.Destroy.
DeusExPlayer instead uses its own `100141F0` override. This is finite evidence
for those native ancestries, not permission to bypass every subclass override.

Reread actual GIsEditor at `103960B7..C8`. Outside editor, load native
L.FirstDeleted (`+100`), store it into source's Deleted link (`+F8`), then
publish source as L.FirstDeleted (`103960CD..D9`) and return true. No active-tick,
newborn, class or queue-length predicate guards that insertion.
In editor, invoke Level virtual`+98` CleanupDestroyed(1), then return true.
CleanupDestroyed is a separate service (thunk10301811 ->103965A0), not this body.

Actual UObject lookup and object-index lifetime continue until genuine
destructor/cleanup. The normal UObject destructor later unhashes and frees its
index; see ORIGINAL-OBJECT-HASH-ORDER.md. ConListItem scalar deleting-destructor
retirement is **not** an appropriate replacement for Actor destruction.

## Required runtime integration and current non-claims

Runtime checkpoint2026-10-21: the genuine current native-Level path now uses
`quest_destroy_actor.h` and `portable_actor_destroy.inl` through the callback/
hash/slot/pending prefix, with original SetOwner ordering. It explicitly refuses
ConditionalDestroy/property/linker teardown and deferred ownership. Repeated
actual first-map startup reaches that later boundary with full rollback. See
TRAINING-ACTOR-DESTRUCTION.md. The legacy metadata-only path described below
is retained separately and is not the current native-Level implementation.

The existing legacy LifecycleNative279 marks bDeleteMe before support/hash and
Destroyed callbacks, scans different relationships, and refuses active retail
hash mutation. Its ordering cannot be reused merely by replacing RemoveActor.
Likewise, a spawned encroachment veto cannot be handled by returning None and
leaving the live birth in its slot/hash. SpawnActor calls DestroyActor(birth,0)
and then returns None regardless of that native deletion result.

A faithful runtime implementation still needs all of the following:

- Exact declaring-field/schema and native editor/standalone authorities for
  entry flags, Role/force handling, StateFrame running Code and selected events.
- Real SetBase and live source-basing scans, with the precise early-delete
  checks above and no broad reentrancy shortcut.
- Original hash removal/CheckActorNotReferenced, same generation/root and
  current collide bit; native world-array item journaling and a genuine hole.
- Selected Destroyed/EndState/LostChild and actual SetOwner/EndTouch dispatch
  through the same Execution, preserving nested callback effects and budgets.
- Real driver/audio presence and handling or explicit unsupported refusal;
  no invented empty service when a live native object exists.
- ConditionalDestroy flags, supported native subclass teardown, auxiliary
  UObject/dynamic-property/linker ownership, and native deferred-deletion list.
- Defined eventual cleanup/GC/weak lookup/index lifetime and transaction undo,
  including callbacks that complete nested deletion before the outer call.
- Allocation-free rollback of all touched fields/slots/relationships/hash,
  conditional flags, deleted-list ownership, RNG and any new loot/bindings.
  Ordinary false/true returns are not exceptional rollback instructions.
- Actual first-Training spawned-check veto/clear, positive crate destruction,
  surviving usable loot, support/falling/landing and restart/save evidence.

Network-player close behavior, editor undo/cleanup, nonnull native auxiliary
objects, full property/linker teardown, Audio notifications and unaudited
native subclass overrides are explicit open providers. Safely refusing them
is an honest boundary, not a completed implementation or successful no-op.
This audit does not narrow the full-port objective or justify integrating other
maps before the first Training map works.
