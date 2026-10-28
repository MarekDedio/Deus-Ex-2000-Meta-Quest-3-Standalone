# Original native Window activation

Implementation batch 2026-10-28; verification results are recorded below only
after the corresponding jobs finish. This is a player-startup prerequisite,
not a completed live crate bridge, original renderer or new APK.

## Implemented contract

`quest_native_window_activation.h` coordinates Extension1004EE20 in its actual
order: compare local visibility; capture effective visibility; local store;
capture effective visibility again; focus consistency; grab consistency; mark
reconfigure; recompute init count; visibility callbacks/sounds; reconfigure the
tree; nearest-modal accelerator dirtiness LAST. Equal local or effective state
returns at the original boundary. The captured transition survives reentrant
callbacks; later providers read live state.

`portable_window_activation.inl` implements the bounded live child traversals
and the actual typed fields, with shared native scratch/work and the caller's
transaction. RecomputeInitTree10050D70 adjusts Root.initCount for each visible
subtree node that is not bIsInitialized. It does NOT set that bit or invoke
WindowReady. InvokeVisibilityChanged10050E30 dispatches the most-derived script
before sound and locally visible descendants. First-child and sibling links
are read after callbacks/recursion, not cached as a stale child list.

Visible AskParentForReconfigure1004E060 retains its unconditional Self dirty
stores and hidden-ancestor early return. A visible child offers its real
parent's +88 virtual the request, propagates to the current parent when not
accepted, and resizes Self if it remains dirty. An unparented root clears its
reconfiguration bit BEFORE calling the +90 virtual within scoped configuration
bookkeeping. Ordinary Window's ChildRequestedReconfiguration is a real script:
it calls child.ResizeChild1462 and returns true. The new no-argument1462 binding
authenticates Extension.Window.ResizeChild and reuses the existing original
resize/query/configure/clip implementation. Static exec10053050 consumes
EndFunctionParms then calls1004EAB0; no new argument/default policy is invented.

`portable_window_configuration.inl` supplies the actual +90 overrides:

- Root1003A660 delegates to TabGroup10045640. TabGroup/Modal retain the real
  bSizeChildrenToParent flag; false dispatches ConfigurationChanged script,
  true configures locally visible children in reverse sibling order.
- Radio10038840 always configures locally visible children to the parent's
  current dimensions in reverse order, without a generic script callback.
- Tile100489C0 computes held child sizes with live Width/Height constraints,
  then configures locally visible children from their held values in forward
  order. Both constraints are specified, so there is no invented copy-back.
- TextLog10047910 acquires its actual GC, measures its current native text,
  stores bTooTall using the original less-or-unordered height comparison,
  releases the context, and only then invokes ConfigurationChanged script.

All original DLL inspection is static/read-only. No original commercial DLL
or EXE is loaded or executed by these implementations or tests.

## Diagnostics and explicit remaining limits

The existing prefix probe captures the original graph immediately after script
InitWindow returns, through an observation-only hook, then allows native
activation to run. It no longer substitutes an artificial production failure
at that boundary. The separate activation probe captures the actual later
state. Both are explicitly non-publishing and roll back their entire root;
they do not establish live Quest startup or persistent state.

Nonnull focus/grab negotiation, parented Modal keyboard/mouse handling,
non-None Window visibility audio, and callback-triggered deferred destruction
still refuse rather than report invented success. First Training's initial
root has no focus/grab, no parent and None visibility sounds. These genuine
early-outs do not prove the other branches.

WindowReady belongs to actual Root.PreRender1003A760 ->1003A9E0 ->1003BC80,
not activation or Tick. Its child-before-parent callbacks and initialized-bit/
initCount stores remain separate work. Config/FExec, initialized Input and the
live VR weapon/frame/publication bridge are also still required before the
crate/loot/gravity test APK.

## Verification

Final pure coordinator run:145 checks/23 explicit injected refusals pass.
This uses a declared mock provider and proves sequencing, not an engine tree
or transaction. The first actual original-data discovery reached the missing
ResizeChild1462 packet at ChildRequestedReconfiguration PC9/op101,15515 writes;
that packet is implemented and the subsequent original native initialization
returns completely at16025 writes, below the unchanged16384 default allowance.

Original-data HUD11225 checks pass. Independent prefix/activation snapshots
both retain198 objects (65 Windows and133 GC objects). The activated root is
visible, layout dirtiness is cleared, initCount==17 exactly matches the17
effectively visible uninitialized Windows, all65 initialized bits remain false
until actual PreRender, and every configuration lock is released. Intentional
diagnostic rollback restores the full save/revision/actor set/native root list,
property edges, unique-name counters and GC object/memory totals. Repeated
activation and shared-write budget refusals preserve that whole-root undo.

Optimized desktop, strict ARM64 and Android-native builds pass. Final CTest:
270 registered,226 executed,44 explicit skips,zero failures,88.66 seconds. All
eight rebuilt owned first-Training regressions pass (crate415/5,deferred429/21,
animation588/24,FastTrace252/0,Input classes10413/38,viewport10310/20,tutorial
route1232,materials1308 actors/227 selected/0 null). Separate owned metadata
13896/125 and GC155/16 pass; GC retains its68 Window/43 text cases.

Actual two-Fire repeat2 is unchanged at271677 instructions/32553 writes and
the precise SET InputExt T Talk FExec refusal, retaining two attack notifies,
four traces,two crate changes,nine births and complete undo. Door repeat2
retains both56-unit moves,two endpoints and the exact unprepared ReadInput
refusal. These successful expected-boundary diagnostics are NOT live gameplay.
No budget,save format or accepted VR-layout change; no new APK is packaged.
Final audit:all45 original files and618 frozen compiler inputs match length,
UTC modification time and SHA256 baselines;all build/test jobs are terminal.
Evidence:artifacts/root-activation-final-audit-20261028.json and the corresponding
root-activation build/test logs. Earlier discovery/build-error logs are retained
as discovery evidence,not passing verification.
