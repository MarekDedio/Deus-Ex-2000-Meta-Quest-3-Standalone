# Original root activation and input continuation

Update2026-10-28: the initial first-Training native activation and visible-layout
path below is implemented; the original unbound InitRootWindow completes at
16025 writes within unchanged budgets. See ORIGINAL-WINDOW-ACTIVATION.md for
implementation/verification and explicit unsupported branches. WindowReady in
PreRender, initialized Input and FExec/config persistence remain distinct gaps.
The2026-10-27 text below records the prior static implementation contract.

2026-10-27 static/read-only audit. This is an implementation contract, not a
claim that visible original roots, input, live physics or an APK are finished.
No original commercial DLL/EXE code was loaded or executed. Current compiler
inputs stayed frozen throughout the audit.

The pending GetEnum implementation advances the dormant original root through
its script InitWindow to PC196/opcode11,198 objects/15491 writes, where native
InitRootWindow still explicitly refuses SetVisibility/init-tree/reconfiguration.
Bound-player startup may encounter other FExec/input dependencies first; it
must be measured separately, not inferred from this unbound-player diagnostic.

## Activation is not a successful Bool assignment

Original SetChildVisibility1004EE20 compares the local visible bit, captures
effective visibility, stores the local bit at1004EE86 and recomputes effective
visibility. When effective visibility changes, the ordered work is:

1. Check focus consistency10050B80,then mouse-grab consistency10050C90.
2. Mark bNeedsReconfigure,then RecomputeInitTree10050D70 for the new visibility.
3. InvokeVisibilityChanged10050E30:self callback and optional sound,then locally
   visible descendants through live firstChild/nextSibling links.
4. ReconfigureTree1004E520(false),then mark nearest modal accelerators dirty
   through1004C1D0 LAST.

RecomputeInitTree counts not-yet-initialized visible-subtree nodes in the actual
root.initCount. It does not set bInitialized or dispatch WindowReady. The current
first-Training root has both focus/grab pointers None,so their first checks are
genuine no-ops; callback reentrancy still requires subsequent actual-state reads.
Do not replace these checks with unconditional success for non-None pointers.

Root's native visibility override is Modal10037330 (Root vtable10059CC4+94).
It invokes the most-derived script; a parent-null root does not increment a
parent's keyboard/mouse handling references. The sound helper10050050 validates
GetRoot/GetPlayer and skips None sounds before requiring an audible backend.

## Visible configuration dispatch

ReconfigureTree checks effective visibility,asks for parent reconfiguration when
needed,then traverses all live children; each child has its own visibility gate.
AskParent1004E060 always marks Self needsReconfigure/needsQuery. For a visible
unparented root,it clears needsReconfigure FIRST at1004E1C4,enters the actual
configuration context/counter,and calls Root vtable+90 ->1003A660.

That Root override calls TabGroup10045640. With sizeChildrenToParent set,it
configures locally-visible children at0,0,parent.Width,parent.Height; otherwise
it invokes the optional most-derived ConfigurationChanged script. It is not the
generic ChangeConfiguration two-walk path. Constructor default
sizeChildrenToParent=false must not be overridden to force a convenient layout.

Existing ResizeNativeWindowChild,ConfigureNativeWindowChild,query,clip/resort and
callback helpers are reusable. NativeWindowConfigurationChanged still explicitly
refuses Root/TabGroup/Modal override handling; visible root/layout semantics are
not supplied merely by lifting the InitRootWindow guard.

## WindowReady belongs to actual pre-render

Root.PreRender1003A760 calls InitializeWindows1003BC80 at1003A9E0. It walks when
initCount>0,then requires initCount==0. InvokeWindowReady1003BD20 visits locally
visible children BEFORE self. For each not-initialized node,it calls checked
most-derived WindowReady,THEN sets bInitialized,THEN decrements root.initCount.
The relevant actual stores are1003BD81/1003BD8C/1003BD96. The caller and traversal
are separately verified in ignored core-getenum-next-window-ready-static-
20261027.txt and direct static1003A9E0 inspection. This corrects the older
ROOT-WINDOW-CONTRACT.md statement that attributed readiness to Root.Tick.

## Input remains a distinct initialized transaction/platform service

Current Input validation and snapshots are constructor-only:initialized=true or
a non-None native Input.Viewport is rejected. RuntimeNativeInputInstance lacks
held Keys[255],Action/Delta and a transactional beforeimage. Add real initialized
state/GC/accounting/undo before admitting stateful input,not a flag-only bypass.

- UInput.Init10301460 ->10393D30 stores the actual Viewport,invokes ResetInput,
  then logs its name. Initial embedding Init call order is still unaudited.
- Reset10302A45 ->10394910 clears255 held key bytes,then inherited input
  ByteProperty and FloatProperty storage (one byte/DWORD per actual offset,
  not ArrayDim or packed-Bool loops),Action/Delta,and finally calls concrete
  viewport.UpdateInput(1). Base viewport+7C is purecall,not an empty service.
- InputExt.PreProcess1001C680 genuinely returns1. Process1002D0B0 is not base
  Process:offer input to actual Root1003B3E0; when consumed,clear held keys/run
  nonempty release bindings. Otherwise preserve duplicate suppression,key
  changes,Action/Delta,own command execution and accumulator-reset ordering.
- ReadInput10394580 gates on actual GIsRunning,caches inherited input-property
  references,uses InputExt.Process(action2,rawdelta) for held keys when ordered
  and not-1,then scales only Float inputs with PC64 20.0/delta or positive zero
  for-1/unordered. Post-input scaling must remain real.

Attach bounded VR samples at concrete viewport.UpdateInput and consume once in
pre-input; post-input-1 must not repoll. Reset/tracking loss releases held state.
Reuse real viewport/session/generation authority. Joy axes and Joy1 Fire in the
owned config do not by themselves define VR packet units or process commands.
Preserve quest_player_local_tick.h pre-input/PlayerInput/PlayerTick/post-input
order. LoadMap's known Reset after SpawnPlayActor/SetPlayer/Possess1038DFF3 must
not move before HUD/key queries. ResetKeyboard544 is separate config reload,
not held-key clearing. See ORIGINAL-TRAINING-CONFIGURED-INPUT-GAP.md and
ORIGINAL-VIEWPORT-INPUT.md.

When the original live player pipeline is activated,retire the competing
presentation locomotion/fire path instead of applying input or physics twice.
The required committing weapon/frame/actor publication bridge remains in
TRAINING-FIRST-PRIORITY.md. Immediate integration remains first Training only.
