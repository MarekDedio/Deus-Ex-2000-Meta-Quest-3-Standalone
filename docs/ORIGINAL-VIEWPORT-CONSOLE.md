# Original viewport Console construction and Init

Read-only/static audit2026-10-28. This contract is NOT implemented by the
current Canvas batch and does not prove successful original startup, a working
Console/FExec provider, platform OpenWindow or initialized Input. No original
commercial DLL or EXE was loaded or executed. Source/compiler inputs stayed
frozen during this audit; only this documentation file was added.

## Configuration, class identity and construction order

The owned `System/DeusEx.ini` selects `[Engine.Engine] Console=Engine.Console`.
GameEngine.Init103896CE uses the actual literal `ini:Engine.Engine.Console`
at1047E6B4 and constrains the selected class to UConsole105823E0. This is not
permission to invent a fallback class when configuration or class authority is
missing.

The existing `NativeConfigConsole` helper in portable_unreal_runtime.cpp
authenticates the serialized Engine.Console UClass: Super=Core.Object,
ClassFlags0x13A, UObject export flags0x040F0004, config nameSystem and actual
serialized Within=Core.Object. **Within is NOT Engine.Viewport.** Viewport is a
later native/reflected pointer, not the Console object's Outer.

At103896E7..103896FA, Console construction passes requested flags0, NameNone,
Outer=-1 and the configured actual class to helper10301609 ->10392F50. The
helper verifies the class derives from UConsole, substitutes GetTransientPackage
for Outer=-1 at10392F8A and forwards to StaticConstructObject with null template.
Static allocation must apply the selected class's Transient bit0x8 from0x13A,
so the actual object carries RF_Transient0x4000 even though requested flags are0.
Authenticate that distinction as done for the existing native Input constructor;
do not replace the requested flags by a fabricated persistent object header.

Construction returns an actual non-Actor UObject. GameEngine stores it in
viewport+34 at10389702, then calls its virtual+54 Init(actual viewport) at
10389705..0A. Only AFTER Console.Init does GameEngine call platform OpenWindow
at1038972F, followed by Input.Init at1038975F..65. Viewport's earlier native
constructor already constructed/initialized configured Canvas and constructed
configured Input, in that order; see ORIGINAL-VIEWPORT-CANVAS.md. Console setup
must not silently run OpenWindow, set RenDev, reset/poll Input or bypass these
actual phase boundaries.

## Native constructor preserves copied data

UConsole.InternalConstructor1030127B ->1034E280 calls constructor thunk10302900
->10379430 for nonnull storage. The constructor first calls imported
UObject::UObject at1037944F. That Core constructor101021C6 ->1014F340 installs
the base vtable only; static allocation/default copying remains authoritative
for object header and reflected values.

UConsole constructor10379430..103795DE establishes native typed members and
finally installs the UObject vtable104297C0 at+0 and the FOutputDevice
vtable104297BC at+28. These are native provider identities, NOT portable32-bit
script addresses. The reflected const IntProperty `Console.vtblOut` must not
expose a guessed numeric pointer.

Do not clear the copied default strings or names:

- At1037946A and for private/string members at774/780/78C/798, the constructor
  uses103022C5 ->10313DA0, whose body is `mov eax,ecx; ret4`: NoInit, no store.
- History[16] at+48 and MsgText[64] at+120 are initialized by the array
  constructor helper104135D0 using the imported NoInit-string constructor.
- MsgType[64] at+420 calls imported default FName10599B54, Core10102383 ->
  101099E0. Its body is `mov eax,ecx; ret`, NOT a NAME_None assignment.
- String members+7A4,+7B0,+7BC,+7C8,+7D4,+7E0,+7EC,+7F8,+804 call imported
  FString(ENoInit)10599B84 ->Core10102491 ->10107440, also no store.
- Imported FStringNoInit default construction similarly resolves through
  Core101031CA ->10109130 and is a genuine no-op.

The shared owned-package decoder audit in ignored
`artifacts/viewport-class-audit-20261028.log` lists57 original Console
properties. Including History16 and four64-element arrays, these represent324
actual property slots before inherited header fields. Relevant fields include
Viewport, TypedStr, History, MsgText, MsgType, MsgPlayer[64], MsgTick, Border,
ConBackground, TimeDemoFont, localized text strings and const private vtblOut.
Reflected default storage, string contents and property-edge identity must be
preserved rather than reconstructed from an assumed zero native memory block.

## Init and the actual state frame

Console.Init thunk103029CD ->10379650 first checks the native Console class
properties size is0x810 at10379684..8C. It then performs these actual stores:

1. Console+2C = actual supplied viewport at103796A8.
2. DWORD+110 =0x3F at103796AD (the original TopLine value63).
3. BorderSize(+720) =1 at103796B7.
4. virtual+38 at103796C1.

The+720 field is the actual `Engine.Console.BorderSize` IntProperty, NOT a Bool.
This is proven from the owned serialized Field.Children/Next chain, not export
order: MsgTime -> MsgTickTime -> MsgText[64] -> MsgType[64] -> MsgPlayer[64] ->
MsgTick[64] -> BorderSize -> ConsoleLines -> BorderLines -> BorderPixels ->
ConsolePos -> ConsoleDest -> FrameX -> FrameY -> ConBackground -> Border ->
the packed Bool fields. Console.Children is reference299; BorderSize is5294
and its actual Next is438(ConsoleLines).

Core.UStruct.Link101260CF..10126116 walks Children(+38)/Next(+2C), calls the
property Link virtual+68 and advances PropertiesSize by Offset+ElementSize*Dim.
UInt.Link10165C90..A5 establishes size4/align4. The native constructor anchors
MsgText at+120 with64 twelve-byte strings and MsgType at+420 with64 four-byte
Names. The actual chain therefore places MsgPlayer+520, MsgTick+620 and
BorderSize+720. Console.PreRender independently reads+720 as signed Int at
10379DC9, compares>=2 and decrements it before scaling the border.

The actual Bool word is+748: bNoStuff01,bTyping02,bNoDrawWorld04,bTimeDemo08,
bStartTimeDemo10,bRestartTimeDemo20,bSaveTimeDemoToFile40. UBool.Link
101660C6..10166105 reuses a prior Bool's offset with the shifted mask, or starts
a new aligned DWORD with mask1. GetDrawWorld10379CA0..AE independently reads
~DWORD(+748)>>2&1, and IsTimeDemo1034E8F0..FC reads DWORD(+748)>>3&1. The
static disassembly is retained in ignored
artifacts/console-layout-static-evidence-20261028.log. No native address or
inferred Bool state should replace the real BorderSize field store.

Console UObject vtable104297C0+38 contains1040C710 ->IAT10599AD0, whose actual
import is UObject.InitExecution. Core thunk1010286F ->10150750 implements:

- Require actual Class atObject+24; free an existing StateFrame atObject+0C.
- Allocate a real40-byte FStateFrame at101507B7.
- Set Node(+4)=Class, Object(+8)=Self, Code(+0C)=None, Locals(+10)=None,
  PreviousFrame(+14)=None and StateNode(+18)=Class.
- Set ProbeMask(+1C/+20)=all ones, store the new frame atObject+0C, then OR
  RF_HasStack0x02000000 into ObjectFlags at10150804..10.

The frame's native latent slot+24 is NOT written by this constructor. The
existing portable Actor helper explicitly uses Continue as a defined policy
while Code is null, not proof that the original stored latent zero. Preserve
that caveat and do not manufacture a runnable continuation from undefined data.

InitExecution does NOT select an auto state, call BeginState, invoke a script
initialization event or execute console commands. Owned Engine.Console.Typing
exists with state flags0 (not auto), and no class-level Console.BeginState export
was found by the port's package inspector. The initial selected/running node is
the actual Console class, codePresent=false, empty locals and all-one probes.
With the original transient allocation, observed post-Init flags should be
0x02004000; this must be verified from the actual portable object header rather
than patched into a detached snapshot.

## Existing implementation seams

The Canvas constructor/API is the closest current lifecycle pattern:

- `portable_viewport_canvas_host.inl`: ConstructNativeViewportCanvas,
  RequireNativeViewportCanvas and CaptureNativeViewportCanvas.
- `portable_viewport_canvas_api.inl` and `portable_viewport_canvas.h`: explicit
  configured preparation, idempotent detached reads and rollback-only mode.
- RuntimeObject nativeCanvasPeer/nativeCanvasObject and Mark establish actual
  viewport/Canvas owning edges. Add a parallel viewport.Console owner edge;
  keep the real mutable Console.Viewport property distinct from bookkeeping
  about the viewport that originally constructed/owns the object. Console's
  Viewport flags are0, unlike const Canvas.Viewport flags2.
- RetireRuntimeCanvasObject, RetireNativePlayerObjects, weak
  persistentNativeCanvases inventory, provisionalCanvases_, Before/Touch and
  transaction Commit/Rollback provide the non-Actor naming/index/lifetime seam.
- Reflected Object-property GC collection currently recognizes Window/Canvas
  in ReferenceFor. Extend the real service for Console fields, including every
  MsgPlayer[64] element, Border, ConBackground and TimeDemoFont. A weak inventory
  is accounting/validation, not a substitute GC root.
- HasStateFrame, CurrentDispatchContext and the existing state journal already
  operate on authored Class identities. Touch captures state/flags/revisions;
  ValidateStateBudget measures old-state copies. Its current live traversal
  excludes weak Console objects: the new Console ledger MUST also feed each
  live StateObject into the shared measured writer so frame/control/locals,
  value/count limits and retained storage remain aggregate, not per-object.
  InitializeActorExecution itself has Actor Level/native-XLevel requirements:
  do NOT call it on Console or weaken those Actor checks. Factor only the
  genuinely shared Core.InitExecution part if reusable construction is needed.
- Original tagged Object/Class defaults retain every array element through
  cls/base references, but copied classDefaultValues/frozenBirthDefaults are
  symbolic Values, not GC roots. Construct and meter a real per-property/index
  edge graph for effective copied Object overlays, as well as future stores.
  Read all64 MsgPlayer slots through indexed property authority; the scalar
  objectPropertyPaths cache cannot decode an array. Clearing one slot must not
  retire another slot's edge. A live-state byte ledger alone does not prove
  future Object-valued locals have GC ownership.
- Console must not be appended to runtime Actor exports/world actor slots or
  map saves. Native-client identity already has explicit script-save refusal;
  native Console lifecycle must preserve that transient exclusion.

An appropriately explicit next API is
`PreparePortableRuntimeViewportConsole(viewportIdentity, engineIni, commit,
limits)` plus a detached `ReadPortableRuntimeViewportConsole`. Its name and
snapshot should certify constructor/Init only. Require configured actual class,
current viewport/Canvas/Input constructor authority and the original phase
order; refuse unaudited native subclass overrides instead of treating them as
Engine.Console merely because they derive from it.

## Bounded implementation acceptance plan

Before claiming this service is implemented, verify with original owned data:

- Actual configured Class/Super/Within, Transient Outer, NameNone-derived unique
  name, final header flags, property schema/default copies and distinct indices.
- All324 property slots retain their genuine copied/default state except the
  proven Init stores. NoNoInit string/name reset, raw vtable/address exposure,
  Canvas.Reset/Update, platform OpenWindow or Input.Init side effect occurs.
- The actual state frame has Class Node/StateNode, null code/locals/previous,
  all-one probe mask and HasStack header, with no premature Typing/BeginState.
- All native/header/property/frame stores share the caller's root write/work
  allowance. Admit constructor, strings/arrays, GC nodes, indexes, snapshot,
  old state and transient allocations BEFORE allocation/copy; no budget raises.
- Forced write/work/retained-byte failures at constructor/Init/frame/publication
  boundaries restore both peer edges, reflected values/roots, frame/flags,
  unique-name counter, namespace/object-index publication, whole map save,
  revision and GC object/memory totals. Repeated rollback reproduces exactly.
- Successful repeated prepare/read does not construct a second object or rerun
  InitExecution. GC retains Console only through real ownership/property edges;
  map change preserves it with the viewport, and client teardown clears/unindexes
  it before object retirement without stale peer or frame authority.

Even after these checks, Console ProcessConsoleExec, GIsScriptable/current-state
Exec lookup, RenDev/OpenWindow, Input.Init/Reset/ReadInput and the complete real
FExec provider route remain separate required services. Do not turn a successful
Console pointer/frame preparation into unconditional SET decline or successful
player startup. Immediate integration remains first Training only.
