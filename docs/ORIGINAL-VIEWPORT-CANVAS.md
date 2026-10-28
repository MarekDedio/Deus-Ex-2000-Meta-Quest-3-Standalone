# Original viewport Canvas constructor and Init

Implementation2026-10-28. This is one real viewport-startup dependency, not
Console/RenDev/OpenWindow, Input.Init, drawing or a completed live VR bridge.
Original game binaries were inspected statically only; no original DLL/EXE is
loaded or executed.

## Owned source evidence

UViewport constructor103730B0 creates configured Canvas before configured
Input. At1037312E it loads `ini:Engine.Engine.Canvas` constrained to the actual
Engine.Canvas class, then StaticConstructObject with Transient Outer, NameNone,
requested flags0 and no template. It stores viewport.Canvas at+5C at103731D9,
calls the Canvas virtual+54 with actual viewport at103731DC..E1, and only later
constructs Input/stores viewport+60 at10373223. The owned INI selects
Engine.Canvas; no fallback class is invented.

UCanvas InternalConstructor103011F9 ->10348100 ->constructor10348120 calls the
original UObject constructor and installs vtable10428EEC. It does not reset
the copied default fields. UCanvas.Init thunk10302095 ->10377540 simply stores
the supplied viewport at Canvas+78 and returns. It does not call Reset, bind a
scene Frame/Render pointer, set dimensions or invoke script initialization.

The serialized original Engine.Canvas UClass flags are0x112 (not Transient),
within Core.Object. Actual Canvas.Viewport is const ObjectProperty/flags2.
SizeX/SizeY and private FramePtr/RenderPtr are also const; Font/drawing controls
and four named font properties remain copied authored defaults. Original
Canvas.Reset is a separate139-byte script event and must not run from Init.

The original-data metadata audit is recorded in ignored
artifacts/viewport-class-audit-20261028.log. Its helper uses only the port's
package decoder and owned Engine.u; disassembly uses read-only objdump.

## Portable lifecycle

`PreparePortableRuntimeViewportCanvas` takes a live registered viewport and an
explicit original engine INI. The selected actual Class is authenticated; the
currently audited native constructor is Engine.Canvas. Other native overrides
refuse rather than inheriting an invented constructor implementation. The
existing strict engine-selector profile refuses conflicting duplicate Canvas
settings and never writes the supplied file.

Construction copies defaults, allocates a genuine non-Actor UObject with actual
Class/Outer/Name/flags and original MakeUniqueObjectName semantics, publishes
its current-generation object-index/hash identity, and establishes reciprocal
Viewport.Canvas/Canvas.Viewport owning edges. GC marks these actual edges. The
inventory is weak and used for accounting/validation, not a second GC root.
Provisional roots and all source/default/field copies share the existing state
budget; property and pointer stores share the outer VM write/work allowance.

Canvas inventory accounting is independent of the live player-session root:
escaped objects remain metered after client teardown until actual GC retirement.
Reflected Object-edge maps include outer/inner buckets, nodes, key storage and
edge-vector capacity in both live and before-image ledgers. First touch admits
the copied graph before allocation and checks any larger resulting capacity.
The weak inventory is staged at the bounded viewport maximum, retaining its old
allocation by swap. Rollback restores that exact allocation before provisional
roots are released; commit releases the old allocation. Retiring the last Canvas
also releases the empty weak inventory. No resource allowance was raised.

The transaction restores native edges, unique-name counter, property storage,
namespace and object-index publication before provisional roots are released.
Canvas objects are not Actor births or map exports and never enter gameplay
saves. Client teardown clears and unindexes actual Canvas peers; map changes
retain them with their viewport, as they do Input. Reads and repeated preparation
do not create another Canvas. Preparing Canvas after Input already exists is
refused; the port must preserve the original constructor order when connecting
the complete viewport initializer.

The detached diagnostic snapshot contains22 real fields, actual object/class/
outer/viewport indices and constructor counter. Raw FramePtr/RenderPtr addresses
are deliberately not converted into script-visible portable32-bit addresses.
No Canvas.Update/Reset or rendering success is implied by initialized=true;
that flag describes only the actual single-pointer UCanvas.Init method.

## Verification

Optimized desktop, strict ARM64 and Android-native builds pass. The dedicated
owned test passes145 checks/10 refusal controls and covers copied defaults/no frame fabrication,
all six constructor/header/native stores, whole checkpoint/GC/name/index undo,
write-budget refusals, repeated preparation, distinct viewport ownership,
Canvas-before-Input order, retention across map reload and client teardown.
Discovery/compile failures are retained as evidence, not counted as passing.
Review also identified missing reflected-edge accounting and retained weak-vector
capacity after rollback; these are corrected in the current frozen revision.
The extended test additionally compares inventory bytes/capacity across all
refusals, tests native-work limits and proves detached reads preserve identity
and accounting. The broad rebuilt desktop suite executes229/275 registered
tests, with46 explicit owned-data skips and zero failures (258.55s). Separate
rebuilt owned first-Training runs pass: crate415/5, deferred429/21,
animation588/24, FastTrace252/0, Input classes10413/38, viewport Input10310/20,
schema34355/1357, metadata13896/125, HUD11225, Core SET9548/6, GC155/16,
recording route1232 and actor materials1308actors/227selected/0null.

The integrated startup diagnostic now prepares actual Canvas before Input.
Two-Fire repeat2 retains271677 instructions/32553 writes, two attack notifies,
four traces, two crate changes, nine births and full undo before the genuine
FExec SET guard. Door repeat2 retains239536/19432, both56-unit moves/two ends
and the exact missing ReadInput service. These are expected-boundary rollback
diagnostics, NOT successful startup, a live world Tick or headset crates.
All45 original files and632 frozen compiler inputs match length/UTCmtime/SHA256;
all compile/test jobs are terminal. Discovery failures and an argument-wrapper
usage failure remain in ignored logs, not counted as passing verification.
