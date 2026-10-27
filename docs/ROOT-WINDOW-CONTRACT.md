# Original native root-window bridge (2026-10-14)

This is a required player/UI prerequisite, not completed HUD or possession.
All original DLL inspection was static/read-only; no original executable or
DLL was run. Commercial classes/textures stay in the owned installation.

## Retail allocation and callback order

Extension.dll execInitRootWindow1052 at10038560 calls InitRootWindow10037df0.
An existing Pawn.RootWindow at+894 returns without reinitialization. Otherwise
ConstructRootWindow10037e70 uses StaticLoadClass constrained to Extension.RootWindow
and the literal ini:Engine.Engine.Root at10098a30. The owned INI selects
DeusEx.DeusExRootWindow. StaticConstructObject at10037ed3 uses actual Pawn Outer,
requested NameNone, flags0, null template. These are not the pinned reference's
transient-package/dxRootWindow/Transient-flags choices.

Window native Init1004bb30 creates two real Extension.GC objects, each Outer=window,
NameNone/flags0. Root Init10039050 runs base Window/TabGroup/Modal initialization,
sets windowType3, establishes parentPawn and reciprocal Pawn.RootWindow, adds an
explicit native GC root and the global root-list link. Core AddToRoot10157400
only appends a pointer; it does not set a reflected root flag.

ProcessScript1004ded0 uses FindFunctionChecked/ProcessEvent for most-derived
InitWindow. DeusExRootWindow.InitWindow calls Extension.RootWindow.InitWindow,
whose empty Window.InitWindow base returns; SetFont1477 at PC6 and cursor
natives1454/1510/1511 now execute. Root subsequently needs NewChild1410
for ActorDisplayWindow, DeusExHUD and scope. There is no DeusExRootWindow.InitHUD.
The real HUD script constructs its health/belt/compass and other child graph.

After successful InitWindow, native SetVisibility(true) at10037f0a is required.
It is not a single Bool store: SetVisibility1004c7d0/internal1004ee20 recomputes
the init tree, invokes most-derived VisibilityChanged, plays optional sound,
reconfigures and negotiates focus. WindowReady/init completion belongs to later
Root.PreRender, NOT activation or Root.Tick (2026-10-27 static audit:
1003A760 ->1003A9E0 ->InitializeWindows1003BC80 ->InvokeWindowReady1003BD20).
The current bridge explicitly refuses this phase until implemented;
the callback now completes ActorDisplayWindow and ammo construction, then enters
the health display's first body-part window. It refuses native1475 at original
Extension.Window.SetTileColorRGB PC51. No partial root or
possessed player is published as a successful result.

## Native initial state and boundaries

Retail Window Init starts hidden/sensitive/not-selectable/not-being-destroyed,
Normal backgroundStyle1, green text RGB and white tile RGB, position0,
size10, margins0, hardcoded size-1, alignments0. Text/tile Plane W is0.
XGC constructor10026ad0 starts CanvasNone, zero clip rectangle, Style1,
PolyFlags900/textPolyFlags802, enabled drawing, disabled smoothing/masking/
translucency/modulation, word wrap/metachar parsing enabled, fontsNone,
underlineHeight1/baselineOffset2/textVSpacing1, multipliers1, gcCount1 and
null stack/free/owner. The original underline texture default remains.
Retail writes only Color RGB, not a deterministic A: the port preserves copied
CDO A as an explicit defined storage policy, not original coverage evidence.
No Canvas or graphics backend is fabricated.

Current code introduces a separate native window-domain identity/root owner,
typed constructor writes and rollback-owned provisional objects. These are
not Actor births, exports, collision bodies or save-manifest Actor records.
Native Pawn.RootWindow is an authoritative transient pointer view, not a saved
string patch. Concrete UClass naming counters are journaled and use retail
MakeUniqueObjectName101575b0: strip trailing digits, consume a class-wide counter,
truncate to62 UTF16 units and retry any-class collisions in the actual Outer.
Counters survive map changes and revert on a refused portable transaction.

General EX_New, NanoKeyInfo object-graph saves, non-root heap GC, complete native
Window/TabGroup/Modal constructor/destructor/backend details and accepted root
visibility/window Tick are still required. Original unbound native1052 and
isolated SetPlayer/Possess reach the first health body-part color call,
with a pending18-object graph. Failure restores root/child/GC indexes, native root list, class
counters, Pawn.RootWindow, complete map save, revision and GC counts/memory.
Repeated original callback and forced shared-write-budget controls pass, along
with isolated CreateKeyRing/session/save tests. This does not certify the
remaining operations or successful possession.

## Font, cursor and reflected GC edges

SetFont1477 exec10053b90 calls SetFont1004d840/SetFonts1004d7a0. It assigns
Window.normalFont/boldFont and only wGc's matching fonts, leaving gGc alone.
AskParentForReconfigure1004e060 marks Self.bNeedsReconfigure/bNeedsQuery. Any
hidden Self/winParent ancestor defers layout without callbacks/ancestor writes;
all-visible reconfiguration remains explicitly unsupported.

Cursor1454 exec10052c20 initializes texture/shadow None, hotspots-1 and RGB
white; evaluates all five arguments sequentially, then assigns all fields.
1510/1511 similarly assign their default-None cursor fields even when omitted.
Retail cursor alpha is uninitialized on omission; the portable temporary uses
defined A0 storage, not a claim about original alpha. Authored root calls omit
trailing parameters entirely, so missing-count and explicit EX_Nothing defaults
both matter. Declared types/native indexes/qualified source are checked.

The VM captures audited native argument-result seeds before any expressions.
NoWrite retains a hotspot's-1; full four-byte zero-fill produces0; audited
little-endian partial Byte/Int/Float writes preserve remaining bytes. Unknown
packed/identity/aggregate partial layouts still refuse. All input snapshots,
OUT aliases, expression order, capacity accounting and rollback stay bounded.

Native window Object-property edges now follow their actual qualified property
and array slot, including Object members inside Struct values. Replacing a
font/reference replaces that edge rather than accumulating old references.
The host journals these roots with values, and GC marks them. Copied CDO state
is budgeted before native window allocation; Class_Transient applies effective
RF_Transient in Core even though the requested construction flags are0.

A non-publishing original root probe captures the pending typed graph before
the first callback failure, then rolls it all back. It does not alter owned INI,
substitute a root script, call more VM operations after failure or publish a HUD.
This permits direct assertions of font/cursor/hidden flags and actual GC edges,
not merely relying on a later unsupported opcode as proof of correct values.
The owned prefix/session/ring controls pass1459 checks, including constructor
flags, clipping, preserved CDO fields, all255 key/accelerator slots and the
original HUD/ammo/health child binding, canonical Color zero-fill, requested
geometry and the actual head-texture GC style. Generated VM checks pass682125/528
refusals, including Color761/115, sequential seeds279/12 and334 additional
Window argument-copy/zero-seed checks.

## Native construction and the first original child

Base Window.Init1004bb30 establishes query/reconfiguration flags, raw-background
and special-text flags, original specified/query geometry, timers/index values,
cursor/sound/client defaults and two owned GCs. It preserves the CDO fields that
retail does not overwrite, including initialized/text-translucent flags,
Background, SoundVolume, held geometry, acceleratorKey, maxClicks and freeTimer.
After parent callbacks it initializes the exact six-Float clip and inherits both
fonts from the nearest current ancestor having either, updating wGc only.

Root native TabGroup/Modal setup now clears the private row/column lists, uses
sizeParentToChildren=true/sizeChildrenToParent=false, focusMode2, None preferred
focus,255 None accelerator slots, dirty accelerators and modal self-list/index0.
Native list pointers are journaled/GC-linked vectors, never pointer-shaped script
Int values. Reading those private DynamicArray layouts as script values refuses.
Root input/focus/render/cursor baseline and its255 Byte key map are initialized;
black raw RGB uses defined A0 where retail's alpha source is uninitialized.

NewChild1410's original NewClass parameter is a generic ClassProperty constrained
to Core.Object, not class<Window>. Its return is Window and optional bShow defaults
true. Portable construction still validates an actual supported Window class;
it does not execute an invalid native cast. Only pure native Window descendants
are implemented so far; other native subclass Init contracts explicitly refuse.
Window/TabGroup/Modal/Root parent notifications use optional dynamic FindFunction,
not the checked lookup used for InitWindow. Parent and all ancestor notifications
precede clip/font inheritance and the destruction lock. Live parent traversal is
bounded. Destruction during these unlocked notifications explicitly refuses.

The first original ActorDisplayWindow.InitWindow now executes maxPoints=8 and
all16 Sin187 entries. NewChild then follows lock/script/optional-show/reconfigure/
unlock ordering. Showing this child under the hidden Root uses the inherited
native TabGroup visibility request: only local visibility changes, without
VisibilityChanged, init-count, focus/grab, sound or layout callbacks. Its query/
reconfigure flags remain pending. Effective visibility changes and deferred
destruction still refuse; no accepted live root or completed HUD is claimed.

SetWindowAlignments1438 now implements the full finite-input/dependent-default
contract. Retail exec10052360 defaults hMargin1 from already-evaluated hMargin0,
and vMargin1 from vMargin0 before those later expressions. New opt-in sequential
VM hooks capture detached bounded prefixes, preserve explicit omissions and
merge no-write/full/partial scalar results against the actual seed. Existing
entry seeds still run before all arguments; ordinary calls allocate no prefix.
Seed callbacks/writes and caught failures poison shared execution.279 generated
sequential controls/12 refusals pass, including later lvalue mutation and OUT
alias preservation.1438 compares H,V,h0,h1,v0,v1, returns without writes if equal
(including signed zeros), otherwise stores H,h0,V,v0,h1,v1 then asks to reconfigure.
Original unordered-NaN C3 behavior is not claimed: portable ingress stays finite.

GetRootWindow1425 follows actual winParent links to the terminal node, returning
it for unsigned windowType>=3, not the first root-like ancestor. It performs no
callbacks/writes/player pairing. Malformed/cyclic graphs and unsafe casts refuse.
SetSensitivity1416 compares effective sensitivity independent of visibility;
with actual None focus/grab pointers it marks the nearest modal dirty and sends
optional most-derived SensitivityChanged, Self first then locally-sensitive
children through live links. Nonnull focus/grab negotiation, destruction and
unsupported native overrides remain explicit gaps. The original hidden HUD
sets local sensitivity false; its new ammo child remains locally sensitive but
inherits the actual player through root.parentPawn. Ammo construction has now
completed/unlocked and HUD.Ammo is genuinely assigned; HUD and health locks
remain1 while the first health part's color call is pending. Root.hud, HUD.hit
and Head.partWindow are not assigned early.

Hide1414 and SetSize1431 handlers are also supplied. Equal local Hide is a genuine
no-op. SetSize clamps
negative finite dimensions to0, preserves-0, compares requested hardcoded sizes,
writes both when changed, and defers geometry/clip changes to layout. Actual
original ammo95x77 and health84x106 requested dimensions are checked while their
current geometry remains10x10.

## Typed Color, visibility callbacks and first health artwork

Core.Object.Color has an explicit trusted result-layout capability, never
inferred from same-shaped values. Runtime Describe verifies the actual
Core.StructProperty target, actual Core.Struct/no base/no script and exactly
four owned Core.ByteProperty children R,G,B,A, dimension1/no enum. Original
Core.dll UStruct::Serialize10126fe0 calls Link10126060; UByteProperty::Link101656f0
places one-byte fields at consecutive offsets, final size4. execContext101300d0
zeros exactly its encoded result bytes; execLet1012fe40 supplies the actual LHS.
Thus StyleChanged's null-theme result legitimately clears RGBA, without creating
a ThemeManager. Locals, state locals, arrays, members, return/declared argument
destinations and guarded references retain capability. Unknown structs and
partial/overflow Color writes refuse. Caught guarded-write failures poison the
shared transaction rather than committing an earlier prefix.

SetVisibility1004c7d0 compares local state before dispatching the actual parent's
native vtable+84. Ordinary Window10003540 calls optional most-derived
ChildRequestedVisibilityChange. TabGroup/Modal/Root10045870 bypass script and
call the child's SetChildVisibility, then reconfigure the parent only on hide.
Native1467/100532e0 has a Bool0 temporary and calls1004ee20 directly, with no
invented parent relationship. If effective visibility stays unchanged, it has
no init/focus/layout/sound/accelerator effects. DeusExHUD's actual override also
runs ConfigurationChanged while HUD.Ammo is stillNone; the outer assignment is
not moved before this callback. Effective-visibility changes still refuse.

SetBackground1421/1004d5e0 compares actual Texture identity (not case-sensitive
handle spelling), writes only if different and asks to reconfigure. Style1422/
1004d670 always updates Window and wGc style, not gGc. Style0 disables drawing
while retaining modes/PolyFlags; nonzero styles enable drawing and set masked2,
translucent3 or modulated4 exclusively, then regenerate original PolyFlags while
preserving smoothing/text flags. Configure1430/1004cd40 clamps finite negative
dimensions, preserves signed-zero no-ops, writes near alignment/margins/requested
size, and never overwrites far margins or current geometry/clip. The original
first Head window uses texture HUDHitDisplay_Head, requested39,17,4,7, style3,
and wGc flags0x904 while gGc remains0x900. Other style modes/edge-case native
branches still need direct integration coverage; argument-copy tests do not
prove them. The non-publishing snapshot stays bounded at64 objects/16 roots/1MiB.

Next native1475's original wrapper10053ad0 does not initialize its Color result
temporary. Full written Color and full4 Context clearing are representable;
Nothing/no-write/zero0/partial bytes must not silently become declared zero.
That argument-destination contract and the setter remain unimplemented. The
audited setter copies full RGBA to Window/wGc and computes RGB planes with the
binary64 reciprocal1/255, W0; none is claimed working by the current refusal.

## Separate original startup prerequisites

Retail Engine.PlayerPawn.ConsoleCommand native0 returns an empty FString when
its actual native Player is None, after argument evaluation. It does not call
Actor.ConsoleCommand or execute a command. Engine.dll thunk10302e5a dispatches
to103ba200; the PlayerNone branch at103ba2c7 returns at103ba3af. Both authored
ConsoleCommand functions have nativeIndex0/flags400, so full declaration/source
and FString/const Engine.Player schema checks are required. A revoked native
lease cannot become a valid None case. Bound FExec remains unsupported; root
ConditionalBindMultiplayerKeys still genuinely needs KEYNAME/KEYBINDING/SET.

Audited FMax245 (Core10136400) and Clamp251 (Core10134be0) preserve sequential
input copies and retail signed/equal/reversed-bound rules. The startup probe
also exposed GameReplicationInfo.Region(Int) shadowing Actor.Region(PointRegion).
Native zone/birth writes now use exact original declaration identities. Hidden
instance overlays retain qualified keys; save/load validates those owned fields
and keeps both same-name slots without a wire-format change. This does not
extend hidden inherited class-default mutation, which remains unsupported.
The unmodified1308-actor Training probe now reaches BeginPlay slot126,
DeusExMover16,Engine.Mover.BeginPlay PC166/nativeMove266. That movement contract
remains unsupported. Exact repeat and shared-budget failures restore all
actor/script/birth/RNG/GC/save/revision state; no begun world is published.

## Lifetime and next window contracts

Retail root CleanUp10039240 unlinks root list100aa8f8, clears pawn.RootWindow only
if still paired, RemoveFromRoot101574b0, then Modal cleanup. Pawn native Destroy
10037d40 safely destroys its root; GameEngineExt.Destroy10026230 calls
DestroyAllWindows1003a330 before base destruction. Complete DestroyWindow,
children, hide/disable and destruction-lock semantics must be implemented
before any live initialized root can survive map travel/restoration.

NewChild1410/CreateNewWindow1004df70 also allocates Outer=parent, NameNone, flags0.
Retail XWindow.Init links parentOwner+2c and family winParent+138, then calls
parent.ChildAdded and parent plus every ancestor.DescendantAdded BEFORE script
child.InitWindow. The pinned engine fires afterward and excludes the parent.
Do not copy that sequence. The supported pure-Window/hidden-root path now follows
its ordinary later lock/show/reconfigure/unlock rules; destruction and visible
layout remain required before accepting a live initialized root.

The bounded owned-INI reader accepts unambiguous Root settings without guessing
defaults. Conflicting duplicate Root values are refused because retail duplicate
precedence is unproven; missing input stays unavailable. No quote/comment or
environment expansion is invented. Production deployment already transfers
the owned System directory, including DeusEx.ini; it is never bundled in Git/APK.
