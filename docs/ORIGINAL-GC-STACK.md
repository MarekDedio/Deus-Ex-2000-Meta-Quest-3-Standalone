# Original GC context stack and lifetime contract

Data-only/static audit, 2026-10-27. This document describes a required dependency
of the actual first-Training TextWindow preferred-size call. It is not an
implemented service, a successful startup, or a headset acceptance result.
No commercial executable or DLL was executed or loaded as code. No compiler
input was changed, build started, or test run for this audit.

## Authentic declarations and entry points

The repository-owned `script_bytecode_inspect.exe` read these declarations
from the owned original `System/Extension.u`:

- `Extension.GC.PushGC`: native1271, function flags0x401, no arguments;
  `ReturnValue` is IntProperty, dimension1, flags0x580.
- `Extension.GC.PopGC`: native1272, function flags0x401;
  `gcNum` is IntProperty, dimension1, flags0x90 (optional input), no result.
- `Extension.Window.GetGC`: native1485, function flags0x401, no arguments;
  its ObjectProperty result has dimension1, flags0x580, type Extension.GC.
- `Extension.Window.ReleaseGC`: native1486, function flags0x401;
  its required input `GC` is ObjectProperty, dimension1, flags0x80,
  type Extension.GC, no result.

The protocol fields are actual original reflected properties, not inferred
presentation properties or an extra private vector substituted for the graph:

| Property | Exact descriptor | Original native offset |
| --- | --- | --- |
| GC.bFree | BoolProperty, dim1, flags0 | bit0 at+0xA4 |
| GC.gcCount | IntProperty, dim1, flags0 | +0xA8 |
| GC.gcStack | ObjectProperty GC, dim1, flags0 | +0xAC |
| GC.gcFree | ObjectProperty GC, dim1, flags0 | +0xB0 |
| GC.gcOwner | ObjectProperty GC, dim1, flags0 | +0xB4 |

The offsets identify the inspected 32-bit retail binary, not a portable C++
struct layout. Native/source/declaration authentication must remain separate
from translating those fields into the existing typed host.

Decorated exports identify PushGC100278D0, PopGC()100279F0,
PopGC(Int)10027A80, CopyGC10027760, copying constructor10026C70,
default constructor10026AD0 and Destroy10026D40. The XGC allocation operator
is10002C40. The script wrappers are1002BF60 and1002BFA0.

## PushGC: save a context, do not return a node

`R` below is the actual GC receiver; `N` is a saved GC node. All count tests
are signed 32-bit comparisons; count increment is the original machine-word
increment, not a widened count or a bounded array index.

1. Read `R.gcOwner` and `R.gcCount`. Preserve the original count as the
   integer return value.
2. If `gcOwner` is non-None, return that count with no writes or allocation.
   This branch does not forward a push to the owner.
3. If `R.gcFree` is non-None, use that object as `N`, first set
   `R.gcFree = N.gcStack`, then call `N.CopyGC(R)`.
4. Otherwise allocate an actual Extension.GC with Outer=R, NameNone and
   caller flags0, then run the copying constructor with R as its source.
   The native object size is0xB8 in this binary. The allocation operator
   delegates to original Core.StaticAllocateObject; a portable implementation
   must use its own authenticated constructor/identity domain, not that code.
5. If the native allocation returns None, return the old count without
   linking or incrementing it. The portable memory-admission failure policy
   must remain an explicit whole-root failure, not a fabricated success.
6. Once N exists, set fields in this order:
   - `N.gcOwner = R`;
   - `N.gcFree = None`;
   - clear only `N.bFree` (preserve other bits sharing its word);
   - `N.gcCount = R.gcCount`;
   - `N.gcStack = R.gcStack`;
   - read current `R.gcCount` again as the return value;
   - `R.gcStack = N`;
   - `R.gcCount = returnValue + 1` with 32-bit word semantics.
7. Return the pre-increment count, not N or the new depth.

CopyGC is a context copy; it must not copy the protocol fields in the table.
Those exclusions are essential when reusing a node or restoring a receiver.
Its complete context-field/store-order contract is a separate audit.

The copying constructor is not the default GC constructor followed by an
arbitrary public CopyGC call:10026C70 invokes the base constructor, clears
the six clip words, copies the source context, clears bFree, sets gcStack,
gcFree and gcOwner to None, and sets gcCount1. The subsequent push suffix
then replaces the saved-node protocol fields as above. Default constructor
defaults are documented in ROOT-WINDOW-CONTRACT.md; rerunning all those
defaults would add stores that the inspected copying path does not perform.

## PopGC wrappers: omitted and explicit minus one have the same meaning

Script native1272's wrapper initializes its local argument to Int-1, evaluates
the optional argument, then advances over EndFunctionParms. If the resulting
word is-1, it calls the no-argument native overload. Otherwise it calls the
integer overload. Thus explicit PopGC(-1) is also the no-argument operation;
it is not the same operation as directly invoking the C++ PopGC(Int) with-1.
The future VM native seed must match this sentinel and actual declaration.

No-argument PopGC() does the following before entering PopGC(Int):

- If `R.gcStack` exists, select that head node's current gcCount.
- Otherwise select `R.gcCount - 1` with 32-bit word semantics.
- Invoke PopGC(Int) on the same R. It does not redirect to gcOwner.

PushGC's wrapper only advances over EndFunctionParms, invokes PushGC(), and
stores the returned Int. It does not manufacture an argument or node handle.

## PopGC(Int): threshold unwind and reusable free contexts

For requested signed count T:

1. If `T >= R.gcCount`, return without writes. If `R.gcOwner` is non-None,
   also return without writes. These are distinct original guards, not a
   generic clamp of T or forwarding to the owner.
2. Initialize `lastPopped=None` and `matched=false`.
3. Inspect `R.gcStack`. Stop if absent or if its node.gcCount is signed-less
   than T. If it equals T, set matched=true.
4. Pop the selected node N and perform the stores in this order:
   - remember N as lastPopped;
   - `R.gcStack = N.gcStack`;
   - `R.gcCount = N.gcCount` (read before overwriting N's count);
   - `N.gcCount = R.gcFree ? R.gcFree.gcCount + 1 : 1`;
   - `N.gcOwner = R`;
   - set only `N.bFree` true, preserving other bits;
   - `N.gcStack = R.gcFree`;
   - `N.gcFree = None`;
   - `R.gcFree = N`.
5. Repeat until matched=true or the next stack head is missing/below T.
6. If at least one node was popped, call `R.CopyGC(lastPopped)` exactly once,
   after all those protocol stores. Otherwise do not copy a context.

The gcCount of a free node is therefore its free-list serial, not its saved
stack depth. Both stack and free-list next links use the same gcStack field.
The owner remains R in both lists; bFree distinguishes their membership.
No node is destroyed or handed back to an allocator by ordinary PopGC.

If no exact saved count equals T, the method still unwinds all successive
heads whose counts are at least T and restores the final popped context.
It does not require an exact count match or force the receiver's count to T.
T can be zero/negative in the integer overload; there is no minimum-one clamp.
An empty stack can leave gcCount unchanged even when the initial guard passes.

Examples, derived from these branches rather than observed retail execution:

- Fresh R starts count1/empty lists. Push returns1, saves node count1 and
  leaves R count2. No-arg Pop selects1, restores context/count1 and places
  that node on the free list with free count1/bFree=true/owner=R.
- A second push reuses the same identity. It removes the free head before
  CopyGC, clears bFree and returns1 again; there is no second allocation.
- With nested saved counts2 then1 and current count3, Pop(1) moves both
  nodes to the free list, restoring the older count1 context only once.
- On that stack Pop(2) restores only its newest count2 context. Pop(3) is
  no-op. Script Pop(-1) instead selects the current head's count.

These examples are proposed future controls, not current passing tests.

## Window acquisition/release: same receiver and actual context source

Window.GetGC1004DDC0 loads Window.gGc at+0x134. None returns None without
calling either operation. Non-None calls gGc.PushGC(), ignores its Int result,
then gGc.CopyGC(Window.wGc at+0x130), and returns that same gGc object.
It does not return the newly saved stack node, nor copy gGc to wGc.

Window.ReleaseGC1004DE50 tests its actual GC argument; None is a no-op,
otherwise it invokes PopGC() on that argument. It does not reload Window.gGc
or match against an invented context token. Preferred-size's successful
non-null acquisition must release the exact GC it obtained.

GetGC's owner-bound Push no-op branch does not suppress the following CopyGC.
Invalid/None source wGc is not a documented default-context branch: the
original CopyGC dereferences its source. The portable host must authenticate
valid, live, owned GC identities and report malformed state explicitly.

## Native Destroy and default-constructor boundaries

Destroy10026D40 has separate ownerless and owner-bound paths:

- Ownerless: call PopGC(Int0), then repeatedly remove the current gcFree head
  by its gcStack link and invoke its virtual deleting destructor with flag1.
  It counts released free nodes; greater than16 only enters original logging,
  not a16-node collection or allocation limit. Finally call the base Destroy.
- Owner-bound: select owner's free list when this.bFree, otherwise owner's
  stack list. Traverse via gcStack to find this node. If found after another
  node, set predecessor.gcStack=this.gcStack. If at the stack head, set
  owner.gcStack=this.gcStack. No owner count repair appears in this path.
- The free-head branch is counterintuitive:10026E49..10026E55 loads this.gcStack
  and stores it into **this.gcFree**, not owner.gcFree. This is a literal
  receiver/offset observation, not a claim that it is a safe unlink algorithm.
  Do not silently replace it with a guessed corrected behavior or use this
  audit to claim arbitrary explicit GC destruction is supported.

The ordinary C++ XGC destructor10002C70 performs base-destructor processing;
the linked-list policy above belongs to the separate virtual Destroy method.
Default-constructor initialization of gcCount1, empty lists, ownerNone and
bFree=false is already present in the host's InitializeWindowGC. That alone
does not establish stack allocation, reuse, context restoration or destruction.

## Owned adapter, budget, lifetime and rollback requirements

These are implementation requirements derived from the observed graph and
the existing portable transaction model, not claims about original safety:

- Authenticate receiver class/source and exact property/function schemas.
  Maintain actual GC objects, Outer=receiver on new saved nodes, native class
  identity and class-wide original unique-name counters. Nodes are non-Actor
  window-domain UObjects, not spawned Actors, collision shapes or save births.
- Existing ConstructWindowObject already admits a non-Actor Window/GC class
  and nonnull Outer, journals name counters and provisional identities, and
  establishes actual Class/Name/Outer/ObjectFlags. Use the real ownership
  domain, not a permanent detached node pool or untracked raw pointer vector.
- Every gcOwner/gcStack/gcFree/Canvas/font/underlineTexture property edge needs
  actual markable UObject ownership through the existing NativeWindowWrite
  reference-root service. RuntimeWindowDomain's objects vector is not an
  automatic strong root: it marks only its explicit root windows. Stack/free
  nodes must survive by real reachable object references.
- Precharge work, node storage, copied CDO/default state, reflected maps,
  object-reference roots, name/lookup entries and beforeimages before their
  allocations. Include retained free nodes in the existing byte/object limits;
  a Pop does not release their memory. Do not turn the retail logging threshold
  into a fabricated16-node semantic limit or raise a cap just to fit a graph.
- Journal the entire root operation: field values, exact node identity/list
  membership, copied context words, class counters, indexes, provisional
  objects, reference edges and retained allocation accounting. Budget/late
  callback/invalid source failure after acquisition must restore the pre-call
  graph, not merely balance a guessed pop. Rollback must remain nonallocating.
- Reject malformed ownership, incompatible class/source, cycles or excessive
  traversal through explicit charged validation. Do not silently drop nodes,
  copy defaults or turn a failed acquisition into no-GC sizing success.
- Save/unload and root retirement must not persist transient native GC stack
  nodes as authored actors. Old receiver/node identities cannot authorize a
  replacement map/root. Successful runtime collection and explicit original
  Destroy are different lifecycles and need separately justified integration.

Before accepting a stack implementation, controls should cover fresh/reused
and nested pushes; explicit thresholds without exact match; empty/current/
owner-bound no-op paths; all signed count boundary words; explicit/omitted
minus-one native arguments; context bit-preserving copies with live reference
edges; wrong declarations/classes/sources; alias/cyclic/malformed graphs;
allocation and every write/work/retained-byte cut; collection while both lists
are populated; save/replacement and late outer rollback. Then repeat the actual
first-Training preferred-size call, not only an isolated helper fixture.

## Evidence and unresolved scope

Original Extension.dll SHA256:
`93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
Export identities/RVAs are already recorded in
`artifacts/world-native-startup-20261011/extension-exports-imports.log`.
This audit inspected original Extension.u declarations with the owned data
reader and original DLL data with GNU objdump at100278D0..10027BB0,
10026AD0..10026E82,10002C40..10002CD0,1002BF60..1002C019 and
1004DDC0..1004DED0. No raw commercial disassembly was added to the project.

CopyGC's complete typed context copy, full GC text parser/metrics, preferred-
size output aliasing/rounding, failure policy and actual host adapter remain
implementation work. In particular the explicit free-head Destroy branch
needs a deliberate supported-scope decision, not inference from a clean list
design. This audit does not remove the current preferred-size refusal and
does not establish live VR crate destruction, loot or gravity acceptance.
