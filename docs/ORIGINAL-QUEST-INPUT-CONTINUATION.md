# Next Quest input continuation (2026-11-01)

Read-only source/integration audit prepared while compiler inputs were frozen.
Only this document is added. This is NOT an implementation of platform input,
Root handlers, initialized Input, ReadInput, playable Training or a new APK.
No original commercial DLL/EXE is loaded or executed. Immediate integration
remains the first `00_Training` map, not other-map work or account screens.

## Current concrete boundary

`portable_input_initialize_host.inl` reaches the actual Init reset prefix after
genuine SpawnPlayActor/SetPlayer/Possess, then calls its required platform
UpdateInput(1). The action-command diagnostic exercises real qualified Pawn
cells at that authority; it is not physical event delivery. After staging the
actual bound Quest source, the retained continuation still refuses with:

`Original Quest viewport UpdateInput dispatch/reconciliation is not prepared`

`quest_input_process.h` supplies exact typed InputExt.Process and Root.Process
coordinators, but they currently have no authenticated runtime consumer.
`quest_input_source.h` supplies bounded non-consuming observations, not events.
The missing callback must connect those two services to actual native owners,
current binding execution and transactional cursor/baseline publication.

## Source authority

Owned Extension.dll SHA256:
`93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C`.
The original preferred image base is10000000. These are source addresses, not
portable object offsets. External MinGW objdump reads the PE as data only.

| Original exported service | Body |
| --- | --- |
| XInputExt.Process | 1002D0B0 |
| XRootWindow.Process | 1003B3E0 |
| XRootWindow.HandleMouse | 1003B670..1003B6E1 |
| XRootWindow.HandleKeyboard | 1003B720..1003B817 |
| XRootWindow.HandleVirtualKeyboard | 1003B820..1003B951 |
| XRootWindow.HandleButtons | 1003B960..1003BC6F |

Root Process semantics are implemented in `quest_input_process.h`: keyDownMap
stores precede grab checks; a zero handleMouseRef/handleKeyboardRef genuinely
declines; a selected mouse grab clears the actual parentPawn Fire/AltFire;
duplicate releases suppress the selected handler, but a selected grab still
consumes. InputExt offers the actual Root first, releases gameplay-held keys
after consumption, and otherwise retains duplicate suppression and current
Action/Delta/binding execution order. These coordinators do not authenticate
the native Input, Pawn, Root or output device themselves.

## Actual Root handlers, not blanket script false

HandleVirtualKeyboard starts with the actual root FocusWindow(+5C8). If None,
it calls GetTopChild(true), then GetLowerSibling(true) until native
windowType>=2, otherwise uses the Root itself. These select the last locally
visible child and previous locally visible siblings, NOT firstChild/nextSibling.
The original calls at1003B857 and1003B86B target1004C6B0 and1004C650;
quest_native_window_navigation.h independently preserves that last/prev order.
For each selected window:

1. When not duplicatePress, call actual virtual+BC
   RawKeyPressed(key,action,0). Preserve its result.
2. If unhandled, perform the original destruction assertion/check before the
   next optional callback; duplicatePress does not remove that check.
3. If still unhandled and action==1, call actual virtual+CC
   VirtualKeyPressed(key,duplicatePress).
4. If still unhandled, perform the second original destruction assertion/check
   and read current winParent(+138) AFTER callbacks before continuing.

HandleKeyboard is a DISTINCT character-input path, not a replacement for that
virtual-key protocol. It uses virtual+C8 KeyPressed(character), checks actual
Alt state through IsKeyDown where the source does, and walks current parents.
The raw Quest observation source has no character/text field. Do not invent
character events from arbitrary controller button numbers or claim a virtual
keyboard/text-entry producer that does not exist.

HandleButtons first respects the actual bMouseButtonLocked state. Otherwise it
selects the current visible modal/root context and calls genuine GetMouseWindow
1003B0D0. Press invokes actual GrabMouse1004CAC0 and conditionally
SetFocusWindow1004F0E0 according to the context's focus mode; release invokes
UngrabMouse1004CB60. It uses actual GetTickOffset1004FDA0, mouse positions,
last-button identity/window, timeout, distance and click-count state.

The subsequent parent walk captures that iteration's parent BEFORE its
callbacks (1003BB39), unlike HandleVirtualKeyboard's after-callback reread.
ConvertCoordinates1004FBE0 produces target-local coordinates. The handler
offers actual RawMouseButtonPressed first, then action1 MouseButtonPressed or
action3 MouseButtonReleased with the calculated click count. A handled result
stops propagation. Do not replace hit-testing, coordinate conversion, focus,
grab or click state with a constant false/true callback.

The actual XWindow primary vtable is10059930. Its source cells independently
match these exported implementations:

| Slot | Actual virtual | Exported body |
| --- | --- | --- |
| +B8 | RawMouseButtonPressed(Float,Float,Key,Action) | 10003700 |
| +BC | RawKeyPressed(Key,Action,Int) | 10003750 |
| +C0 | MouseButtonPressed(Float,Float,Key,Int) | 10003790 |
| +C4 | MouseButtonReleased(Float,Float,Key,Int) | 100037E0 |
| +C8 | KeyPressed(Word) | 10003830 |
| +CC | VirtualKeyPressed(Key,Int) | 10003A40 |

Owned Extension.u inspection shows Window.RawKeyPressed and
Window.RawMouseButtonPressed are genuine four-logical-byte false-returning
base scripts. This does NOT authorize false for every most-derived event.
Use the actual inherited/overridden event selection and supplied arguments.

## Navigation dependency and current implementation

The six owned Extension.Window declarations are native, flags401, zero input
parameters, with ObjectProperty ReturnValue dimension1/flags580/ref Window:

| Native | Declaration |
| --- | --- |
| 1442 | MoveFocusLeft |
| 1443 | MoveFocusRight |
| 1444 | MoveFocusUp |
| 1445 | MoveFocusDown |
| 1446 | MoveTabGroupNext |
| 1447 | MoveTabGroupPrev |

All six are absent from the current runtime native dispatcher and copied-input
wrapper inventory. RootWindow.VirtualKeyPressed calls1442/1443/1444/1445 for
Left37/Right39/Up38/Down40; Tab9 selects1447 with Shift16, otherwise1446.
Their native focus/tab traversal and callback bodies still need their own
source-authenticated implementations; returning None is not one.

Native1470 Window.IsKeyDown IS implemented against the actual terminal Root's
live keyDownMap. It supplies Shift/Ctrl/Alt queries, not the missing navigation
or Root event handlers. DeusExRootWindow.VirtualKeyPressed additionally uses
Ctrl17/Alt18/Shift16 and invokes original menu/DataVault/SHOT behavior. Its
most-derived script must not be silently replaced by the base Window script.

## Minimal honest production connection

1. Introduce one explicit versioned controller-to-original-input mapping. The
   source publishes normalized sticks, trigger/grip scalars, A/B/X/Y/Menu/stick
   buttons and metre/quaternion poses. It publishes neither Unreal axis units
   nor WinDrv packets. Mapping/scales, activation thresholds, handedness and
   focus/mount/head/individual-hand gates are a port adaptation, not a claim
   to have reproduced Windows device units. Preserve accepted VR control
   behavior, including snap-turn/recenter; do not also apply analog PC turning.
2. Authenticate the actual same client lifetime, viewport/session identity,
   configured native Input, lookup generation and current reciprocal Pawn
   before staging. Use the existing exact-token Quest source binding. Separate
   constructor and genuinely completed initialized-state authority rather than
   weakening the constructor-only public validator or setting a flag early.
3. Keep PRIOR PHYSICAL mapped held state in the native Input COW payload,
   distinct from Input.Keys. InputExt legitimately clears Input.Keys when a
   Root consumes an event. Comparing the next physical sample only against
   Input.Keys would invent repeated presses for a still-held trigger/grip.
   Cursor alone cannot reconstruct that prior state after consumed history.
4. Stage immutable source rows, map real physical transitions and dispatch
   through authenticated InputExt.Process -> actual Root.Process -> current
   binding ExecInputCommands/output. Bind real selected Root handlers; a
   reached unsupported handler must refuse, never manufacture neutral success.
   Continuous axes need explicitly documented units/scales and later actual
   ReadInput handling, not direct writes that bypass the original input path.
5. Reset accepts the newest valid physical baseline, never claims to replay
   overwritten history. Reconcile held/released states through the real
   protocol. Focus/mount/head/hand tracking loss releases previously admitted
   corresponding actions, rather than retaining a stuck Fire or fabricating a
   successful missing-provider zero sample. Rejection/overflow requires an
   explicit reset; the last rejected physical frame cannot expose a stale
   observation as a fresh valid reset baseline.
6. Stage the cursor, prior physical baseline, mapping identity and actual
   native/script mutations in the SAME outer root. Publish only with commit.
   Failure restores Input.Keys/Action/Delta/cursor/baseline, actual Root key map,
   Pawn fields and all native ownership. Hardware polling itself is not undone.
   Recheck source/client lifetime at publication, and clear transient authority
   on teardown/map replacement rather than serializing it into actor saves.
7. Connect UViewport.ReadInput with its original pre/post ordering. Ordered
   delta other than-1 polls UpdateInput(0); post-input-1/unordered does not
   repoll. Actual Input.ReadInput independently checks process-running
   authority, calls configured Process for held keys, then scales actual input
   Float declarations. Reuse the shared actual-property cache; do not replace
   original scaling with assignments or directly invoke Fire.

The existing presentation locomotion/fire route must hand over ownership when
the original committing player pipeline is enabled; it must not run alongside
it and apply motion/fire/physics twice. Poses additionally need the correct
reference-space/recenter/world transform before original aim consumers use
them. The current raw handoff alone supplies neither transformation authority
nor an original player-frame consumer.

Required evidence remains generated transition/reset/failure controls, actual
first-Training native-owner/Root/binding checks with full rollback, and physical
Quest acceptance of the real producer and release behavior. Neutral fixtures,
compiled coordinators and this static audit are not that acceptance.
