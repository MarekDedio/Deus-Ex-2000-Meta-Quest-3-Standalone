# Original Window text translucency (2026-10-27)

Verified first-Training startup dependency, not completed startup or live VR
crate/loot/gravity acceptance. The accepted presentation HUD layout is unchanged.
Commercial DLLs/EXEs were read only as static data, never loaded or executed.

## Original contract and implementation

Extension.Window.EnableTranslucentText is native1483, flags0x401. Its sole
bEnable parameter is optional BoolProperty,dimension1,flags0x90. The wrapper
10053F10..10053F53 initializes INT1 before evaluating its argument. Omitted,
Nothing and NoWrite arguments therefore request true, not declaration zero.
Ordinary argument values are copied in the caller frame before native ingress.

Window1004DC90..1004DCFB compares its current canonical bTextTranslucent bit
with the argument. Equal canonical input returns without writes or inspecting
wGc, even if the drawing GC is inconsistent. On change, it stores the Window
flag then reads wGc. With a real context, GC10027610..1002763A always stores
bTextTranslucent followed by the complete textPolyFlags word:0x804 for true,
0x802 for false. A None context leaves the one Window store. The Window flag
is original BoolProperty flags2, wGc ObjectProperty flags0/Extension.GC, and
the two GC scalars are original Bool/Int flags0. Actual declaration, native
ownership and object identity guards remain in place. No general-GC update,
font/color/style callback, layout request or synthesized drawing service occurs.
All stores use the caller's existing typed journal and rollback boundary.

## Verification and actual later boundary

The real GC fixture now includes43 Window cases:the previous four context
acquisition controls plus four direct ingress guards,32 equality/None/context/
repeated-call combinations and three default-input forms. Direct precondition
guards are not misrepresented as independent failed VM roots. Existing cold/
warm root, early/late write/work failure, save/reload and complete object/GC/
counter/save undo controls pass. Totals remain155 top-level assertions/16 failed
root controls, now10680 actual writes/189903 work. VM696501 checks/920 refusal
controls pass, including source-frame snapshots and late input-write undo.

Owned HUD initialization now reaches HUDReceivedDisplay.CreateReceivedLabel
PC54/opcode102, native1550,14091 writes and193 actual native objects. Mission-
start shadow/foreground initialization completes, including zero margins and
actual foreground textPolyFlags0x804. HUDBarkDisplay also completes; the
received-item Tile/TextWindow exists but its SetText call has not run. Exact
parent/sibling/Outer/reference/visibility/layout and whole-root rollback checks
pass:9679 original-player controls.

The larger diagnostic graph initially exceeded the unchanged4MiB snapshot cap.
The conservative generic Value accountant charged short-string characters that
are stored inside already-counted std::string objects. Snapshot accounting now
subtracts only provably inline character capacity, adds NUL ownership for
external buffers, and preserves conservative map overhead/node/depth guards.
All fields remain captured; the complete193-object graph fits without raising
the snapshot/live limit or changing save formats. Initial unknown-native VM
fixture registration and the stale HUD boundary assertion were repaired; their
failed discovery logs are not passing evidence.

Full desktop build, current Android-native link and strict ARM64 pass. CTest:
269 registered,225 executed,44 explicit owned-data skips,zero failures138.06s.
Separate rebuilt first-Training crate415/5,deferred429/21,animation588/24,
FastTrace252/0,Input classes10413/38,viewport10310/20,materials and1232 route/
save controls pass. All45 originals/20 frozen compiler inputs match length,
UTC modification time and SHA256; all jobs are terminal.

Actual cold/warm two-Fire repeat2 reaches native1550 at254194 instructions/
30860 writes, retaining48 observations,two attack notifies,four traces,two
crate changes,nine births and complete undo. Door repeat2 retains its exact
unprepared ReadInput refusal at239536/19432, both56-unit moves/two endpoints
and complete undo. None publishes a completed player or live gameplay.

## Next service, not implemented here

Original HUDReceivedDisplay.CreateReceivedLabel calls TextWindow.SetText1550.
Actual declaration:flags0x401, NewText StrProperty dimension1/flags0x400880.
Wrapper10046A30 constructs an empty FString temporary, evaluates the input,
calls Window.ConvertScriptString10050710, then dispatches virtual+0x11C.
ConvertScriptString replaces lowercase |n with LF while preserving other bar
pairs/trailing bar, through original fixed-chunk/global FString storage; it is
not an arbitrary markup stripper. The base XTextWindow.SetText10045D50 compares
text case-insensitively (actual appStricmp import100590C8). Equal text causes
no store, accelerator work or layout. Changed text replaces private Text and,
when bTextIsAccelerator is set, calls SetAcceleratorText1004F310 before native
reconfiguration1004E060. Own static import evidence is retained in ignored
artifacts/textwindow-settext-static-imports-20261027.json. Native overrides,
accelerator effects and exact conversion/UTF16 ownership need their own audit
before admission; merely assigning a script string is not the full contract.

No new APK was packaged or installed. Remaining input/startup services and the
committing VR weapon/frame/publication bridge remain required; first Training
only, with no account/character-creation UI or other-map integration.
