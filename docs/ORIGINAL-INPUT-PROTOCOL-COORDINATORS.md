# Original Input reset and event coordinators (2026-11-01)

This is a partial native-service implementation, NOT completed Quest input,
Input.Init/ResetInput, ReadInput, startup, renderer dynamics, crate gameplay or
a new APK. The runtime diagnostic intentionally refuses the still-missing
concrete Quest UpdateInput dispatch/reconciliation after recording the real
reset prefix. A staged physical observation is not event processing.

## Exact shared coordinators

`quest_input_initialize.h` follows original UInput.Init10393D30 and
ResetInput10394910..10394A7E: actual Viewport pointer store, reset, then actual
viewport-name log. Reset clears255 key bytes ascending, walks actual inherited
ByteProperty declarations with flags bit4, separately rereads Actor.Class and
walks FloatProperty declarations, clears Action/Delta and calls the concrete
viewport.UpdateInput(1). Selected writes reread Input.Viewport.Actor; each
declaration receives ONE byte/DWORD store at index0, not an ArrayDim loop,
Bool/Int clearing or leaf-name substitution. Children/Next/Super are actual
declaring-owner metadata. Repeated Init has no invented idempotent shortcut.

Every selected callback must exist. Work joins the caller's root, with one
cumulative tightened traversal bound and admitted simultaneous lexical frames.
An exception preserves original prefix writes for the OUTER journal; helpers
do not pretend to internally undo partially completed native calls.

`quest_input_process.h` follows Extension.InputExt.Process1002D0B0 and
RootWindow.Process1003B3E0. InputExt offers the actual PlayerPawnExt/root first.
Consumed events clear held keys ascending and run nonempty release bindings
with Action3/Delta+0. Otherwise duplicate presses/releases suppress dispatch,
held bytes change before binding selection, and current Action/raw Delta wrap
the selected ExecInputCommands call. Accumulators clear after normal return,
NOT on a fabricated exception-success path. Empty binding can return false
after a real held-byte change. Float raw bits are not sanitized/clamped here.

Root processing writes its actual keyDownMap before selecting mouse/keyboard
grabs. Mouse keys are1,2,4,228..231,236,237. A zero grab legitimately declines;
a selected mouse grab clears actual parent Fire/AltFire and dispatches buttons
unless duplicate-release. A selected keyboard grab dispatches virtual keyboard
with duplicate-press state unless duplicate-release. Missing selected native
handlers refuse; no default false result or direct Fire substitute is added.
These two Process coordinators currently have generated fixtures, NOT an
authenticated runtime Root-handler/command or Quest-mapping consumer.

## Actual first-Training reset-prefix connection

The opt-in `--observe-initial-input-reset-prefix` startup diagnostic invokes
the initial GameEngine Input.Init primitive AFTER genuine SpawnPlayActor and
its real tail, with actual configured Input/Console/Pawn/Viewport ownership.
This is an explicit rollback-only primitive diagnostic, not Init inserted at
the later LoadMap.ResetInput site. Normal startup leaves the option false and
keeps its existing later guard. Initial order is Spawn -> Init -> OpenWindow;
later LoadMap resets an already initialized Input. These remain distinct.

RuntimeNativeInputInstance now owns255 held-key bytes, Action, Delta and a
source cursor. They are fixed instance protocol storage, not CDO/config/Actor
exports or serialized platform identities. Existing full-instance COW and its
owner/viewport/Enum/native-Viewport GC roots retain their earliest beforeimage;
fixed state joins existing byte accounting. Public constructor snapshots expose
detached protocol observations for restoration checks without admitting partial
initialization. Constructor-only public authentication is NOT weakened.

The diagnostic sets explicit stress key/accumulator values in that real COW
instance, then executes actual pointer/key/property/accumulator writes. Those
stress values are test inputs, not claims about original constructor state.
The actual current Class/Children/Next/Super, property Class ancestry, flags,
declaring ownership and qualified VM references authenticate reflected writes.
The first owned run exposed an unresolved Core.ByteProperty Class: fifteen
Float writes reached the platform boundary while all thirteen Pawn Byte inputs
were silently skipped. That run FAILED its own assertion and is retained as
failure evidence. The correction registers the genuine indexed ByteProperty
Class and refuses unresolved/foreign property-Class identities instead of
treating them as a negative IsA. The independent first-Training oracle requires
EXACTLY13 Byte and15 Float stores, checks each actual qualified field after its
store, and prints observations before testing the aggregate assertion. See
ORIGINAL-BYTE-PROPERTY-REGISTRATION.md for native registration/source inventory.
The concrete platform continuation requires the exact current bound Quest
source and a valid published reset baseline. Batch scratch is charged. Missing,
empty or rejected-baseline sources refuse rather than inventing zero input.

Even a valid source does NOT complete the dispatcher. After authenticating it,
the current host explicitly throws:

`Original Quest viewport UpdateInput dispatch/reconciliation is not prepared`

It does not advance the source cursor, publish initialized=true or emit the
success log. The whole original startup root must restore native Viewport,
keys, Action/Delta/cursor, reflected actor fields, GC/lookup/client/hash/RNG,
world revision and full saved state. Native aliases/bindings/extension names,
retained bytes, object/class/viewport/Enum indices and the naming counter must
also agree with their constructor snapshot after rollback. Owned CLI binding is an EXPLICIT test
QuestInputSource with a published observation, not a real headset capture.

## Remaining implementation

Actual Quest key/axis mapping and reset/release reconciliation, authenticated
Root HandleButtons/HandleVirtualKeyboard, ExecInputCommands/ParseLine and
action-sensitive viewport/Input FExec routing, ReadInput scaling/held processing,
native render/audio services and the committing original player frame remain
required. ExecInputCommands must respect genuine GIsRunning and ParseLine256/
Exact0; false running is not permission to force that branch for tests.

Build/test outcomes belong in STATUS.md. A compiled helper or generated callback
fixture is not proof of real native owners, platform input or playable Training.

The native1470 Window.IsKeyDown implementation supplies a real keyboard
dependency, not the missing Root handlers themselves. Its actual UFunction is
native1470/flags401 with Key Byte,
dimension1/flags80/enum Extension.ExtensionObject.EInputKey, and Bool return
dimension1/flags580. Source1004C980 obtains the actual terminal GetRoot and reads
that root's keyDownMap[Key] within0..254; only genuine absent Root/out-of-range
returns zero. It is not a snapshot of Input.Keys or a nearest/cached-root guess.
The concrete wrapper100535F0 seeds its byte argument0, evaluates once and masks
to0xFF. The shared VM's native-input snapshot route preserves lvalue/NoWrite/
ZeroFill ingress rather than requiring only detached test values. Actual
configured Root/child controls exercise live keyDownMap writes and complete
rollback; generated callbacks alone are not owner authority.
Root.VirtualKeyPressed calls navigation natives1442..1447 and modifier query1470;
DeusExRootWindow adds actual menu/DataVault/screenshot behavior. Apart from the
implemented modifier query, these services and raw/virtual/mouse parent event
dispatch remain unimplemented. Base Window
event scripts returning false do not authorize blanket false for derived events.
